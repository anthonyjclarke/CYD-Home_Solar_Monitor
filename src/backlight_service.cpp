#include "backlight_service.h"

#include "board_profile.h"
#include "config.h"
#include <Arduino.h>

namespace cyd::backlight {

namespace {

// Arduino core 2.0.x channel API (espressif32@6.12.0). Apps must not use channel 0.
constexpr uint8_t kBacklightChannel = 0;
uint8_t currentBrightness = 0;

}  // namespace

void begin() {
  ledcSetup(kBacklightChannel, 5000, 8);
  ledcAttachPin(boardProfile().backlightPin, kBacklightChannel);
  setBrightness(APP_DEFAULT_BRIGHTNESS);
}

void setBrightness(uint8_t value) {
  // Called every loop pass; only touch the LEDC peripheral on a change.
  if (value == currentBrightness) return;
  currentBrightness = value;
  ledcWrite(kBacklightChannel, value);
}

uint8_t brightness() {
  return currentBrightness;
}

}  // namespace cyd::backlight

