#pragma once
#include <stdint.h>

namespace diagnostics {
void begin();
void update(uint32_t now);
}
