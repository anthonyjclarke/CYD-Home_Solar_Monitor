// Host checks for the vendored Improv serial parser (audit F1). Built with
// AddressSanitizer and UndefinedBehaviorSanitizer by tools/check_host.py, so
// an out-of-bounds read or write fails the run rather than passing silently.
#include <ImprovWiFiLibrary.h>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static int connects = 0;
static std::string lastSsid, lastPassword;
static bool connectOk(const char* ssid, const char* password) {
    ++connects; lastSsid = ssid; lastPassword = password; return true;
}
static int callbackError = -1;
static void onError(ImprovTypes::Error error) { callbackError = error; }

static std::vector<uint8_t> frame(uint8_t type, const std::vector<uint8_t>& payload, int lengthOverride = -1) {
    std::vector<uint8_t> f = {'I', 'M', 'P', 'R', 'O', 'V', ImprovTypes::IMPROV_SERIAL_VERSION, type,
                              static_cast<uint8_t>(lengthOverride < 0 ? payload.size() : lengthOverride)};
    f.insert(f.end(), payload.begin(), payload.end());
    uint8_t sum = 0;
    for (uint8_t b : f) sum += b;
    f.push_back(sum);
    return f;
}
static std::vector<uint8_t> wifi(const std::string& ssid, const std::string& password) {
    std::vector<uint8_t> rpc = {ImprovTypes::WIFI_SETTINGS, static_cast<uint8_t>(ssid.size() + password.size() + 2),
                                static_cast<uint8_t>(ssid.size())};
    rpc.insert(rpc.end(), ssid.begin(), ssid.end());
    rpc.push_back(static_cast<uint8_t>(password.size()));
    rpc.insert(rpc.end(), password.begin(), password.end());
    return frame(ImprovTypes::TYPE_RPC, rpc);
}

struct Harness {
    Stream serial;
    ImprovWiFi improv{&serial};
    Harness() {
        improv.setDeviceInfo(ImprovTypes::CF_ESP32, "CYD-Solar", "0.9.1", "Solar Monitor");
        improv.setCustomConnectWiFi(connectOk);
        improv.onImprovError(onError);
    }
    void feed(const std::vector<uint8_t>& bytes) {
        serial.input.insert(serial.input.end(), bytes.begin(), bytes.end());
        while (serial.available()) improv.handleSerial();
    }
    // The error code of the most recent ERROR_STATE frame written, or -1.
    int lastError() const {
        const auto& o = serial.output;
        int error = -1;
        for (size_t i = 0; i + 10 < o.size(); ++i)
            if (o[i] == 'I' && o[i + 1] == 'M' && o[i + 7] == ImprovTypes::TYPE_ERROR_STATE) error = o[i + 9];
        return error;
    }
    bool wrote(const std::string& text) const {
        return std::string(serial.output.begin(), serial.output.end()).find(text) != std::string::npos;
    }
};

static void expectRejected(const std::vector<uint8_t>& rpc, int error, const char* name) {
    Harness h;
    // A long valid frame first leaves stale bytes in the receive buffer that
    // an unbounded parser would read as SSID/password data.
    h.feed(wifi("stale-network-name-xxxxxxxx", "stale-password-yyyyyyyy"));
    const int before = connects;
    h.serial.output.clear();
    h.feed(frame(ImprovTypes::TYPE_RPC, rpc));
    if (connects != before || h.lastError() != error) {
        std::fprintf(stderr, "%s: connects %d->%d, error %d (want %d)\n", name, before, connects, h.lastError(), error);
        assert(false);
    }
    // The parser recovers: the next valid frame is handled.
    h.feed(wifi("home", "pw"));
    assert(connects == before + 1 && lastSsid == "home" && lastPassword == "pw");
}

int main() {
    {
        Harness h;
        h.feed(wifi("home", "secret"));
        assert(connects == 1 && lastSsid == "home" && lastPassword == "secret");
        h.feed(wifi("open-net", ""));  // Empty password is valid provisioning.
        assert(connects == 2 && lastSsid == "open-net" && lastPassword.empty());
        h.feed(frame(ImprovTypes::TYPE_RPC, {ImprovTypes::GET_DEVICE_INFO, 0}));
        assert(h.wrote("CYD-Solar") && h.wrote("Solar Monitor"));
    }
    {
        // Longest payload that fits: 9 header bytes + 119 = 128-byte buffer.
        Harness h;
        const std::string ssid(32, 's'), password(119 - 2 - 2 - 32, 'p');
        h.feed(wifi(ssid, password));
        assert(lastSsid == ssid && lastPassword == password);
    }
    {
        // Oversized advertised lengths are rejected before any payload byte
        // is stored, and a valid frame straight after is still accepted.
        Harness h;
        const int before = connects;
        h.feed(frame(ImprovTypes::TYPE_RPC, std::vector<uint8_t>(255, 0x41)));
        h.feed(frame(ImprovTypes::TYPE_RPC, std::vector<uint8_t>(120, 0x41)));
        assert(connects == before);
        h.feed(wifi("after", "big"));
        assert(connects == before + 1 && lastSsid == "after");
    }
    {
        Harness h;
        auto bad = wifi("home", "secret");
        bad.back() ^= 0xFF;
        const int before = connects;
        callbackError = -1;
        h.feed(bad);  // Reported to the callback only; no reply frame is sent.
        assert(connects == before && callbackError == ImprovTypes::ERROR_INVALID_RPC);
        h.feed(wifi("home", "secret"));
        assert(connects == before + 1);
    }
    using namespace ImprovTypes;
    expectRejected({}, ERROR_UNKNOWN_RPC, "empty payload");
    expectRejected({WIFI_SETTINGS}, ERROR_UNKNOWN_RPC, "command only");
    expectRejected({WIFI_SETTINGS, 0}, ERROR_INVALID_RPC, "no SSID length");
    expectRejected({WIFI_SETTINGS, 1, 4}, ERROR_INVALID_RPC, "SSID length only");
    expectRejected({WIFI_SETTINGS, 3, 4, 'h', 'o'}, ERROR_INVALID_RPC, "SSID truncated");
    expectRejected({WIFI_SETTINGS, 5, 4, 'h', 'o', 'm', 'e'}, ERROR_INVALID_RPC, "no password length");
    expectRejected({WIFI_SETTINGS, 7, 4, 'h', 'o', 'm', 'e', 3, 'p'}, ERROR_INVALID_RPC, "password truncated");
    expectRejected({WIFI_SETTINGS, 6, 0, 0, 0, 0, 0, 0}, ERROR_INVALID_RPC, "empty SSID");
    std::puts("Improv parser checks passed.");
}
