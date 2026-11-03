#include "storage_service.h"

#include "board_profile.h"
#include "config.h"
#include <Preferences.h>

namespace cyd::storage {

namespace {

Preferences prefs;
Settings current;

String defaultDeviceName() {
  const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFF);
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "cyd-solar-%04X", suffix);
  return String(buffer);
}

}  // namespace

void begin() {
  prefs.begin("cyd-core", false);

  current.bootCount = prefs.getUInt("boot_count", 0) + 1;
  prefs.putUInt("boot_count", current.bootCount);

  current.deviceName = prefs.getString("device_name", "");
  if (current.deviceName.isEmpty()) {
    current.deviceName = defaultDeviceName();
    prefs.putString("device_name", current.deviceName);
  }

  current.debugLevel = prefs.getUChar("debug_level", APP_DEFAULT_DEBUG_LEVEL);
}

const Settings& settings() {
  return current;
}

void setDebugLevel(uint8_t level) {
  current.debugLevel = level;
  prefs.putUChar("debug_level", level);
}

}  // namespace cyd::storage

