#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace cyd::display {

void begin();
void showBootMessage(const String& title, const String& detail);
void showWifiPortalMessage(const String& apName);
TFT_eSPI& tft();
// Panel position of a point in the 320 x 240 UI frame: the identity on
// 320 x 240 panels, centred at 1x on larger ones (80,40 on the 4.0-inch).
int16_t panelX(int16_t frameX);
int16_t panelY(int16_t frameY);
// Pushes the frame region (x, y, w, h) of the 8-bit 320 x 240 sprite.
void pushFrame(TFT_eSprite& frame, int16_t x, int16_t y, int16_t w, int16_t h);

}  // namespace cyd::display

