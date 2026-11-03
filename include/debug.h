#pragma once

#include "config.h"
#include <Arduino.h>

enum DebugLevel : uint8_t {
  DBG_LEVEL_OFF = 0,
  DBG_LEVEL_ERROR = 1,
  DBG_LEVEL_WARN = 2,
  DBG_LEVEL_INFO = 3,
  DBG_LEVEL_VERBOSE = 4,
};

extern uint8_t debugLevel;

inline const char* debugLevelName(uint8_t level) {
  switch (level) {
    case DBG_LEVEL_ERROR: return "error";
    case DBG_LEVEL_WARN: return "warn";
    case DBG_LEVEL_INFO: return "info";
    case DBG_LEVEL_VERBOSE: return "verbose";
    default: return "off";
  }
}

#define DBG_ERROR(fmt, ...)   do { if (debugLevel >= DBG_LEVEL_ERROR)   Serial.printf("[ERR ][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)
#define DBG_WARN(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_WARN)    Serial.printf("[WARN][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)
#define DBG_INFO(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_INFO)    Serial.printf("[INFO][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)
#define DBG_VERBOSE(fmt, ...) do { if (debugLevel >= DBG_LEVEL_VERBOSE) Serial.printf("[VERB][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)

