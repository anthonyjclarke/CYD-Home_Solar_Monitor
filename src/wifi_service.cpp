#include "wifi_service.h"
#include "monitor_config.h"
#include "storage_service.h"
#include <WiFi.h>
#include <WiFiManager.h>

namespace cyd::network {
namespace {
WiFiManager manager;
Status current = {false, false, "", "", ""};
uint32_t started = 0, lastAttempt = 0;
bool portalStarted = false;
bool everConnected = false;

void startPortal() {
    manager.setConfigPortalBlocking(false);
    manager.setConfigPortalTimeout(180);
    manager.setConnectTimeout(10);
    manager.startConfigPortal(current.apName.c_str(), monitor::accessCode().c_str());
    portalStarted = true;
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
    started = lastAttempt = millis();
}
void update() {
    manager.process();
    current.connected = WiFi.isConnected();
    if (current.connected) everConnected = true;
    current.portalActive = manager.getConfigPortalActive();
    current.ssid = current.connected ? WiFi.SSID() : "offline";
    current.ip = current.connected ? WiFi.localIP().toString() : "-";
    if (current.connected && current.portalActive) {
        manager.stopConfigPortal(); current.portalActive = false;
    }
    if (!current.connected && !everConnected && !portalStarted && millis() - started >= 20000) startPortal();
    if (!current.connected && !current.portalActive && millis() - lastAttempt >= 30000) {
        lastAttempt = millis(); WiFi.reconnect();
    }
    // The timed-out setup portal is reopened after a bounded retry interval.
    if (!current.connected && !current.portalActive && portalStarted && millis() - lastAttempt >= 20000) {
        startPortal(); lastAttempt = millis();
    }
}
const Status& status() { return current; }
}  // namespace cyd::network

