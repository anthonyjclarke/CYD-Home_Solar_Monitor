#include "serial_diagnostics.h"
#include "diagnostic_helpers.h"
#include "debug.h"
#include "ha_source.h"
#include "wifi_service.h"
#include "history_service.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <sys/time.h>
#include <ctime>
#include <algorithm>
#include <climits>

namespace diagnostics {
namespace {
uint32_t lastTick=0,lastCycle=0,lastHealth=0,lastRevision=0;
CycleLogGate cycleLogs;
int lastWifi=-1;
bool lastPortal=false,lastClock=false,seenMode=false;
source::SourceMode lastMode=source::SourceMode::Live;
unsigned lastValid=UINT_MAX,lastFresh=UINT_MAX;
boot::Report startup;
boot::Tracker startupTracker;
uint32_t lastStartupUpdate=0;
bool startupFinished=false, startupScreenClosed=false, startupHardwareProblem=false;

const char* resetReason(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:return "power-on";
        case ESP_RST_EXT:return "external";
        case ESP_RST_SW:return "software restart";
        case ESP_RST_PANIC:return "panic/exception";
        case ESP_RST_INT_WDT:return "interrupt watchdog";
        case ESP_RST_TASK_WDT:return "task watchdog";
        case ESP_RST_WDT:return "watchdog";
        case ESP_RST_DEEPSLEEP:return "deep-sleep wake";
        case ESP_RST_BROWNOUT:return "brownout";
        case ESP_RST_SDIO:return "SDIO reset";
        default:return "unknown";
    }
}

const char* modeName(source::SourceMode mode) {
    return mode==source::SourceMode::Demo ? "demo" : mode==source::SourceMode::TokenMissing ? "token-missing" : "live";
}
const char* readError(source::ReadError error) {
    switch(error) {
        case source::ReadError::None:return "none";
        case source::ReadError::NotRead:return "not-read";
        case source::ReadError::Wifi:return "wifi-offline";
        case source::ReadError::MissingCa:return "missing-ca";
        case source::ReadError::Clock:return "waiting-for-clock";
        case source::ReadError::Url:return "invalid-url";
        case source::ReadError::Http:return "http-or-transport";
        case source::ReadError::TooLarge:return "response-too-large";
        case source::ReadError::Json:return "invalid-json";
        case source::ReadError::Value:return "invalid-state-or-unit";
    }
    return "unknown";
}

String endpoint(const String& url) {
    // Log only a validated origin. Userinfo, path, query and fragments never
    // appear in the serial output, even for an old malformed saved URL.
    int start=url.indexOf("://");
    if(start<0 || !(url.startsWith("http://") || url.startsWith("https://"))) return "<invalid>";
    start+=3;
    int end=start;
    while(end<int(url.length()) && url[end]!='/' && url[end]!='?' && url[end]!='#') ++end;
    if(end==start) return "<invalid>";
    for(int i=start;i<end;++i) {
        const char ch=url[i];
        if(!(isAlphaNumeric(ch) || ch=='.' || ch==':' || ch=='-' || ch=='[' || ch==']')) return "<redacted>";
    }
    return url.substring(0,end);
}

void updateStartup(uint32_t now, bool force=false) {
    if ((startupFinished && startupScreenClosed) || (!force && uint32_t(now-lastStartupUpdate)<250)) return;
    lastStartupUpdate=now;
    const auto& cfg=monitor::config();
    const auto& wifi=cyd::network::status();
    const auto state=source::diagnosticSnapshot();
    now=millis();
    const time_t epoch=time(nullptr);
    boot::Inputs in;
    in.connected=wifi.connected; in.portal=wifi.portalActive;
    in.ssid=wifi.ssid.c_str(); in.ip=wifi.ip.c_str(); in.web="http://" APP_MDNS_NAME ".local/";
    in.rssi=wifi.connected?WiFi.RSSI():0;
    in.clock=epoch>history::clockMinimum;
    char localTime[24]=""; tm local{};
    if (in.clock && localtime_r(&epoch,&local)) strftime(localTime,sizeof(localTime),"%H:%M %Z",&local);
    in.localTime=localTime;
    in.demo=cfg.demo; in.demoReady=state.data.demo; in.token=!cfg.token.isEmpty(); in.attempted=state.trace.id!=0;
    in.readerFailed=source::readerState()==source::ReaderState::Failed;
    in.sourceTimeKnown=in.clock;
    for (unsigned i=0;i<energy::SourceCount;++i) {
        in.sources+=energy::metricUsable(state.data,static_cast<energy::Metric>(i),wifi.connected,now,
                                       in.clock?uint64_t(epoch)*1000:0,cfg.staleSeconds,cfg.pollSeconds);
        in.sourceTimeKnown &= state.data.readings[i].reportedMs!=0;
    }
    in.solar=energy::metricUsable(state.data,energy::Solar,wifi.connected,now,
                                 in.clock?uint64_t(epoch)*1000:0,cfg.staleSeconds,cfg.pollSeconds);
    in.restore=history::restoreState(); in.historyHealthy=history::storageHealthy();
    in.historySamples=boot::restoredSampleCount(history::data(),history::savedAt(),in.clock?uint32_t(epoch):0);
    startup=boot::build(in); startup.warnings |= startupHardwareProblem;
    const auto changes=startupTracker.update(startup,now);
    for (unsigned i=0;i<boot::RowCount;++i) if (changes.rows&(1U<<i)) {
        if (startup.rows[i].state==boot::Problem) DBG_WARN("[BOOT] %s: %s",boot::label(i),startup.rows[i].value);
        else DBG_INFO("[BOOT] %s: %s",boot::label(i),startup.rows[i].value);
    }
    if (changes.finished) {
        startupFinished=true;
        if (changes.timedOut) DBG_WARN("[BOOT] Startup diagnostic window ended; services still pending (Wi-Fi=%u clock=%u HA=%u/6 solar=%u)",
                                      unsigned(in.connected),unsigned(in.clock),in.sources,unsigned(in.solar));
        else if (startup.warnings) DBG_WARN("[BOOT] Startup ready with warnings; check the status rows above");
        else DBG_INFO("[BOOT] Startup ready: Wi-Fi, clock, source and history checks complete");
    }
}
void logConfig() {
    const auto& c=monitor::config();
    DBG_INFO("[CFG] HA=%s token=%s mode=%s poll=%us stale=%us solar_skew_limit=%us ca=%s debug=%s",
             endpoint(c.url).c_str(),c.token.isEmpty()?"missing":"set",c.demo?"demo":"live",
             unsigned(c.pollSeconds),unsigned(c.staleSeconds),unsigned(energy::skewLimitSeconds(c.pollSeconds)),
             c.caCertificate.isEmpty()?"missing":"set",debugLevelName(c.debugLevel));
    for(int i=0;i<energy::SourceCount;++i)
        DBG_INFO("[MAP] %s=%s",metricName(static_cast<energy::Metric>(i)),c.entities[i].c_str());
    DBG_INFO("[MAP] today_kwh=%s forecast_kwh=%s",c.totalEntities[energy::Today].isEmpty()?"<none>":c.totalEntities[energy::Today].c_str(),
             c.totalEntities[energy::Tomorrow].isEmpty()?"<none>":c.totalEntities[energy::Tomorrow].c_str());
    if(std::isnan(c.latitude)) DBG_INFO("[WEATHER] off (no location) tz=%s",c.tzPosix.c_str());
    else DBG_INFO("[WEATHER] place=%s lat=%.4f lon=%.4f tz=%s",c.place.c_str(),c.latitude,c.longitude,c.tzPosix.c_str());
    DBG_INFO("[SIGNS] normalized grid=+import/-export; battery=+discharge/-charge; solar=DC-battery");
}

void logCycle(const source::DiagnosticSnapshot& state,uint32_t now,uint64_t epoch) {
    const auto& c=monitor::config();const auto& s=state.data;const auto& trace=state.trace;
    const uint32_t limit=energy::skewLimitSeconds(c.pollSeconds);
    unsigned valid=0;
    for(int i=0;i<energy::SourceCount;++i) valid+=s.readings[i].valid;
    DBG_INFO("[POLL] cycle=%lu rev=%lu mode=%s duration=%lums next=%lums valid=%u/%u epoch_ms=%llu heartbeat_age=%lldms",
             static_cast<unsigned long>(trace.id),static_cast<unsigned long>(s.revision),modeName(trace.mode),
             static_cast<unsigned long>(trace.elapsedMs),static_cast<unsigned long>(trace.nextPollMs),
             valid,unsigned(energy::SourceCount),static_cast<unsigned long long>(epoch),
             epoch && s.heartbeatMs ? static_cast<long long>(epoch)-static_cast<long long>(s.heartbeatMs) : -1LL);
    if(trace.mode==source::SourceMode::TokenMissing) {
        DBG_WARN("[HA] No token configured; use the device's local settings page");return;
    }
    for(int i=0;i<energy::SourceCount;++i) {
        const auto metric=static_cast<energy::Metric>(i);
        const auto& r=s.readings[i];const auto& req=trace.requests[i];
        char raw[40],received[24],reported[32],line[440];
        if(req.numeric) snprintf(raw,sizeof(raw),"%.3f%s",req.rawValue,req.unit);
        else snprintf(raw,sizeof(raw),"%s/%s",req.state,req.unit);
        if(r.valid || r.receivedMs) snprintf(received,sizeof(received),"%lums",static_cast<unsigned long>(now-r.receivedMs));
        else snprintf(received,sizeof(received),"unknown");
        if(epoch && r.reportedMs) snprintf(reported,sizeof(reported),"%lldms",static_cast<long long>(epoch)-static_cast<long long>(r.reportedMs));
        else snprintf(reported,sizeof(reported),"unknown");
        const bool fresh=energy::metricFresh(s,metric,now,epoch,c.staleSeconds,limit);
        snprintf(line,sizeof(line),"[READ] %s entity=%s http=%d ms=%lu raw=%s normalized=%+.3f%s valid=%u fresh=%u received_age=%s report_age=%s report_ms=%llu error=%s reason=%s",
                 metricName(metric),c.entities[i].c_str(),req.httpStatus,static_cast<unsigned long>(req.elapsedMs),raw,
                 r.value,metric==energy::Soc?"%":"W",unsigned(r.valid),unsigned(fresh),received,reported,
                 static_cast<unsigned long long>(r.reportedMs),readError(req.error),snapshotReason(s,metric,now,epoch,c.staleSeconds));
        if(trace.mode==source::SourceMode::Live && req.error!=source::ReadError::None) DBG_WARN("%s",line);
        else DBG_INFO("%s",line);
    }
    const auto& dc=s.readings[energy::InverterDc];const auto& battery=s.readings[energy::Battery];
    const auto& solar=s.readings[energy::Solar];
    const uint64_t reportSkew=dc.reportedMs>battery.reportedMs?dc.reportedMs-battery.reportedMs:battery.reportedMs-dc.reportedMs;
    const uint32_t dcAge=now-dc.receivedMs,batteryAge=now-battery.receivedMs;
    const uint32_t receiveSkew=dcAge>batteryAge?dcAge-batteryAge:batteryAge-dcAge;
    DBG_INFO("[SOLAR] formula=max(DC+raw_charge,0) DC=%.3fW raw_charge=%+.3fW stored_solar=%.3fW valid=%u fresh=%u coherent=%u receive_skew=%lums raw_report_skew=%llums report_pair_known=%u limit=%lus reason=%s",
             dc.value,-battery.value,solar.value,unsigned(solar.valid),
             unsigned(energy::metricFresh(s,energy::Solar,now,epoch,c.staleSeconds,limit)),
             unsigned(energy::solarSourcesCoherent(s,now,limit)),static_cast<unsigned long>(receiveSkew),
             static_cast<unsigned long long>(dc.reportedMs && battery.reportedMs?reportSkew:0),
             unsigned(dc.reportedMs && battery.reportedMs),static_cast<unsigned long>(limit),solarReason(s,now,epoch,c.staleSeconds,limit));
    if(energy::metricFresh(s,energy::Inverter,now,epoch,c.staleSeconds,limit) &&
       energy::metricFresh(s,energy::Home,now,epoch,c.staleSeconds,limit) &&
       energy::metricFresh(s,energy::Grid,now,epoch,c.staleSeconds,limit)) {
        const float expected=s.readings[energy::Inverter].value+s.readings[energy::Grid].value;
        DBG_INFO("[BALANCE] AC %.3fW + grid_import %+.3fW = %.3fW; home=%.3fW delta=%+.3fW source_skew=%lus",
                 s.readings[energy::Inverter].value,s.readings[energy::Grid].value,expected,
                 s.readings[energy::Home].value,s.readings[energy::Home].value-expected,static_cast<unsigned long>(s.skewSeconds));
    } else DBG_INFO("[BALANCE] Not checked: AC/home/grid input invalid or stale");
    if(trace.totalsRead) {
        const char* names[]={"today","forecast"};
        for(int t=0;t<energy::TotalCount;++t) {
            if(c.totalEntities[t].isEmpty()) continue;
            const auto& r=s.totals[t];const auto& req=trace.totals[t];
            if(req.error!=source::ReadError::None)
                DBG_WARN("[TOTAL] %s entity=%s http=%d ms=%lu state=%s unit=%s error=%s",names[t],c.totalEntities[t].c_str(),
                         req.httpStatus,static_cast<unsigned long>(req.elapsedMs),req.state,req.unit,readError(req.error));
            else DBG_INFO("[TOTAL] %s entity=%s http=%d ms=%lu value=%.3fkWh",names[t],c.totalEntities[t].c_str(),
                          req.httpStatus,static_cast<unsigned long>(req.elapsedMs),r.value);
        }
    }
    // Clock/Wi-Fi waits are retried each cycle and would only add noise.
    if(trace.weatherRead && trace.weather.error!=source::ReadError::Clock && trace.weather.error!=source::ReadError::Wifi) {
        const auto& w=s.weather;const auto& req=trace.weather;
        if(req.error==source::ReadError::None)
            DBG_INFO("[WEATHER] place=%s http=%d ms=%lu now=%.1fC code=%d tomorrow_max=%.1fC code=%d",c.place.c_str(),req.httpStatus,
                     static_cast<unsigned long>(req.elapsedMs),w.nowC,w.nowCode,w.tomorrowMaxC,w.tomorrowCode);
        else DBG_WARN("[WEATHER] place=%s http=%d ms=%lu error=%s; retry in %us",c.place.c_str(),req.httpStatus,
                      static_cast<unsigned long>(req.elapsedMs),readError(req.error),unsigned(weather::retrySeconds));
    }
    DBG_INFO("[SOURCE] %s",s.message);
}
}  // namespace

