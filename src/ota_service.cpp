#include "ota_service.h"
#include "monitor_config.h"
#include "ha_source.h"
#include "config.h"
#include "debug.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>

namespace cyd::ota {
namespace { Status current = {false, ""}; }
void begin(const String& name) { current.hostname = name; }
void update() {
    // Also enables OTA when Wi-Fi connects after boot/portal timeout.
    if (!current.enabled && WiFi.isConnected()) {
        // ArduinoOTA starts mDNS under this hostname, so solar.local and the
        // OTA service share one responder; add the web server alongside it.
        ArduinoOTA.setHostname(APP_MDNS_NAME);
        ArduinoOTA.setPassword(monitor::accessCode().c_str());
        ArduinoOTA.setPort(APP_OTA_PORT);
        ArduinoOTA.onStart([] { source::pause(true); });
        ArduinoOTA.onError([](ota_error_t) { source::pause(false); });
        ArduinoOTA.begin(); current.enabled = true;
        MDNS.addService("http", "tcp", 80);
        DBG_INFO("[MDNS] http://%s.local/ (OTA port %u)", APP_MDNS_NAME, unsigned(APP_OTA_PORT));
    }
    if (current.enabled) ArduinoOTA.handle();
}
const Status& status() { return current; }
}  // namespace cyd::ota
