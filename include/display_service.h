#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace cyd::display {

void begin();
void showBootMessage(const String& title, const String& detail);
void showWifiPortalMessage(const String& apName);
TFT_eSPI& tft();
// Panel position of a point in the UI frame (CYD_EXPECTED_WIDTH x
// CYD_EXPECTED_HEIGHT). The frame matches every supported panel, so this is
// the identity; a larger panel would show the frame centred at 1x.
int16_t panelX(int16_t frameX);
int16_t panelY(int16_t frameY);
// Pushes the frame region (x, y, w, h) from an 8-bit sprite whose first row
// is frame row bandY (0 when the sprite holds the whole frame).
void pushFrame(TFT_eSprite& band, int16_t bandY, int16_t x, int16_t y, int16_t w, int16_t h);

}  // namespace cyd::display

