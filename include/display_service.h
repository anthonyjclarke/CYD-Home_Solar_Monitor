#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace cyd::display {

void begin();
void showBootMessage(const String& title, const String& detail);
void showWifiPortalMessage(const String& apName);
TFT_eSPI& tft();
// Top-left of the 320 x 240 UI frame: 0,0 on 320 x 240 panels, centred on
// larger ones (80,40 on the 4.0-inch 480 x 320).
int16_t originX();
int16_t originY();

}  // namespace cyd::display

