#include "flow_layout.h"
#include <algorithm>
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
// Pipe bands (pale) and comet tails (mid) per flow; heads use the ink colours above.
constexpr unsigned bandSolar = 0xffdbaa, bandGrid = 0xb6dbff, bandBattery = 0xb6ffaa, bandHouse = 0xdbdbff, bandIdle = 0xdbdbaa;
constexpr unsigned tailSolar = 0xffb655, tailGrid = 0x6d92ff, tailBattery = 0x49b655, tailHouse = 0x6d6daa;
static_assert(rgb332(bandSolar) && rgb332(bandGrid) && rgb332(bandBattery) && rgb332(bandHouse) && rgb332(bandIdle) &&
              rgb332(tailSolar) && rgb332(tailGrid) && rgb332(tailBattery) && rgb332(tailHouse),
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
    c.rect(232,218,80,20,hub,3);
    c.text(272,220,2,white,Center,"History");
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
int readingWidth(Canvas& c, const energy::Reading& r) {
    char number[20];
    const char* unit;
    formatPower(number, sizeof(number), unit, r);
    return c.textWidth(4, number) + 3 + c.textWidth(2, unit);
}

void dot(Canvas& c, int x, int y, int r, unsigned color) { c.rect(x - r, y - r, 2 * r + 1, 2 * r + 1, color, r); }

// A 6 px rounded pipe along a horizontal or vertical centre line, with comets
// (head, mid and pale tail) eased from start to end while power flows. Comets
// fade in and out at the ends; speed and count rise with power, size does not.
constexpr int pipeCap = 3;
void pipe(Canvas& c, int x0, int y0, int x1, int y1, unsigned band, unsigned tail, unsigned head,
          bool active, float kw, float& phase, float dt, bool forward) {
    c.rect(std::min(x0, x1) - pipeCap, std::min(y0, y1) - pipeCap, std::abs(x1 - x0) + 2 * pipeCap + 1,
           std::abs(y1 - y0) + 2 * pipeCap + 1, active ? band : bandIdle, pipeCap);
    c.line(x0, y0, x1, y1, track);
    if (!active) return;
    phase = std::fmod(phase + dt / std::fmax(0.9f, 2.6f - kw * 0.35f), 1.0f);
    if (!forward) { std::swap(x0, x1); std::swap(y0, y1); }
    const int ux = (x1 > x0) - (x1 < x0), uy = (y1 > y0) - (y1 < y0), count = kw > 2.5f ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        const float p = std::fmod(phase + float(i) / count, 1.0f), q = p < 0.5f ? 2 * p * p : 1 - 2 * (1 - p) * (1 - p);
        const int x = x0 + int((x1 - x0) * q + 0.5f), y = y0 + int((y1 - y0) * q + 0.5f);
        if (p < 0.12f || p > 0.88f) { dot(c, x, y, 2, tail); continue; }  // Fading in or out.
        dot(c, x - ux * 7, y - uy * 7, 1, tail);
        dot(c, x - ux * 4, y - uy * 4, 2, tail);
        dot(c, x, y, 3, head);
    }
}

// Upright battery with a terminal nub on top, filled from the bottom to the
// state of energy: healthy colour from 50%, amber from 20%, red below.
void uprightBattery(Canvas& c, int x, int y, int w, int h, const energy::Reading& soc, bool live) {
    const unsigned outline = live ? ink : track;
    c.rect(x + 5, y - 4, w - 10, 4, outline, 1);
    c.rect(x, y, w, h, outline, 3);
    c.rect(x + 2, y + 2, w - 4, h - 4, paper, 2);
    if (!soc.valid && !soc.receivedMs) return;
    const float level = std::fmin(std::fmax(soc.value, 0.0f), 100.0f);
    const int fill = int((h - 6) * level / 100 + 0.5f);
    const unsigned color = !live ? track : level >= 50 ? batteryInk : level >= 20 ? solarFlow : lowRed;
    if (fill > 0) c.rect(x + 3, y + h - 3 - fill, w - 6, fill, color, 1);
}

