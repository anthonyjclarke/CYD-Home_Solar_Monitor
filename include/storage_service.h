#pragma once

#include <Arduino.h>

namespace cyd::storage {

struct Settings {
  String deviceName;
  uint32_t bootCount;
};

void begin();
const Settings& settings();

}  // namespace cyd::storage

