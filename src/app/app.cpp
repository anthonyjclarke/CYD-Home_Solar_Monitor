#include "app/app.h"
#include "flow_layout.h"
#include "history_layout.h"
#include "history_service.h"
#include "touch_gesture.h"
#include "display_service.h"
#include "touch_service.h"
#include "wifi_service.h"
#include "monitor_config.h"
#include "ha_source.h"
#include "backlight_service.h"
#include "monitor_web.h"
#include "serial_diagnostics.h"
#include <ctime>
#include <algorithm>

namespace app {
namespace {
TFT_eSprite buffer(&cyd::display::tft());
bool buffered = false, details = false, wasPressed = false, historyShown = false;
ui::TouchGesture gesture;
history::Field historyField = history::Solar;
uint16_t tapX = 0, tapY = 0;
uint32_t lastPaint = 0, lastMotion = 0, lastTouch = 0;
bool flowShown = false;  // The last full frame was the flow screen.
bool booting = true;
uint32_t bootStart = 0, connectedAt = 0;
bool calibrating = false, calibrationHeld = false;
bool waitCalibrationRelease = false;
unsigned target = 0;
uint16_t raw[4][2] = {};
const int targets[4][2] = {{20,20},{299,20},{299,219},{20,219}};

// The frame's position on the panel; non-zero only on panels larger than it.
int16_t ox() { return cyd::display::originX(); }
int16_t oy() { return cyd::display::originY(); }

unsigned rgb565(unsigned rgb) {
    return ((rgb>>8)&0xf800) | ((rgb>>5)&0x07e0) | ((rgb>>3)&0x001f);
}
class TftCanvas : public ui::Canvas {
public:
    void rect(int x,int y,int w,int h,unsigned color,int radius=0) override {
        if (radius) buffer.fillRoundRect(x,y,w,h,radius,rgb565(color));
        else buffer.fillRect(x,y,w,h,rgb565(color));
    }
    void line(int x,int y,int a,int b,unsigned color) override { buffer.drawLine(x,y,a,b,rgb565(color)); }
    void text(int x,int y,int font,unsigned color,ui::Align align,const char* s) override {
        buffer.setTextDatum(align == ui::Left ? TL_DATUM : align == ui::Right ? TR_DATUM : TC_DATUM);
        buffer.setTextColor(rgb565(color)); buffer.drawString(s,x,y,font);
    }
    int textWidth(int font,const char* s) override { return buffer.textWidth(s,font); }
} canvas;

// Drawn into the frame sprite and pushed whole, so the 1 Hz refresh does not flash.
void paintInfo() {
    auto& t = buffer;
    const auto& wifi = cyd::network::status();
    t.fillSprite(TFT_BLACK); t.setTextDatum(TL_DATUM); t.setTextColor(TFT_WHITE,TFT_BLACK);
    t.drawString(wifi.connected ? "DEVICE / SETUP" : "CONNECT TO WIFI",10,8,2);
    t.drawString(wifi.connected ? "http://" APP_MDNS_NAME ".local/" : wifi.apName,10,37,2);
    t.drawString(wifi.connected ? "or http://" + wifi.ip + "/   OTA code:" : "AP password / device code:",10,62,2);
    t.setTextColor(TFT_YELLOW,TFT_BLACK);
    t.drawString(monitor::accessCode(),10,84,4);
    t.setTextColor(TFT_WHITE,TFT_BLACK);
    t.drawString("HA: " + monitor::config().url.substring(0,35),10,120,2);
    t.drawString("Firmware " FIRMWARE_VERSION,10,143,2);
    t.drawString("Tap: return / hold 2s: calibrate",10,170,2);
    t.drawString(wifi.connected ? "Use browser for settings and updates" : "Portal: 192.168.4.1 (after 20s)",10,196,2);
    t.drawString("No Home Assistant token in logs",10,219,1);
    buffer.pushSprite(ox(),oy());
}

// Start-up screen: one row per step, repainted every 250 ms so Wi-Fi, IP and
// clock fill in as they come up. Theme follows the chosen panel layout.
void paintBoot(uint32_t now) {
    const auto& cfg = monitor::config();
    const auto& status = diagnostics::bootStatus();
    ui::BootView v;
    v.layout = static_cast<ui::Layout>(cfg.layout);
    v.version = "v" FIRMWARE_VERSION;
    for (unsigned i=0;i<boot::RowCount;++i)
        v.rows[i] = {boot::label(i),status.rows[i].value,status.rows[i].state};
    char footer[48];
    if (connectedAt) {
        const uint32_t left = APP_BOOT_HOLD_MS - std::min<uint32_t>(APP_BOOT_HOLD_MS, now - connectedAt);
        snprintf(footer, sizeof(footer), "Starting display in %u s - tap to skip", unsigned((left + 999) / 1000));
        v.progress = float(now - connectedAt) / APP_BOOT_HOLD_MS;
    } else {
        snprintf(footer, sizeof(footer), "Waiting for Wi-Fi - tap to skip");
        v.progress = float(now - bootStart) / APP_BOOT_MAX_MS;
    }
    v.footer = footer;
    ui::drawBoot(canvas, v);
    buffer.pushSprite(ox(),oy());
}

void paintCalibration() {
    auto& t = cyd::display::tft();
    t.fillScreen(TFT_BLACK); t.setTextDatum(TC_DATUM); t.setTextColor(TFT_WHITE,TFT_BLACK);
    t.drawString("Tap each cross once",ox()+160,oy()+95,2);
    t.drawString("Timeout: 45 seconds",ox()+160,oy()+120,2);
    int x=ox()+targets[target][0], y=oy()+targets[target][1];
    t.drawLine(x-8,y,x+8,y,TFT_YELLOW); t.drawLine(x,y-8,x,y+8,TFT_YELLOW);
}
void sampleCalibration(const cyd::touch::State& touch, uint32_t now) {
    if (now-lastTouch > 45000) { calibrating=false; details=true; return; }
    if (waitCalibrationRelease) {
        if (!touch.pressed) waitCalibrationRelease=false;
        return;
    }
    if (touch.pressed && !calibrationHeld) {
        raw[target][0]=touch.rawX; raw[target][1]=touch.rawY; calibrationHeld=true;
    }
    if (!touch.pressed && calibrationHeld) {
        calibrationHeld=false; ++target;
        if (target == 4) {
            // Targets are inset 20 pixels; extrapolate to panel edges.
            int x0=(raw[0][0]+raw[3][0])/2, x1=(raw[1][0]+raw[2][0])/2;
            int y0=(raw[0][1]+raw[1][1])/2, y1=(raw[2][1]+raw[3][1])/2;
            int dx=(x1-x0)*20/279, dy=(y1-y0)*20/199;
            bool ok=cyd::touch::calibrate(constrain(x0-dx,0,4095),constrain(x1+dx,0,4095),
                                         constrain(y0-dy,0,4095),constrain(y1+dy,0,4095));
            calibrating=false; details=true;
            auto& t=cyd::display::tft(); t.fillScreen(TFT_BLACK);
            t.drawString(ok ? "Calibration saved" : "Calibration rejected",ox()+160,oy()+100,2);
        } else paintCalibration();
        lastTouch=now;
    }
}
}

void begin() {
    buffer.setColorDepth(8);
    buffered = buffer.createSprite(320,240) != nullptr;
    diagnostics::bootStage("Display",buffered?"frame buffer ready":"frame buffer allocation failed",!buffered);
    if (!buffered) {
        cyd::display::showBootMessage("Low memory", "Restart device to retry display");
        diagnostics::bootScreenClosed("display buffer unavailable");
    }
    bootStart = millis();
    if (buffered) paintBoot(bootStart);
    diagnostics::bootStage("History","mounting LittleFS and checking checkpoints");
    history::begin();
    diagnostics::bootStage("Home Assistant","starting background reader");
    source::begin();
    const bool failed=source::readerState()==source::ReaderState::Failed;
    diagnostics::bootStage("Home Assistant",failed?"reader task failed":"reader task started",failed);
}

void update(uint32_t now) {
    const auto& touch = cyd::touch::state();
    const auto& cfg = monitor::config();
    if (calibrating) {
        gesture.update(touch.pressed,now,true);
        sampleCalibration(touch,now); wasPressed=touch.pressed; return;
    }
    tm local{}; time_t epoch=time(nullptr); bool clock=epoch>1700000000;
    if (clock) localtime_r(&epoch,&local);
    const auto phase=night::phase(monitor::nightSchedule(cfg),clock,local.tm_hour*60+local.tm_min);
    const bool dimmed=night::brightness(phase,cfg.brightness,now,lastTouch)<cfg.brightness;
    if (touch.pressed && !wasPressed) { tapX=touch.x; tapY=touch.y; lastTouch=now; }
    const auto event=gesture.update(touch.pressed,now,dimmed);
    if (touch.pressed) lastTouch=now;
    if (event==ui::Gesture::Hold) {
        if (details) {
            // A separate hold in Settings retains the existing calibration action.
            calibrating=true; target=0; calibrationHeld=false; waitCalibrationRelease=true;
            paintCalibration(); wasPressed=true; return;
        }
        if (booting) diagnostics::bootScreenClosed("Settings opened by hold");
        details=true; historyShown=false; booting=false; lastPaint=0;
    } else if (event==ui::Gesture::Tap) {
        if (booting) { booting=false; diagnostics::bootScreenClosed("skipped by touch"); }
        else if (details) details=false;
        else if (historyShown) {
            if (tapY<29 && tapX>=260) historyShown=false;
            else if (tapY>=31 && tapY<58) historyField=history::tabs[std::min<unsigned>(4,tapX/64)];
            else if (tapY>=209 && (tapX<52 || tapX>=268)) {
                unsigned i=0;
                while (i+1<history::FieldCount && history::order[i]!=historyField) ++i;
                historyField=history::order[(i+history::FieldCount+(tapX<52?-1:1))%history::FieldCount];
            }
        } else if (tapY>=209 && tapX>=232) historyShown=true;
        lastPaint=0;
    }
    wasPressed=touch.pressed;
    const uint8_t brightness=night::brightness(phase,cfg.brightness,now,lastTouch);
    cyd::backlight::setBrightness(brightness);
    // Backlight off leaves touch, sampling, history, networking and OTA active.
    // A scheduled turn-on or wake touch repaints a complete frame immediately.
    if (!brightness) { flowShown=false; lastPaint=0; return; }
    if (web::updating() || !buffered) return;
    // Full frames repaint everything; between them, motion frames redraw and
    // push only the pipes, once the flow screen is showing.
    const bool full = now-lastPaint >= APP_FULL_FRAME_MS;
    if (!full && (cfg.layout != ui::Centre || !flowShown || now-lastMotion < APP_FLOW_FRAME_MS)) return;
    if (full) { lastPaint=now; flowShown=false; }
    lastMotion=now;
    if (booting) {
        if (!connectedAt && cyd::network::status().connected) connectedAt = now;
        const bool done = connectedAt ? now - connectedAt >= APP_BOOT_HOLD_MS : now - bootStart >= APP_BOOT_MAX_MS;
        if (!done) { paintBoot(now); return; }
        booting = false;
        diagnostics::bootScreenClosed(connectedAt?"Wi-Fi hold complete":"offline timeout");
    }
    if (historyShown) {
        ui::HistoryView h{history::data()};
        h.epoch=clock?uint32_t(epoch):0; h.field=historyField; h.layout=static_cast<ui::Layout>(cfg.layout);
        h.storageOK=history::storageHealthy(); h.demo=cfg.demo;
        ui::drawHistory(canvas,h);
        if (gesture.heldMs(now)>300) {
            canvas.rect(0,237,std::min<uint32_t>(320,gesture.heldMs(now)*320/2000),3,0x9292aa);
        }
        buffer.pushSprite(ox(),oy()); return;
    }
    if (details) {
        // Repaint at 1 Hz to avoid flashing the setup page.
        static uint32_t infoPaint=0;
        if (now-infoPaint>=1000 || !infoPaint) { paintInfo(); infoPaint=now; }
        return;
    }
    ui::FlowView view;
    view.data=source::snapshot(); view.wifi=cyd::network::status().connected;
    // Copy first, then evaluate receive ages, as history does: a publication
    // newer than `now` would otherwise wrap to a huge unsigned age.
    const uint32_t ageNow=millis();
    view.clockKnown=clock; view.phase=now/250; view.ms=now; view.motionOnly=!full;
    bool all=true, reportKnown=clock;
    for (int i=0;i<energy::Count;++i) {
        view.usable[i]=energy::metricUsable(view.data,static_cast<energy::Metric>(i),view.wifi,ageNow,clock ? uint64_t(epoch)*1000 : 0,cfg.staleSeconds,cfg.pollSeconds);
    }
    for (auto metric:energy::displayMetrics) {
        all &= view.usable[metric]; reportKnown &= view.data.readings[metric].reportedMs!=0;
    }
    const char* status=view.data.demo ? "DEMO / hold for setup" : !view.wifi ? "WiFi disconnected" : !all ? "Waiting / stale or unavailable" : !reportKnown ? "HA connected / source age unknown" : "Live / hold for settings";
    view.status=status; view.reportKnown=reportKnown;
    String ip=cyd::network::status().ip;
    view.address=ip.c_str();
    view.batteryLabel=cfg.batteryLabel.c_str();
    view.layout=static_cast<ui::Layout>(cfg.layout);
    for (int t=0;t<energy::TotalCount;++t)
        view.totalLive[t]=energy::totalUsable(view.data,static_cast<energy::Total>(t),view.wifi,ageNow);
    view.weatherLive=(view.data.demo || view.wifi) && weather::usable(view.data.weather,ageNow);
    char clockText[8]="";
    if (clock) strftime(clockText,sizeof(clockText),"%H:%M",&local);
    view.time=clockText;
    const ui::Box box=ui::drawFlow(canvas,view);
    if (full && gesture.heldMs(now)>300)
        canvas.rect(0,237,std::min<uint32_t>(320,gesture.heldMs(now)*320/2000),3,0x9292aa);
    if (full) { buffer.pushSprite(ox(),oy()); flowShown=true; }
    else if (box.w > 0 && box.h > 0) buffer.pushSprite(ox()+box.x,oy()+box.y,box.x,box.y,box.w,box.h);
}
}  // namespace app

