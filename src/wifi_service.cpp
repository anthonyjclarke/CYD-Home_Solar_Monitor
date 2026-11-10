#include "wifi_service.h"
#include "wifi_lifecycle.h"
#include "monitor_config.h"
#include "storage_service.h"
#include <WiFi.h>
#include <WiFiManager.h>

namespace cyd::network {
namespace {
WiFiManager manager;
Status current = {false, false, "", "", ""};
Lifecycle lifecycle;

void startPortal() {
    manager.setConfigPortalBlocking(false);
    manager.setConfigPortalTimeout(180);
    manager.setConnectTimeout(10);
    manager.startConfigPortal(current.apName.c_str(), monitor::accessCode().c_str());
}
}
void begin() {
    current.apName = "CYD-Solar-" + String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xffff), HEX);
    WiFi.mode(WIFI_STA);
    // Mains-powered: modem sleep made pings swing 40-450 ms and page loads 0.3-6 s.
    WiFi.setSleep(false);
    WiFi.setHostname(storage::settings().deviceName.c_str());
    WiFi.setAutoReconnect(true);
    WiFi.begin();
    lifecycle.begin(millis());
}
void update() {
    manager.process();
    current.connected = WiFi.isConnected();
    current.portalActive = manager.getConfigPortalActive();
    current.ssid = current.connected ? WiFi.SSID() : "offline";
    current.ip = current.connected ? WiFi.localIP().toString() : "-";
    switch (lifecycle.update(millis(), current.connected, current.portalActive)) {
        case Action::OpenPortal: startPortal(); break;
        case Action::ClosePortal: manager.stopConfigPortal(); break;
        case Action::Reconnect: WiFi.reconnect(); break;
        case Action::None: break;
    }
    current.portalActive = manager.getConfigPortalActive();
}
const Status& status() { return current; }
}  // namespace cyd::network

