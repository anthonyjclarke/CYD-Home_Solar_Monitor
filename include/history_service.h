#pragma once
#include "history_model.h"

namespace history {
// All service calls run on the main Arduino task; the HA worker only publishes
// snapshots. No network or filesystem work happens in the renderer.
void begin();
void update();
void configure();
bool checkpoint();
const Store& data();
bool storageReady();
bool storageHealthy();
RestoreState restoreState();
uint32_t savedAt();
uint32_t now();
}  // namespace history
