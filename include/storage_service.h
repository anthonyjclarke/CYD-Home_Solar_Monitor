#pragma once

#include <Arduino.h>

namespace cyd::storage {

struct Settings {
  String deviceName;
  uint32_t bootCount;
  uint8_t debugLevel;
};

void begin();
const Settings& settings();
void setDebugLevel(uint8_t level);

}  // namespace cyd::storage