void begin() {
    startupTracker.begin(millis());
    const auto reset=esp_reset_reason();
    DBG_INFO("[BOOT] %s %s reset_reason=%d (%s) heap=%u min_heap=%u debug=%s",
             PROJECT_NAME,FIRMWARE_VERSION,int(reset),resetReason(reset),ESP.getFreeHeap(),ESP.getMinFreeHeap(),debugLevelName(debugLevel));
#if defined(TOUCH_CS)
    constexpr const char* touchBus="shared-HSPI";
#else
    constexpr const char* touchBus="VSPI";
#endif
    DBG_INFO("[BOARD] %s (%s) %s %ux%u panel, %ux%u UI; TFT=HSPI touch=%s serial=115200",
             CYD_BOARD_NAME,CYD_BOARD_KEY,CYD_DRIVER_NAME,unsigned(CYD_NATIVE_HEIGHT),unsigned(CYD_NATIVE_WIDTH),
             unsigned(CYD_EXPECTED_WIDTH),unsigned(CYD_EXPECTED_HEIGHT),touchBus);
    const auto* partition=esp_ota_get_running_partition();
    DBG_INFO("[BOOT] App slot=%s flash=%u bytes",partition?partition->label:"unknown",ESP.getFlashChipSize());
    logConfig();
    updateStartup(millis(),true);
}

