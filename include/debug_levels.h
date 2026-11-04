#pragma once
#include <cstdint>

enum DebugLevel : uint8_t {
    DBG_LEVEL_OFF = 0,
    DBG_LEVEL_ERROR = 1,
    DBG_LEVEL_WARN = 2,
    DBG_LEVEL_INFO = 3,
    DBG_LEVEL_VERBOSE = 4,
};
inline bool validDebugLevel(int level) { return level >= DBG_LEVEL_OFF && level <= DBG_LEVEL_VERBOSE; }
constexpr uint32_t serialReportIntervalMs = 60000;
// New cycles only: routine output is limited to once a minute. Mode changes,
// settings changes and occasional totals/weather requests can be reported at
// once. Verbose retains every-poll diagnostics without repeating one snapshot.
class CycleLogGate {
    uint32_t lastCycle = 0, lastReport = 0;
    uint8_t lastLevel = DBG_LEVEL_OFF;
    bool reported = false;
public:
    bool ready(uint32_t now, uint32_t cycle, uint8_t level, bool changed = false) {
        if (level == DBG_LEVEL_OFF) { lastLevel = level; return false; }
        if (!cycle) return false;
        const bool levelChanged = level != lastLevel;
        lastLevel = level;
        if (cycle == lastCycle && !levelChanged) return false;
        lastCycle = cycle;
        if (!reported || levelChanged || changed || level == DBG_LEVEL_VERBOSE || uint32_t(now-lastReport) >= serialReportIntervalMs) {
            reported = true; lastReport = now; return true;
        }
        return false;
    }
};
inline const char* debugLevelName(uint8_t level) {
    switch (level) {
        case DBG_LEVEL_ERROR: return "error";
        case DBG_LEVEL_WARN: return "warn";
        case DBG_LEVEL_INFO: return "info";
        case DBG_LEVEL_VERBOSE: return "verbose";
        default: return "off";
    }
}
