#pragma once

#include <Arduino.h>

namespace cyd::network {

struct Status {
  bool connected;
  bool portalActive;
  String ssid;
  String ip;
  String apName;
};

void begin();
void update();
const Status& status();

}  // namespace cyd::network

