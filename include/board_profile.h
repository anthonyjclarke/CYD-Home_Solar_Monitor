#pragma once

#include "config.h"
#include <Arduino.h>

namespace cyd {

struct BoardProfile {
  const char* key;
  const char* name;
  const char* driver;
  uint16_t nativeWidth;
  uint16_t nativeHeight;
  uint16_t viewWidth;
  uint16_t viewHeight;
  uint8_t rotation;
  int8_t backlightPin;
  bool hasTouch;
  bool hasRgbLed;
  bool hasLdr;
  bool rgbActiveLow;
  int8_t ledRPin;
  int8_t ledGPin;
  int8_t ledBPin;
  int8_t ldrPin;
  // Touch calibration is applied after any board-specific raw axis transform.
  int16_t touchXMin;
  int16_t touchXMax;
  int16_t touchYMin;
  int16_t touchYMax;
};

inline const BoardProfile& boardProfile() {
  static const BoardProfile profile = {
    CYD_BOARD_KEY,
    CYD_BOARD_NAME,
    CYD_DRIVER_NAME,
    CYD_NATIVE_WIDTH,
    CYD_NATIVE_HEIGHT,
    CYD_EXPECTED_WIDTH,
    CYD_EXPECTED_HEIGHT,
    CYD_LANDSCAPE_ROT,
    TFT_BL_PIN,
    true,
    CYD_HAS_RGB_LED != 0,
    CYD_HAS_LDR != 0,
    CYD_RGB_ACTIVE_LOW != 0,
    LED_R_PIN,
    LED_G_PIN,
    LED_B_PIN,
    LDR_PIN,
    TOUCH_X_MIN,
    TOUCH_X_MAX,
    TOUCH_Y_MIN,
    TOUCH_Y_MAX,
  };
  return profile;
}

}  // namespace cyd