void bootStage(const char* stage, const char* detail, bool problem) {
    startupHardwareProblem |= problem;
    if (problem) DBG_ERROR("[BOOT] %s: %s",stage,detail);
    else DBG_INFO("[BOOT] %s: %s",stage,detail);
}
const boot::Report& bootStatus() { return startup; }
void bootScreenClosed(const char* reason) {
    if (!startupScreenClosed) DBG_INFO("[BOOT] CYD startup screen closed: %s",reason);
    startupScreenClosed=true;
}
void update(uint32_t now) {
    // Boot status still drives the TFT with serial logging disabled, and has
    // its own bounded cadence rather than waiting for minute runtime reports.
    updateStartup(now);
    const uint8_t level=debugLevel.load();
    if(level==DBG_LEVEL_OFF) { cycleLogs.ready(now,0,level); return; }
    if(uint32_t(now-lastTick)<250) return;
    lastTick=now;
    timeval tv{};gettimeofday(&tv,nullptr);
    const uint64_t epoch=tv.tv_sec>1700000000?uint64_t(tv.tv_sec)*1000+tv.tv_usec/1000:0;
    const int wifi=int(WiFi.status());
    if(wifi!=lastWifi) {
        if(WiFi.isConnected()) DBG_INFO("[WIFI] Connected ip=%s gateway=%s rssi=%ddBm",WiFi.localIP().toString().c_str(),WiFi.gatewayIP().toString().c_str(),WiFi.RSSI());
        else DBG_WARN("[WIFI] Not connected status=%d; reconnect/provisioning remains active",wifi);
        lastWifi=wifi;
    }
    const bool portal=cyd::network::status().portalActive;
    if(portal!=lastPortal) { DBG_INFO("[WIFI] Setup portal %s",portal?"opened (code on panel)":"closed");lastPortal=portal; }
    if(bool(epoch)!=lastClock) { DBG_INFO("[CLOCK] %s epoch_ms=%llu",epoch?"synchronised":"not synchronised",static_cast<unsigned long long>(epoch));lastClock=bool(epoch); }
    const auto state=source::diagnosticSnapshot();
    // The worker may publish while copying. Evaluate receive ages only after
    // the copy so a newly received reading cannot appear to wrap millis().
    now=millis();
    const auto& c=monitor::config();
    if(state.data.revision && state.data.revision!=lastRevision) {
        if(lastRevision) logConfig();
        lastRevision=state.data.revision;
    }
    unsigned valid=0,fresh=0;
    for(int i=0;i<energy::Count;++i) {
        if(state.data.readings[i].valid) valid|=1U<<i;
        if(energy::metricUsable(state.data,static_cast<energy::Metric>(i),WiFi.isConnected(),now,epoch,c.staleSeconds,c.pollSeconds)) fresh|=1U<<i;
    }
    if(valid!=lastValid || fresh!=lastFresh) {
        DBG_INFO("[DATA] valid_mask=0x%02X fresh_mask=0x%02X (AC,home,grid,battery,SOC,DC,solar)",valid,fresh);
        lastValid=valid;lastFresh=fresh;
    }
    const bool modeChanged=!seenMode || state.trace.mode!=lastMode;
    const bool weatherAttempt=state.trace.weatherRead && state.trace.weather.error!=source::ReadError::Clock &&
                              state.trace.weather.error!=source::ReadError::Wifi;
    if(cycleLogs.ready(now,state.trace.id,level,modeChanged || state.trace.totalsRead || weatherAttempt))
        logCycle(state,now,epoch);
    if(state.trace.id) { lastCycle=state.trace.id;lastMode=state.trace.mode;seenMode=true; }
    if(uint32_t(now-lastHealth)>=serialReportIntervalMs) {
        lastHealth=now;
        DBG_INFO("[HEALTH] heap=%u min_heap=%u rssi=%ddBm wifi=%d clock=%s cycle=%lu mode=%s valid=0x%02X fresh=0x%02X",
                 ESP.getFreeHeap(),ESP.getMinFreeHeap(),WiFi.isConnected()?WiFi.RSSI():0,wifi,
                 epoch?"synced":"unknown",static_cast<unsigned long>(state.trace.id),modeName(state.trace.mode),valid,fresh);
    }
}
}  // namespace diagnostics
