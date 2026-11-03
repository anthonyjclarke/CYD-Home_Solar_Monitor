#pragma once

#include <stdint.h>

namespace cyd::touch {

struct State {
  bool available;
  bool pressed;
  uint16_t x;
  uint16_t y;
  uint16_t rawX;
  uint16_t rawY;
  uint16_t z;
};

void begin(uint16_t screenWidth, uint16_t screenHeight);
void update();
const State& state();
bool calibrate(uint16_t x0, uint16_t x1, uint16_t y0, uint16_t y1);

}  // namespace cyd::touch

