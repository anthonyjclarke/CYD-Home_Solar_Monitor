#include "app/app.h"
#include "flow_layout.h"
#include "display_service.h"
#include "touch_service.h"
#include "wifi_service.h"
#include "monitor_config.h"
#include "ha_source.h"
#include "backlight_service.h"
#include "monitor_web.h"
#include <ctime>
#include <algorithm>

namespace app {
namespace {
TFT_eSprite buffer(&cyd::display::tft());
bool buffered = false, details = false, wasPressed = false, swallowTap = false;
uint32_t lastPaint = 0, lastTouch = 0, pressedAt = 0;
bool calibrating = false, calibrationHeld = false;
bool waitCalibrationRelease = false;
unsigned target = 0;
uint16_t raw[4][2] = {};
const int targets[4][2] = {{20,20},{299,20},{299,219},{20,219}};

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
} canvas;

// Drawn into the frame sprite and pushed whole, so the 1 Hz refresh does not flash.
void paintInfo() {
    auto& t = buffer;
    const auto& wifi = cyd::network::status();
    t.fillSprite(TFT_BLACK); t.setTextDatum(TL_DATUM); t.setTextColor(TFT_WHITE,TFT_BLACK);
    t.drawString(wifi.connected ? "DEVICE / SETUP" : "CONNECT TO WIFI",10,8,2);
    t.drawString(wifi.connected ? "http://" + wifi.ip + "/" : wifi.apName,10,37,2);
    t.drawString(wifi.connected ? "Browser user: admin" : "AP password / device code:",10,62,2);
    t.setTextColor(TFT_YELLOW,TFT_BLACK);
    t.drawString(monitor::accessCode(),10,84,4);
    t.setTextColor(TFT_WHITE,TFT_BLACK);
    t.drawString("HA: " + monitor::config().url.substring(0,35),10,120,2);
    t.drawString("Firmware " FIRMWARE_VERSION,10,143,2);
    t.drawString("Tap: return / hold 2s: calibrate",10,170,2);
    t.drawString(wifi.connected ? "Use browser for settings and updates" : "Portal: 192.168.4.1 (after 20s)",10,196,2);
    t.drawString("No Home Assistant token in logs",10,219,1);
    buffer.pushSprite(0,0);
}

void paintCalibration() {
    auto& t = cyd::display::tft();
    t.fillScreen(TFT_BLACK); t.setTextDatum(TC_DATUM); t.setTextColor(TFT_WHITE,TFT_BLACK);
    t.drawString("Tap each cross once",160,95,2);
    t.drawString("Timeout: 45 seconds",160,120,2);
    int x=targets[target][0], y=targets[target][1];
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
            t.drawString(ok ? "Calibration saved" : "Calibration rejected",160,100,2);
        } else paintCalibration();
        lastTouch=now;
    }
}
}

void begin() {
    buffer.setColorDepth(8);
    buffered = buffer.createSprite(320,240) != nullptr;
    if (!buffered) cyd::display::showBootMessage("Low memory", "Restart device to retry display");
    source::begin();
}

void update(uint32_t now) {
    const auto& touch = cyd::touch::state();
    const auto& cfg = monitor::config();
    if (calibrating) { sampleCalibration(touch,now); wasPressed=touch.pressed; return; }
    tm local{}; time_t epoch=time(nullptr); bool clock=epoch>1700000000;
    if (clock) localtime_r(&epoch,&local);
    bool night=false;
    if (clock && cfg.nightDim && cfg.nightStart != cfg.nightEnd)
        night=cfg.nightStart<cfg.nightEnd ? local.tm_hour>=cfg.nightStart && local.tm_hour<cfg.nightEnd : local.tm_hour>=cfg.nightStart || local.tm_hour<cfg.nightEnd;
    const bool dimmed = night && now-lastTouch>60000;
    if (touch.pressed && !wasPressed) {
        // A touch on a dimmed panel only wakes it; it does not also open setup.
        swallowTap = dimmed; pressedAt=now; lastTouch=now;
    }
    if (touch.pressed && details && !swallowTap && now-pressedAt >= 2000) {
        calibrating=true; target=0; calibrationHeld=true; lastTouch=now; paintCalibration();
        // Discard the hold that entered calibration; wait for its release.
        calibrationHeld=false; waitCalibrationRelease=true;
        wasPressed=true; return;
    }
    if (!touch.pressed && wasPressed && !swallowTap && now-pressedAt >= 50 && now-pressedAt < 1500) {
        details=!details; lastPaint=0; lastTouch=now;
    }
    wasPressed=touch.pressed;
    cyd::backlight::setBrightness(night && now-lastTouch>60000 ? std::max(5,int(cfg.brightness)/5) : cfg.brightness);
    if (web::updating() || !buffered || now-lastPaint < 250) return;
    lastPaint=now;
    if (details || (!cyd::network::status().connected && !cfg.demo)) {
        // Repaint at 1 Hz to avoid flashing the setup page.
        static uint32_t infoPaint=0;
        if (now-infoPaint>=1000 || !infoPaint) { paintInfo(); infoPaint=now; }
        return;
    }
    ui::FlowView view;
    view.data=source::snapshot(); view.wifi=cyd::network::status().connected;
    view.clockKnown=clock; view.phase=now/250;
    bool all=true, reportKnown=clock;
    for (int i=0;i<energy::Count;++i) {
        view.usable[i]=energy::metricUsable(view.data,static_cast<energy::Metric>(i),view.wifi,now,clock ? uint64_t(epoch)*1000 : 0,cfg.staleSeconds,cfg.pollSeconds);
    }
    for (auto metric:energy::displayMetrics) {
        all &= view.usable[metric]; reportKnown &= view.data.readings[metric].reportedMs!=0;
    }
    const char* status=view.data.demo ? "DEMO / tap for setup" : !view.wifi ? "WiFi disconnected" : !all ? "Waiting / stale or unavailable" : view.data.skewSeconds>energy::skewLimitSeconds(cfg.pollSeconds) ? "Source times differ" : !reportKnown ? "HA connected / source age unknown" : "Live / tap for device settings";
    view.status=status;
    String ip=cyd::network::status().ip;
    view.address=ip.c_str();
    view.batteryLabel=cfg.batteryLabel.c_str();
    ui::drawFlow(canvas,view); buffer.pushSprite(0,0);
}
}  // namespace app

