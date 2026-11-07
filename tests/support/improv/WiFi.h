#pragma once
#include <Arduino.h>
#include <cstdint>

// Host stand-in for the parts of the ESP32 WiFi API the Improv library calls.
// The test connects through setCustomConnectWiFi(), so these are never asked
// to join a network; they only report a fixed state.
enum { WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum { WIFI_AUTH_OPEN = 0 };
struct IPAddress {
    uint8_t octets[4] = {192, 168, 1, 50};
    uint8_t operator[](int i) const { return octets[i]; }
};
struct HostWiFi {
    bool connected = false;
    int status() const { return connected ? WL_CONNECTED : WL_DISCONNECTED; }
    IPAddress localIP() const { return {}; }
    void disconnect() { connected = false; }
    void begin(const char*, const char*) {}
    int scanNetworks() { return 0; }
    String SSID(int) const { return String(""); }
    int RSSI(int) const { return 0; }
    int encryptionType(int) const { return WIFI_AUTH_OPEN; }
};
inline HostWiFi WiFi;
inline void delay(uint32_t) {}
