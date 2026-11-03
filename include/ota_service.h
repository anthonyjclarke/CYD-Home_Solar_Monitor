#pragma once

#include <Arduino.h>

namespace cyd::ota {

struct Status {
  bool enabled;
  String hostname;
};

void begin(const String& hostname);
void update();
const Status& status();

}  // namespace cyd::ota

