#pragma once
#include <stdint.h>
#include "boot_status.h"

namespace diagnostics {
void begin();
void update(uint32_t now);
void bootStage(const char* stage, const char* detail, bool problem = false);
const boot::Report& bootStatus();
void bootScreenClosed(const char* reason);
}
