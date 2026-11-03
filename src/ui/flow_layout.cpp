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
// Centre layout: light theme. Pure white is the closest RGB332 level to an off-white.
constexpr unsigned paper = 0xffffff, ink = 0x242455, label = 0x6d6d55, track = 0xb6b6aa, hubDot = 0x9292aa;
constexpr unsigned solarInk = 0x926d00, solarFlow = 0xdb9255, gridInk = 0x2449aa, batteryInk = 0x249255;
constexpr unsigned goodDot = 0x92b6aa, lowRed = 0xdb2400;
static_assert(rgb332(paper) && rgb332(ink) && rgb332(label) && rgb332(track) && rgb332(hubDot) && rgb332(solarInk) &&
              rgb332(solarFlow) && rgb332(gridInk) && rgb332(batteryInk) && rgb332(goodDot) && rgb332(lowRed),
              "Palette must be exact RGB332 levels");

// Watts below 1 kW, two decimals above: a third decimal only adds flicker.
void formatPower(char* out, size_t size, const char*& unit, const energy::Reading& r) {
    const long watts = std::lround(std::abs(r.value));
    if (!r.valid && !r.receivedMs) { std::snprintf(out, size, "--"); unit = "kW"; }
    else if (watts < 1000) { std::snprintf(out, size, "%ld", watts); unit = "W"; }
    else { std::snprintf(out, size, "%.2f", watts / 1000.0f); unit = "kW"; }
}

