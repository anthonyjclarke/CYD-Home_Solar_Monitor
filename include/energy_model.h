#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cctype>
#include "weather.h"

namespace energy {
// Keep the first five source indices stable for saved 0.1.x configurations.
enum Metric : uint8_t { Inverter, Home, Grid, Battery, Soc, InverterDc, SourceCount, Solar = SourceCount, Count };
constexpr Metric displayMetrics[] = {Solar, Home, Grid, Battery, Soc};
struct Reading {
    float value = 0;
    bool valid = false;
    uint32_t receivedMs = 0;
    uint64_t reportedMs = 0;
};
// Optional daily totals in kWh, read from HA on a slower cadence. They are not
// power sources and are not part of the persisted six-entity mapping.
enum Total : uint8_t { Today, Tomorrow, TotalCount };
struct Snapshot {
    Reading readings[Count];
    Reading totals[TotalCount];
    uint32_t cycleMs = 0;
    uint32_t revision = 0;
    uint32_t skewSeconds = 0;  // Raw spread of report times; diagnostic only.
    // Newest upstream report time among this cycle's valid sources: proof the
    // integration is still writing states.
    uint64_t heartbeatMs = 0;
    weather::Report weather;
    bool demo = false;
    char message[80] = "Configure Home Assistant";
};

// HA state strings include unknown/unavailable. Reject partial numbers,
// non-finite values, unexpected units and physically implausible magnitudes.
inline bool parseValue(const char* state, const char* unit, Metric metric, float& value) {
    if (!state || !unit || !*state) return false;
    char* end = nullptr;
    double n = std::strtod(state, &end);
    if (end == state) return false;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end || !std::isfinite(n)) return false;
    if (metric == Soc) {
        if (std::strcmp(unit, "%") || n < 0 || n > 100) return false;
    } else {
        if (!std::strcmp(unit, "kW")) n *= 1000;
        else if (std::strcmp(unit, "W") && std::strcmp(unit, "Watt") && std::strcmp(unit, "Watts") &&
                 std::strcmp(unit, "watt") && std::strcmp(unit, "watts")) return false;
        if (std::abs(n) > 1000000 || (metric == Home && n < 0)) return false;
        // Source grid is +export; battery is +charge. Internal signs are
        // +import and +discharge, respectively.
        if (metric == Grid || metric == Battery) n = -n;
    }
    value = static_cast<float>(n);
    return true;
}

// Energy totals: kWh, Wh or MWh, normalised to kWh. A daily meter or a
// forecast can never be negative or implausibly large for a home.
inline bool parseEnergy(const char* state, const char* unit, float& kwh) {
    if (!state || !unit || !*state) return false;
    char* end = nullptr;
    double n = std::strtod(state, &end);
    if (end == state) return false;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end || !std::isfinite(n)) return false;
    if (!std::strcmp(unit, "Wh")) n /= 1000;
    else if (!std::strcmp(unit, "MWh")) n *= 1000;
    else if (std::strcmp(unit, "kWh")) return false;
    if (n < 0 || n > 10000) return false;
    kwh = static_cast<float>(n);
    return true;
}

// Totals change slowly (a daily meter is flat overnight), so upstream report
// time is not used; only the device's receive age decides whether to show them.
constexpr uint32_t totalStaleSeconds = 600;
inline bool totalUsable(const Snapshot& s, Total total, bool online, uint32_t now) {
    const Reading& r = s.totals[total];
    return (s.demo || online) && r.valid && static_cast<uint32_t>(now - r.receivedMs) <= totalStaleSeconds * 1000UL;
}

inline int64_t civilDays(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yo = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    return era * 146097 + yo * 365 + yo / 4 - yo / 100 + doy - 719468;
}

// Parse ISO-8601 HA timestamps without depending on the device's timezone.
inline uint64_t parseTimestamp(const char* s) {
    if (!s || std::strlen(s) < 20) return 0;
    int y, mo, d, h, mi, sec, pos = 0;
    if (std::sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &sec, &pos) != 6)
        return 0;
    if (y < 1970 || y > 2100 || mo < 1 || mo > 12 || h < 0 || h > 23 || mi < 0 || mi > 59 || sec < 0 || sec > 59) return 0;
    const int mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    if (d < 1 || d > mdays[mo - 1] + (mo == 2 && leap)) return 0;
    const char* p = s + pos;
    unsigned fraction = 0, digits = 0;
    if (*p == '.') {
        ++p;
        while (std::isdigit(static_cast<unsigned char>(*p))) {
            if (digits < 3) fraction = fraction * 10 + (*p - '0');
            ++digits; ++p;
        }
        if (!digits) return 0;
        for (unsigned i = digits; i < 3; ++i) fraction *= 10;
    }
    int offset = 0;
    if (*p == 'Z' && !p[1]) { /* UTC */ }
    else if ((*p == '+' || *p == '-') && std::strlen(p) == 6 && p[3] == ':' &&
             std::isdigit(p[1]) && std::isdigit(p[2]) && std::isdigit(p[4]) && std::isdigit(p[5])) {
        int oh = (p[1] - '0') * 10 + p[2] - '0';
        int om = (p[4] - '0') * 10 + p[5] - '0';
        if (oh > 23 || om > 59) return 0;
        offset = (oh * 60 + om) * 60 * (*p == '+' ? 1 : -1);
    } else return 0;
    const int64_t seconds = civilDays(y, mo, d) * 86400 + h * 3600 + mi * 60 + sec - offset;
    return seconds >= 0 ? static_cast<uint64_t>(seconds) * 1000 + fraction : 0;
}

