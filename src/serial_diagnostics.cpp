#include "serial_diagnostics.h"
#include "diagnostic_helpers.h"
#include "debug.h"
#include "ha_source.h"
#include "wifi_service.h"
#include <WiFi.h>
#include <esp_system.h>
#include <sys/time.h>
#include <algorithm>
#include <climits>

namespace diagnostics {
namespace {
uint32_t lastTick=0,lastCycle=0,lastDemoLog=0,lastHealth=0,lastRevision=0;
int lastWifi=-1;
bool lastPortal=false,lastClock=false,seenMode=false;
source::SourceMode lastMode=source::SourceMode::Live;
unsigned lastValid=UINT_MAX,lastFresh=UINT_MAX;

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
void logConfig() {
    const auto& c=monitor::config();
    DBG_INFO("[CFG] HA=%s token=%s mode=%s poll=%us stale=%us solar_skew_limit=%us ca=%s",
             endpoint(c.url).c_str(),c.token.isEmpty()?"missing":"set",c.demo?"demo":"live",
             unsigned(c.pollSeconds),unsigned(c.staleSeconds),unsigned(energy::skewLimitSeconds(c.pollSeconds)),
             c.caCertificate.isEmpty()?"missing":"set");
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
    DBG_INFO("[BOOT] %s %s reset_reason=%d heap=%u min_heap=%u debug=%s",
             PROJECT_NAME,FIRMWARE_VERSION,int(esp_reset_reason()),ESP.getFreeHeap(),ESP.getMinFreeHeap(),debugLevelName(debugLevel));
    DBG_INFO("[BOARD] %s %s 320x240; TFT=HSPI touch=VSPI serial=115200",
             CYD_BOARD_NAME,CYD_DRIVER_NAME);
    logConfig();
}

void update(uint32_t now) {
    if(debugLevel==DBG_LEVEL_OFF || uint32_t(now-lastTick)<250) return;
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
    if(state.trace.id && state.trace.id!=lastCycle) {
        if(state.trace.mode==source::SourceMode::Live || modeChanged ||
           (state.trace.mode==source::SourceMode::Demo && now-lastDemoLog>=5000)) {
            logCycle(state,now,epoch);
            if(state.trace.mode==source::SourceMode::Demo) lastDemoLog=now;
        }
        lastCycle=state.trace.id;lastMode=state.trace.mode;seenMode=true;
    }
    if(now-lastHealth>=30000) {
        lastHealth=now;
        DBG_INFO("[HEALTH] heap=%u min_heap=%u rssi=%ddBm wifi=%d clock=%s cycle=%lu mode=%s valid=0x%02X fresh=0x%02X",
                 ESP.getFreeHeap(),ESP.getMinFreeHeap(),WiFi.isConnected()?WiFi.RSSI():0,wifi,
                 epoch?"synced":"unknown",static_cast<unsigned long>(state.trace.id),modeName(state.trace.mode),valid,fresh);
    }
}
}  // namespace diagnostics