void power(Canvas& c, int x, int y, const energy::Reading& r, bool live) {
    char number[20], value[30];
    const char* unit;
    formatPower(number, sizeof(number), unit, r);
    std::snprintf(value, sizeof(value), "%s %s", number, unit);
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
// Battery outline with a terminal nub, filled to the state of energy:
// healthy colour from 50%, amber from 20%, red below.
void batteryGauge(Canvas& c, int x, int y, int w, int h, const energy::Reading& soc, bool live,
                  unsigned outline, unsigned inside, unsigned healthy) {
    c.rect(x, y, w, h, outline, 2);
    c.rect(x + w, y + h / 4, 3, h - h / 2, outline);
    c.rect(x + 1, y + 1, w - 2, h - 2, inside, 1);
    if (!soc.valid && !soc.receivedMs) return;
    const float level = std::fmin(std::fmax(soc.value, 0.0f), 100.0f);
    const int fill = int((w - 4) * level / 100 + 0.5f);
    const unsigned color = !live ? outline : level >= 50 ? healthy : level >= 20 ? solarFlow : lowRed;
    if (fill > 0) c.rect(x + 2, y + 2, fill, h - 4, color, 1);
}

void cloud(Canvas& c, int x, int y, unsigned color) {
    c.rect(x + 1, y + 5, 16, 6, color, 3);
    c.rect(x + 3, y + 2, 8, 8, color, 4);
    c.rect(x + 8, y, 9, 9, color, 4);
}

void sun(Canvas& c, int cx, int cy, int r, unsigned color) {
    c.rect(cx - r, cy - r, 2 * r + 1, 2 * r + 1, color, r);
    const int a = r + 2, b = r + 3, d = (r + 2) * 7 / 10, e = (r + 3) * 7 / 10;
    c.line(cx, cy - a, cx, cy - b, color); c.line(cx, cy + a, cx, cy + b, color);
    c.line(cx - a, cy, cx - b, cy, color); c.line(cx + a, cy, cx + b, cy, color);
    c.line(cx - d, cy - d, cx - e, cy - e, color); c.line(cx + d, cy - d, cx + e, cy - e, color);
    c.line(cx - d, cy + d, cx - e, cy + e, color); c.line(cx + d, cy + d, cx + e, cy + e, color);
}

// 20 x 18 weather icon from primitives, top-left at (x, y).
void skyIcon(Canvas& c, int x, int y, weather::Sky sky, unsigned sunColor, unsigned cloudColor, unsigned rainColor) {
    switch (sky) {
        case weather::Clear: sun(c, x + 10, y + 9, 4, sunColor); break;
        case weather::PartCloud: sun(c, x + 13, y + 6, 3, sunColor); cloud(c, x, y + 7, cloudColor); break;
        case weather::Cloud: cloud(c, x + 1, y + 4, cloudColor); break;
        case weather::Fog:
            for (int i = 0; i < 3; ++i) c.rect(x + 1 + (i % 2) * 2, y + 4 + i * 5, 16, 2, cloudColor, 1);
            break;
        case weather::Drizzle: case weather::Rain: case weather::Snow: case weather::Storm:
            cloud(c, x + 1, y, cloudColor);
            if (sky == weather::Storm) {
                c.line(x + 10, y + 12, x + 7, y + 15, sunColor); c.line(x + 7, y + 15, x + 11, y + 15, sunColor);
                c.line(x + 11, y + 15, x + 8, y + 18, sunColor);
            } else for (int i = 0; i < 3; ++i) {
                const int dx = x + 5 + i * 5;
                if (sky == weather::Snow) c.rect(dx - 1, y + 13 + (i % 2) * 3, 2, 2, rainColor);
                else if (sky == weather::Drizzle) c.rect(dx - 1, y + 14 + (i % 2) * 2, 2, 2, rainColor);
                else c.line(dx, y + 13, dx - 2, y + 17, rainColor);
            }
            break;
        default: break;
    }
}

void drawCards(Canvas& c, const FlowView& v) {
    c.rect(0,0,320,240,bg);
    char title[32] = "HOME ENERGY";
    if (v.totalLive[energy::Today]) std::snprintf(title, sizeof(title), "%.1f kWh SOLAR TODAY", v.data.totals[energy::Today].value);
    c.text(8,7,2,white,Left,title);
    bool all = true;
    for (auto metric : energy::displayMetrics) all &= v.usable[metric];
    const char* badge = v.data.demo ? "DEMO" : !v.wifi ? "OFFLINE" : all ? "HA" : "WAIT";
    if (v.weatherLive) {
        char now[12];
        std::snprintf(now, sizeof(now), "%.0f`", v.data.weather.nowC);  // Font 2 draws ` as a degree sign.
        c.text(260, 8, 2, white, Right, now);
        skyIcon(c, 260 - c.textWidth(2, now) - 24, 5, weather::skyFor(v.data.weather.nowCode), amber, muted, blue);
    }
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
    const char* batteryTitle = !batteryLive ? "BATTERY / DC" : battery < -30 ? "CHARGING / DC" : battery > 30 ? "SUPPLYING / DC" : "BATTERY IDLE";
    c.text(17,147,2,lime,Left,batteryTitle);
    power(c,17,167,v.data.readings[energy::Battery],batteryLive);
    char soc[40];
    const char* gap = *v.batteryLabel ? "  " : "";
    if (v.data.readings[energy::Soc].valid || v.data.readings[energy::Soc].receivedMs)
        std::snprintf(soc,sizeof(soc),"%s%s%.1f%%", v.batteryLabel, gap, v.data.readings[energy::Soc].value);
    else std::snprintf(soc,sizeof(soc),"%s%s--%%", v.batteryLabel, gap);
    c.text(17,200,1,v.usable[energy::Soc] ? muted : dim,Left,soc);
    batteryGauge(c,17+c.textWidth(1,soc)+6,198,22,11,v.data.readings[energy::Soc],v.usable[energy::Soc],muted,panel,lime);

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

// Value in font 4 with a small grey unit, positioned by alignment of the pair.
void reading(Canvas& c, int x, int y, Align align, const energy::Reading& r, bool live, unsigned color) {
    char number[20];
    const char* unit;
    formatPower(number, sizeof(number), unit, r);
    const int numberWidth = c.textWidth(4, number), width = numberWidth + 3 + c.textWidth(2, unit);
    const int left = align == Left ? x : align == Right ? x - width : x - width / 2;
    c.text(left, y, 4, live ? color : track, Left, number);
    c.text(left + numberWidth + 3, y + 8, 2, live ? label : track, Left, unit);
}

// Chevrons march along a straight horizontal or vertical path. They stop, and
// only the faint track remains, when the flow is stale or below the idle threshold.
void chevrons(Canvas& c, int x0, int y0, int x1, int y1, unsigned color, bool active, bool forward, unsigned phase) {
    c.line(x0, y0, x1, y1, track);
    if (!active) return;
    const bool vertical = x0 == x1;
    const int length = vertical ? y1 - y0 : x1 - x0, spacing = 9, size = 4;
    const int offset = int(phase * 3 % spacing);
    for (int p = forward ? offset : spacing - 1 - offset; p + size <= length; p += spacing) {
        // Tip leads in the direction of flow.
        const int tip = forward ? p + size : p, tail = forward ? p : p + size;
        for (int w = 0; w < 2; ++w) {  // Two strokes for a bolder mark.
            if (vertical) {
                c.line(x0 - size, y0 + tail + (forward ? -w : w), x0, y0 + tip + (forward ? -w : w), color);
                c.line(x0 + size, y0 + tail + (forward ? -w : w), x0, y0 + tip + (forward ? -w : w), color);
            } else {
                c.line(x0 + tail + (forward ? -w : w), y0 - size, x0 + tip + (forward ? -w : w), y0, color);
                c.line(x0 + tail + (forward ? -w : w), y0 + size, x0 + tip + (forward ? -w : w), y0, color);
            }
        }
    }
}

void gear(Canvas& c, int x, int y, unsigned color) {
    c.rect(x - 1, y - 7, 3, 14, color);
    c.rect(x - 7, y - 1, 14, 3, color);
    c.line(x - 5, y - 5, x + 5, y + 5, color); c.line(x - 5, y - 4, x + 4, y + 5, color);
    c.line(x - 5, y + 5, x + 5, y - 5, color); c.line(x - 5, y + 4, x + 4, y - 5, color);
    c.rect(x - 5, y - 5, 11, 11, color, 5);
    c.rect(x - 2, y - 2, 5, 5, paper, 2);
}

void drawCentre(Canvas& c, const FlowView& v) {
    const auto& r = v.data.readings;
    const auto& u = v.usable;
    c.rect(0, 0, 320, 240, paper);
    // Top row: today's solar energy (else the clock) and tomorrow's forecast.
    if (v.totalLive[energy::Today]) {
        char today[16];
        std::snprintf(today, sizeof(today), "%.2f", v.data.totals[energy::Today].value);
        c.text(12, 6, 4, ink, Left, today);
        c.text(12 + c.textWidth(4, today) + 4, 14, 2, label, Left, "kWh today");
    } else {
        c.text(12, 6, 4, ink, Left, v.time);
        if (v.weatherLive && *v.time) {
            char now[12];
            std::snprintf(now, sizeof(now), "%.0f`", v.data.weather.nowC);  // Font 2 draws ` as a degree sign.
            c.text(12 + c.textWidth(4, v.time) + 8, 14, 2, label, Left, now);
        }
    }
    // Right: tomorrow's sky icon, then the solar forecast in kWh, or the
    // forecast maximum temperature when no kWh forecast entity is set.
    char forecast[16] = "";
    if (v.totalLive[energy::Tomorrow]) std::snprintf(forecast, sizeof(forecast), "~%.0f kWh", v.data.totals[energy::Tomorrow].value);
    else if (v.weatherLive) std::snprintf(forecast, sizeof(forecast), "%.0f`", v.data.weather.tomorrowMaxC);
    if (*forecast) {
        c.text(308, 14, 2, v.totalLive[energy::Tomorrow] ? solarInk : ink, Right, forecast);
        const int labelRight = 308 - c.textWidth(2, forecast) - 8;
        c.text(labelRight, 14, 2, label, Right, "Tomorrow");
        if (v.weatherLive)
            skyIcon(c, labelRight - c.textWidth(2, "Tomorrow") - 24, 10, weather::skyFor(v.data.weather.tomorrowCode),
                    solarFlow, hubDot, gridInk);
    } else if (v.data.demo) c.text(308, 14, 2, solarInk, Right, "DEMO");
    else if (!v.wifi) c.text(308, 14, 2, label, Right, "OFFLINE");

    // Labelled arms around a central hub: solar above, house below, grid left,
    // battery right. Font 4 values keep every label on screen and readable.
    c.text(160, 30, 2, label, Center, "SOLAR");
    reading(c, 160, 46, Center, r[energy::Solar], u[energy::Solar], solarInk);
    c.text(12, 78, 2, label, Left, "GRID");
    reading(c, 12, 96, Left, r[energy::Grid], u[energy::Grid], gridInk);
    c.text(308, 78, 2, label, Right, "BATTERY");
    reading(c, 308, 96, Right, r[energy::Battery], u[energy::Battery], batteryInk);
    c.text(160, 150, 2, label, Center, "HOUSE");
    reading(c, 160, 166, Center, r[energy::Home], u[energy::Home], ink);

    const float grid = r[energy::Grid].value, battery = r[energy::Battery].value;
    const char* gridState = !u[energy::Grid] ? "No data" : grid > 30 ? "Importing" : grid < -30 ? "Exporting" : "Idle";
    c.text(12, 124, 2, u[energy::Grid] ? gridInk : track, Left, gridState);
    const char* batteryState = !u[energy::Battery] ? "No data" : battery < -30 ? "Charging" : battery > 30 ? "Supplying" : "Idle";
    c.text(214, 124, 2, u[energy::Battery] ? batteryInk : track, Left, batteryState);
    char soc[12];
    const bool socKnown = r[energy::Soc].valid || r[energy::Soc].receivedMs;
    if (socKnown) std::snprintf(soc, sizeof(soc), "%.0f%%", r[energy::Soc].value);
    else std::snprintf(soc, sizeof(soc), "--%%");
    // State of energy as a battery icon and its percentage, on its own row so
    // it is not read as part of "Charging" / "Supplying".
    batteryGauge(c, 214, 142, 52, 14, r[energy::Soc], u[energy::Soc], u[energy::Soc] ? ink : track, paper, batteryInk);
    c.text(308, 141, 2, u[energy::Soc] ? ink : track, Right, soc);

    // Grid +import flows into the hub; battery +discharge flows into the hub.
    chevrons(c, 160, 74, 160, 102, solarFlow, u[energy::Solar] && r[energy::Solar].value > 30, true, v.phase);
    chevrons(c, 104, 108, 152, 108, gridInk, u[energy::Grid] && std::abs(grid) > 30, grid > 0, v.phase);
    chevrons(c, 168, 108, 210, 108, batteryInk, u[energy::Battery] && std::abs(battery) > 30, battery < 0, v.phase);
    chevrons(c, 160, 114, 160, 144, ink, u[energy::Home] && r[energy::Home].value > 30, true, v.phase);
    c.rect(156, 104, 9, 9, hubDot, 4);

    bool all = true;
    for (auto metric : energy::displayMetrics) all &= u[metric];
    const bool good = all && !v.data.demo && (grid < -100 || (battery < -100 && r[energy::Soc].value >= 95));
    const char* status = good ? "Good time to use power" : all && !v.data.demo ? "All readings live" : v.status;
    c.rect(12, 218, 9, 9, good ? goodDot : all ? hubDot : track, 4);
    c.text(27, 214, 2, good ? batteryInk : label, Left, status);
    const int settingsWidth = c.textWidth(2, "Settings");
    c.text(308, 214, 2, label, Right, "Settings");
    gear(c, 308 - settingsWidth - 12, 221, label);
}
}  // namespace

void drawBoot(Canvas& c, const BootView& v) {
    const bool dark = v.layout == Cards;
    const unsigned back = dark ? bg : paper, text = dark ? white : ink, soft = dark ? muted : label;
    const unsigned rule = dark ? hub : track, ok = dark ? lime : batteryInk, wait = dark ? amber : solarFlow;
    c.rect(0, 0, 320, 240, back);
    sun(c, 24, 24, 7, dark ? amber : solarFlow);
    c.text(44, 12, 4, text, Left, "Solar Monitor");
    c.text(308, 18, 2, soft, Right, v.version);
    c.rect(12, 46, 296, 1, rule);
    for (int i = 0; i < BootView::rowCount; ++i) {
        const BootRow& row = v.rows[i];
        if (!*row.label) continue;
        const int y = 56 + i * 23;
        if (row.state != BootInfo)
            c.rect(14, y + 4, 8, 8, row.state == BootOk ? ok : row.state == BootWaiting ? wait : lowRed, 4);
        c.text(30, y, 2, soft, Left, row.label);
        c.text(104, y, 2, row.state == BootProblem ? lowRed : text, Left, row.value);
    }
    // Progress across the boot screen's time on screen, then what happens next.
    const float p = v.progress < 0 ? 0 : v.progress > 1 ? 1 : v.progress;
    c.rect(12, 204, 296, 5, rule, 2);
    if (p > 0) c.rect(12, 204, int(296 * p + 0.5f), 5, dark ? amber : solarInk, 2);
    c.text(160, 216, 2, soft, Center, v.footer);
}

void drawFlow(Canvas& c, const FlowView& v) {
    if (v.layout == Cards) drawCards(c, v);
    else drawCentre(c, v);
}
}  // namespace ui
