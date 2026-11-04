#pragma once
#include <string>
#include <cstdint>
#include <cctype>
#include <cstdio>

// Minimal host stand-in for the String operations used by the HTTP handler.
// Serialization uses the real pinned ArduinoJson ArduinoStringWriter, which
// clears its destination; std::string's ArduinoJson writer behaves differently.
class String {
    std::string value;
public:
    String() = default;
    String(const char* s) : value(s ? s : "") {}
    String& operator=(const char* s) { value = s ? s : ""; return *this; }
    String& operator+=(const char* s) { value += s; return *this; }
    String& operator+=(const String& s) { value += s.value; return *this; }
    bool operator!=(const char* s) const { return value != s; }
    bool concat(const char* s) { value += s; return true; }
    const char* c_str() const { return value.c_str(); }
    size_t length() const { return value.length(); }
    bool isEmpty() const { return value.empty(); }
    char operator[](size_t pos) const { return value[pos]; }
    auto begin() const { return value.begin(); }
    auto end() const { return value.end(); }
    void reserve(size_t n) { value.reserve(n); }
    void remove(size_t pos) { value.erase(pos); }
};

inline bool isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)); }
inline bool isAlphaNumeric(char c) { return std::isalnum(static_cast<unsigned char>(c)); }
inline uint32_t millis() { return 1000; }
inline uint32_t esp_random() { return 0x1234; }
struct HostSerial {
    unsigned calls = 0;
    template<class... Args> void printf(const char*, Args...) { ++calls; }
};
inline HostSerial Serial;
