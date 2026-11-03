#include "ota_service.h"
#include "monitor_config.h"
#include "ha_source.h"
#include "config.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

namespace cyd::ota {
namespace { Status current = {false, ""}; }
void begin(const String& name) { current.hostname = name; }
void update() {
    // Also enables OTA when Wi-Fi connects after boot/portal timeout.
    if (!current.enabled && WiFi.isConnected()) {
        ArduinoOTA.setHostname(current.hostname.c_str());
        ArduinoOTA.setPassword(monitor::accessCode().c_str());
        ArduinoOTA.setPort(APP_OTA_PORT);
        ArduinoOTA.onStart([] { source::pause(true); });
        ArduinoOTA.onError([](ota_error_t) { source::pause(false); });
        ArduinoOTA.begin(); current.enabled = true;
    }
    if (current.enabled) ArduinoOTA.handle();
}
const Status& status() { return current; }
}  // namespace cyd::ota
