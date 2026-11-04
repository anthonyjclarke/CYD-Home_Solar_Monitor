#pragma once

#include "config.h"
#include "debug_levels.h"
#include <Arduino.h>
#include <atomic>

// Written by the main task when settings change, read by both Arduino tasks.
extern std::atomic<uint8_t> debugLevel;

#define DBG_ERROR(fmt, ...)   do { if (debugLevel >= DBG_LEVEL_ERROR)   Serial.printf("[ERR ][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)
#define DBG_WARN(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_WARN)    Serial.printf("[WARN][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)
#define DBG_INFO(fmt, ...)    do { if (debugLevel >= DBG_LEVEL_INFO)    Serial.printf("[INFO][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)
#define DBG_VERBOSE(fmt, ...) do { if (debugLevel >= DBG_LEVEL_VERBOSE) Serial.printf("[VERB][%lu ms] " fmt "\n", static_cast<unsigned long>(millis()), ##__VA_ARGS__); } while (0)

