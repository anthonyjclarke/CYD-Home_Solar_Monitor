#include "flow_layout.h"
#include <cstdio>
#include <cmath>

namespace ui {
namespace {
// The panel renders through an 8-bit (RGB332) sprite. Every colour must be an
// exact RGB332 level or it is quantised on hardware: R/G steps of 255/7, B of 85.
constexpr bool rgb332Level(unsigned v, unsigned steps) { return v == ((v * steps + 127) / 255 * 255 + steps / 2) / steps; }
constexpr bool rgb332(unsigned c) { return rgb332Level(c >> 16, 7) && rgb332Level((c >> 8) & 0xff, 7) && rgb332Level(c & 0xff, 3); }
constexpr unsigned bg = 0x000000, panel = 0x002455, hub = 0x242455, white = 0xffffff;
constexpr unsigned muted = 0x9292aa, dim = 0x6d6daa;
constexpr unsigned amber = 0xffb655, teal = 0x49dbaa, blue = 0x6db6ff, lime = 0xb6db55;
constexpr unsigned badgeLive = 0x004955, badgeDemo = 0x6d4900;
static_assert(rgb332(bg) && rgb332(panel) && rgb332(hub) && rgb332(white) && rgb332(muted) && rgb332(dim) &&
              rgb332(amber) && rgb332(teal) && rgb332(blue) && rgb332(lime) && rgb332(badgeLive) && rgb332(badgeDemo),
              "Palette must be exact RGB332 levels");

void power(Canvas& c, int x, int y, const energy::Reading& r, bool live) {
    char value[30];
    const long watts = std::lround(std::abs(r.value));
    // Watts below 1 kW, two decimals above: a third decimal only adds flicker.
    if (!r.valid && !r.receivedMs) std::snprintf(value, sizeof(value), "-- kW");
    else if (watts < 1000) std::snprintf(value, sizeof(value), "%ld W", watts);
    else std::snprintf(value, sizeof(value), "%.2f kW", watts / 1000.0f);
    c.text(x, y, 4, live ? white : dim, Left, value);
}
void arrow(Canvas& c, int x0, int y0, int x1, int y1, unsigned color, bool active, unsigned phase) {
    c.line(x0,y0,x1,y1, active ? color : dim);
    if (!active) return;
    const int dx = (x1 > x0) - (x1 < x0), dy = (y1 > y0) - (y1 < y0);
    c.line(x1,y1,x1-dx*5-dy*3,y1-dy*5+dx*3,color);
    c.line(x1,y1,x1-dx*5+dy*3,y1-dy*5-dx*3,color);
    const int length = std::abs(x1-x0)+std::abs(y1-y0);
    const int t = 2 + (phase % 4) * (length-4) / 4;
    c.rect(x0+dx*t-1,y0+dy*t-1,3,3,white);
}
}
void drawFlow(Canvas& c, const FlowView& v) {
    c.rect(0,0,320,240,bg);
    c.text(8,7,2,white,Left,"HOME ENERGY");
    bool all = true;
    for (auto metric : energy::displayMetrics) all &= v.usable[metric];
    const char* badge = v.data.demo ? "DEMO" : !v.wifi ? "OFFLINE" : all ? "HA" : "WAIT";
    c.rect(267,5,45,21,v.data.demo ? badgeDemo : badgeLive,5);
    c.text(289,8,2,v.data.demo ? amber : teal,Center,badge);

    c.rect(8,33,144,76,panel,8);
    c.rect(168,33,144,76,panel,8);
    c.rect(8,139,144,76,panel,8);
    c.rect(168,139,144,76,panel,8);
    c.rect(17,44,3,11,amber,1);
    c.text(25,42,2,amber,Left,"SOLAR");
    power(c,17,63,v.data.readings[energy::Solar],v.usable[energy::Solar]);
    c.text(17,94,1,muted,Left,"Calculated / DC");
    c.rect(177,44,3,11,teal,1);
    c.text(185,42,2,teal,Left,"HOME");
    power(c,177,63,v.data.readings[energy::Home],v.usable[energy::Home]);
    c.text(177,94,1,muted,Left,"Home demand / AC");

    const float battery = v.data.readings[energy::Battery].value;
    const bool batteryLive = v.usable[energy::Battery];
    const char* batteryTitle = !batteryLive ? "BATTERY / DC" : battery < -30 ? "CHARGING / DC" : battery > 30 ? "DISCHARGE / DC" : "BATTERY IDLE";
    c.text(17,147,2,lime,Left,batteryTitle);
    power(c,17,167,v.data.readings[energy::Battery],batteryLive);
    char soc[40];
    const char* gap = *v.batteryLabel ? "  " : "";
    if (v.data.readings[energy::Soc].valid || v.data.readings[energy::Soc].receivedMs)
        std::snprintf(soc,sizeof(soc),"%s%s%.1f%%", v.batteryLabel, gap, v.data.readings[energy::Soc].value);
    else std::snprintf(soc,sizeof(soc),"%s%s--%%", v.batteryLabel, gap);
    c.text(17,200,1,v.usable[energy::Soc] ? muted : dim,Left,soc);

    const float grid = v.data.readings[energy::Grid].value;
    const char* gridTitle = !v.usable[energy::Grid] ? "GRID / AC" : grid > 30 ? "GRID IMPORT" : grid < -30 ? "GRID EXPORT" : "GRID IDLE";
    c.text(177,147,2,blue,Left,gridTitle);
    power(c,177,167,v.data.readings[energy::Grid],v.usable[energy::Grid]);
    c.text(177,200,1,muted,Left,"Whole-property meter");

    // Four net flows meet at the hybrid inverter. A discharging battery
    // feeds the inverter hub, never a misleading arrow into the solar card.
    const bool solarActive = v.usable[energy::Solar] && v.data.readings[energy::Solar].value > 30;
    c.line(80,110,80,118,solarActive ? amber : dim);
    arrow(c,80,118,143,118,amber,solarActive,v.phase);
    const bool homeActive = v.usable[energy::Home] && v.data.readings[energy::Home].value > 30;
    arrow(c,177,118,240,118,teal,homeActive,v.phase);
    c.line(240,118,240,110,homeActive ? teal : dim);
    const bool batteryActive = batteryLive && std::abs(battery)>30;
    c.line(80,130,80,138,batteryActive ? lime : dim);
    if (battery > 0) arrow(c,80,130,143,130,lime,batteryActive,v.phase);
    else arrow(c,143,130,80,130,lime,batteryActive,v.phase);
    const bool gridActive = v.usable[energy::Grid] && std::abs(grid)>30;
    c.line(240,130,240,138,gridActive ? blue : dim);
    if (grid < 0) arrow(c,177,130,240,130,blue,gridActive,v.phase);
    else arrow(c,240,130,177,130,blue,gridActive,v.phase);
    c.rect(144,112,32,24,hub,4);
    c.text(160,120,1,muted,Center,"INV");
    c.text(8,224,1,muted,Left,v.status);
    c.text(312,224,1,muted,Right,v.address);
}
}  // namespace ui

