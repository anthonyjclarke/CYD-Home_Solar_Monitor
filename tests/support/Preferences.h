#pragma once
#include "Arduino.h"
#include <map>

// Models the Preferences read/write contract, not physical NVS reliability.
class Preferences {
    std::string space;
public:
    inline static std::map<std::string,std::map<std::string,std::string>> records;
    inline static bool failWrites = false;
    bool begin(const char* name, bool = false) { space = name; return true; }
    void end() {}
    String getString(const char* key, const char* fallback = "") const {
        const auto& data = records[space];
        const auto found = data.find(key);
        return found == data.end() ? fallback : found->second.c_str();
    }
    uint8_t getUChar(const char* key, uint8_t fallback) const {
        const auto value = getString(key);
        return value.isEmpty() ? fallback : uint8_t(std::stoul(value.c_str()));
    }
    size_t putString(const char* key, const String& value) {
        if (failWrites) return 0;
        records[space][key] = value.c_str(); return value.length();
    }
};