inline bool fresh(const Reading& r, uint32_t nowMs, uint64_t epochMs, uint32_t staleSeconds) {
    if (!r.valid || static_cast<uint32_t>(nowMs - r.receivedMs) > staleSeconds * 1000UL) return false;
    if (r.reportedMs && epochMs) {
        if (r.reportedMs > epochMs + 60000) return false;
        if (epochMs > r.reportedMs && epochMs - r.reportedMs > staleSeconds * 1000ULL) return false;
    }
    return true;
}

// HA does not refresh last_reported for every unchanged state (observed: an
// idle, full battery kept a 10-minute-old report time while the inverter
// entities from the same integration updated every few seconds). An unchanged
// value is current while the integration is alive, so report age is judged
// from the snapshot heartbeat. Entities going unavailable are still rejected
// by parseValue; a whole stalled integration still goes stale.
inline Reading effective(const Snapshot& s, const Reading& r) {
    Reading out = r;
    if (r.reportedMs && s.heartbeatMs > r.reportedMs) out.reportedMs = s.heartbeatMs;
    return out;
}

inline uint64_t newestReport(const Snapshot& s) {
    uint64_t newest = 0;
    for (int i = 0; i < SourceCount; ++i)
        if (s.readings[i].valid && s.readings[i].reportedMs > newest) newest = s.readings[i].reportedMs;
    return newest;
}

inline bool solarSourcesCoherent(const Snapshot& s, uint32_t now, uint32_t maxSkewSeconds) {
    const auto dc = effective(s, s.readings[InverterDc]);
    const auto battery = effective(s, s.readings[Battery]);
    const uint32_t dcAge = now - dc.receivedMs, batteryAge = now - battery.receivedMs;
    if ((dcAge > batteryAge ? dcAge - batteryAge : batteryAge - dcAge) > maxSkewSeconds * 1000UL) return false;
    if (dc.reportedMs && battery.reportedMs) {
        const uint64_t skew = dc.reportedMs > battery.reportedMs ? dc.reportedMs - battery.reportedMs : battery.reportedMs - dc.reportedMs;
        if (skew > maxSkewSeconds * 1000ULL) return false;
    }
    return true;
}

inline void deriveSolar(Snapshot& s, uint32_t now, uint32_t maxSkewSeconds) {
    const auto dc = effective(s, s.readings[InverterDc]);
    const auto battery = effective(s, s.readings[Battery]);
    auto& solar = s.readings[Solar];
    solar.valid = false; // Preserve only the previous number if inputs fail.
    if (!dc.valid || !battery.valid || !solarSourcesCoherent(s, now, maxSkewSeconds)) return;
    // Integration formula: max(DC + raw battery (+charge), 0).
    // Internally battery is +discharge, so subtract it. Never substitute AC.
    const float pv = dc.value - battery.value;
    if (!std::isfinite(pv)) return;
    solar.value = pv > 0 ? pv : 0;
    solar.valid = true;
    solar.receivedMs = uint32_t(now - dc.receivedMs) >= uint32_t(now - battery.receivedMs) ? dc.receivedMs : battery.receivedMs;
    solar.reportedMs = dc.reportedMs && battery.reportedMs ?
        (dc.reportedMs < battery.reportedMs ? dc.reportedMs : battery.reportedMs) : 0;
}

inline bool metricFresh(const Snapshot& s, Metric metric, uint32_t now, uint64_t epoch,
                        uint32_t staleSeconds, uint32_t maxSkewSeconds) {
    if (!fresh(effective(s, s.readings[metric]), now, epoch, staleSeconds)) return false;
    if (metric != Solar) return true;
    return fresh(effective(s, s.readings[InverterDc]), now, epoch, staleSeconds) &&
           fresh(effective(s, s.readings[Battery]), now, epoch, staleSeconds) && solarSourcesCoherent(s, now, maxSkewSeconds);
}

// Largest DC/battery timestamp gap accepted for solar: two polls, at least 10 s.
inline uint32_t skewLimitSeconds(uint32_t pollSeconds) { return pollSeconds * 2 > 10 ? pollSeconds * 2 : 10; }

// A reading is shown live only while online (or in demo) and fresh. Shared by
// the panel, browser API and serial diagnostics so they always agree.
inline bool metricUsable(const Snapshot& s, Metric metric, bool online, uint32_t now, uint64_t epoch,
                         uint32_t staleSeconds, uint32_t pollSeconds) {
    return (s.demo || online) && metricFresh(s, metric, now, epoch, staleSeconds, skewLimitSeconds(pollSeconds));
}

inline Snapshot demoSnapshot(uint32_t now, unsigned scene) {
    Snapshot s;
    const float scenes[][SourceCount] = {
        {463,1990,1527,-3904,28.5f,500},
        {4200,1200,-3000,-1800,63.2f,4300},
        {1800,1800,0,1900,57.4f,1900},
        {0,840,840,0,28.5f,0}
    };
    for (int i = 0; i < SourceCount; ++i) s.readings[i] = {scenes[scene % 4][i], true, now, 0};
    const float today[] = {25.6f, 31.2f, 18.9f, 22.4f};
    s.totals[Today] = {today[scene % 4], true, now, 0};
    s.totals[Tomorrow] = {21.0f, true, now, 0};
    const int sky[] = {2, 0, 3, 61};
    s.weather = {true, now, 23.5f, sky[scene % 4], 24.0f, sky[(scene + 1) % 4]};
    deriveSolar(s, now, 10);
    s.demo = true; s.cycleMs = now;
    std::snprintf(s.message, sizeof(s.message), "Demo / sample data");
    return s;
}
}  // namespace energy