// Comet positions persist between frames so a change in power alters their
// speed smoothly instead of making them jump.
float cometPhase[4] = {0.45f, 0.4f, 0.55f, 0.5f};
uint32_t cometMs = 0;
int textHalf[2] = {36, 36};  // Half widths of the grid and battery blocks.

// A motionOnly frame redraws just the pipes and hub, inside the returned box.
Box drawCentre(Canvas& c, const FlowView& v) {
    const auto& r = v.data.readings;
    const auto& u = v.usable;
    if (!v.motionOnly) {
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
    }

    // Labelled blocks around a hub at (140,115): solar above, house below, grid
    // left and battery right, the side blocks centred on x = 38 and x = 244.
    // Every pipe stops exactly pipeGap px from the ink of its block. Font tops:
    // font 4 digits end 19 px below y, font 2 capitals start 3 px below y.
    // The solar block starts below the top row so "kWh today" can't touch it.
    constexpr int hubX = 140, hubY = 115, hubR = 9, pipeGap = 6, gridX = 38, batteryX = 244, solarY = 50;
    const float grid = r[energy::Grid].value, battery = r[energy::Battery].value;
    const char* gridState = !u[energy::Grid] ? "No data" : grid > 30 ? "Importing" : grid < -30 ? "Exporting" : "Idle";
    const char* batteryState = !u[energy::Battery] ? "No data" : battery < -30 ? "Charging" : battery > 30 ? "Supplying" : "Idle";
    if (!v.motionOnly) {
    c.text(hubX, solarY - 20, 2, label, Center, "SOLAR");
    reading(c, hubX, solarY, Center, r[energy::Solar], u[energy::Solar], solarInk);
    c.text(gridX, 84, 2, label, Center, "GRID");
    reading(c, gridX, 104, Center, r[energy::Grid], u[energy::Grid], gridInk);
    c.text(gridX, 125, 2, u[energy::Grid] ? gridInk : track, Center, gridState);
    c.text(batteryX, 84, 2, label, Center, "BATTERY");
    reading(c, batteryX, 104, Center, r[energy::Battery], u[energy::Battery], batteryInk);
    c.text(batteryX, 125, 2, u[energy::Battery] ? batteryInk : track, Center, batteryState);
    c.text(hubX, 157, 2, label, Center, "HOUSE");
    reading(c, hubX, 178, Center, r[energy::Home], u[energy::Home], ink);

    char soc[12];
    const bool socKnown = r[energy::Soc].valid || r[energy::Soc].receivedMs;
    if (socKnown) std::snprintf(soc, sizeof(soc), "%.0f%%", r[energy::Soc].value);
    else std::snprintf(soc, sizeof(soc), "--%%");
    uprightBattery(c, 291, 88, 18, 56, r[energy::Soc], u[energy::Soc]);
    c.text(300, 148, 2, u[energy::Soc] ? ink : track, Center, soc);
    }

    // Motion frames reuse the text widths of the last full frame, so the box
    // they clear always matches the text on screen.
    if (!v.motionOnly) {
        textHalf[0] = std::max({c.textWidth(2, "GRID"), readingWidth(c, r[energy::Grid]), c.textWidth(2, gridState)}) / 2;
        textHalf[1] = std::max({c.textWidth(2, "BATTERY"), readingWidth(c, r[energy::Battery]), c.textWidth(2, batteryState)}) / 2;
    }
    const int gridHalf = textHalf[0], batteryHalf = textHalf[1];
    const float dt = cometMs && v.ms ? std::min<uint32_t>(v.ms - cometMs, 100) / 1000.0f : 0.0f;
    cometMs = v.ms;
    // The pipes and hub sit inside this box, clear of every text block.
    const int stop = pipeGap + pipeCap;
    Box box;
    box.x = gridX + gridHalf + pipeGap; box.y = solarY + 19 + pipeGap;
    box.w = batteryX - batteryHalf - pipeGap - box.x; box.h = 157 + 3 - pipeGap - box.y;
    if (v.motionOnly) c.rect(box.x, box.y, box.w, box.h, paper);
    // Grid +import flows into the hub; battery +discharge flows into the hub.
    pipe(c, hubX, solarY + 19 + stop, hubX, hubY - hubR, bandSolar, tailSolar, solarFlow,
         u[energy::Solar] && r[energy::Solar].value > 30, r[energy::Solar].value / 1000, cometPhase[0], dt, true);
    pipe(c, gridX + gridHalf + stop, hubY, hubX - hubR, hubY, bandGrid, tailGrid, gridInk,
         u[energy::Grid] && std::abs(grid) > 30, std::abs(grid) / 1000, cometPhase[1], dt, grid > 0);
    pipe(c, hubX + hubR, hubY, batteryX - batteryHalf - stop, hubY, bandBattery, tailBattery, batteryInk,
         u[energy::Battery] && std::abs(battery) > 30, std::abs(battery) / 1000, cometPhase[2], dt, battery < 0);
    pipe(c, hubX, hubY + hubR, hubX, 157 + 3 - stop, bandHouse, tailHouse, ink,
         u[energy::Home] && r[energy::Home].value > 30, r[energy::Home].value / 1000, cometPhase[3], dt, true);
    // Hub: a ring with a small sine wave, the inverter.
    dot(c, hubX, hubY, hubR, hubDot);
    dot(c, hubX, hubY, hubR - 2, paper);
    static const signed char wave[] = {0, -2, -2, 0, 2, 2, 0, -2, -2, 0};
    for (int i = 0; i + 1 < int(sizeof(wave)); ++i) c.line(hubX - 4 + i, hubY + wave[i], hubX - 3 + i, hubY + wave[i + 1], hubDot);
    if (v.motionOnly) return box;

    bool all = true;
    for (auto metric : energy::displayMetrics) all &= u[metric];
    const bool good = all && !v.data.demo && (grid < -100 || (battery < -100 && r[energy::Soc].value >= 95));
    const char* status = good ? (v.reportKnown ? "Good time to use power" : "Spare power / age unknown") :
                         all && !v.data.demo ? (v.reportKnown ? "All readings live" : "Live / source age unknown") : v.status;
    c.rect(12, 218, 9, 9, good ? goodDot : all ? hubDot : track, 4);
    c.text(27, 218, 1, good ? batteryInk : label, Left, status);
    c.rect(232,210,80,26,bandIdle,4);
    c.text(272,214,2,ink,Center,"History");
    return box;
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
        const int y = 54 + i * 21;
        if (row.state != BootInfo)
            c.rect(14, y + 4, 8, 8, row.state == BootOk ? ok : row.state == BootWaiting ? wait : lowRed, 4);
        c.text(30, y, 2, soft, Left, row.label);
        char value[48]; std::snprintf(value,sizeof(value),"%s",row.value);
        size_t length=std::strlen(value);
        const bool clipped=c.textWidth(2,value)>204;
        while (length>3 && c.textWidth(2,value)>204) value[--length]='\0';
        if (clipped && length>=3) std::strcpy(value+length-3,"...");
        c.text(104, y, 2, row.state == BootProblem ? lowRed : text, Left, value);
    }
    // Progress across the boot screen's time on screen, then what happens next.
    const float p = v.progress < 0 ? 0 : v.progress > 1 ? 1 : v.progress;
    c.rect(12, 204, 296, 5, rule, 2);
    if (p > 0) c.rect(12, 204, int(296 * p + 0.5f), 5, dark ? amber : solarInk, 2);
    c.text(160, 216, 2, soft, Center, v.footer);
}

Box drawFlow(Canvas& c, const FlowView& v) {
    if (v.layout == Centre) return drawCentre(c, v);
    drawCards(c, v);
    return {};
}
}  // namespace ui
