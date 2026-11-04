#pragma once
#include <algorithm>
#include <cstdint>

namespace night {
enum Mode : uint8_t { DimOnly, OffOnly, DimThenOff };
enum Phase : uint8_t { Normal, Dimmed, Off };
constexpr uint32_t wakeMs = 60000;
struct Schedule {
    bool enabled = true;
    Mode mode = DimOnly;
    uint16_t dimAt = 19*60, offAt = 23*60, onAt = 7*60;
};
inline bool minuteValid(int minute) { return minute >= 0 && minute < 24*60; }
inline unsigned elapsed(unsigned start, unsigned end) { return (end+1440-start)%1440; }
inline bool valid(const Schedule& s) {
    if (s.mode>DimThenOff || !minuteValid(s.dimAt) || !minuteValid(s.offAt) || !minuteValid(s.onAt)) return false;
    // Equal start/end disables a single-action schedule, as in legacy settings.
    // A staged schedule must have its off time inside the dim-to-on interval.
    return !s.enabled || s.mode!=DimThenOff || (s.dimAt!=s.onAt && elapsed(s.dimAt,s.offAt)<elapsed(s.dimAt,s.onAt));
}
inline Phase phase(const Schedule& s, bool clockKnown, unsigned minute) {
    if (!clockKnown || !s.enabled || !valid(s) || minute>=1440) return Normal;
    const unsigned start=s.mode==OffOnly?s.offAt:s.dimAt;
    const unsigned sinceStart=elapsed(start,minute), length=elapsed(start,s.onAt);
    if (sinceStart>=length) return Normal;
    if (s.mode==OffOnly || (s.mode==DimThenOff && sinceStart>=elapsed(start,s.offAt))) return Off;
    return Dimmed;
}
inline uint8_t brightness(Phase phase, uint8_t normal, uint32_t now, uint32_t lastTouch) {
    if (phase==Normal || uint32_t(now-lastTouch)<=wakeMs) return normal;
    return phase==Off?0:std::max(5,int(normal)/5);
}
}  // namespace night
