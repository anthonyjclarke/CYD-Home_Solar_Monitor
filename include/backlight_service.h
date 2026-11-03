#pragma once

#include <stdint.h>

namespace cyd::backlight {

void begin();
void setBrightness(uint8_t value);
uint8_t brightness();

}  // namespace cyd::backlight

