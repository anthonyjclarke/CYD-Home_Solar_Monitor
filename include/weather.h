#pragma once
#include <cstdint>

// Open-Meteo weather for the configured location. Host-safe: no Arduino types.
namespace weather {
enum Sky : uint8_t { Unknown, Clear, PartCloud, Cloud, Fog, Drizzle, Rain, Snow, Storm };

// WMO weather interpretation codes, as used by Open-Meteo.
inline Sky skyFor(int code) {
    if (code == 0) return Clear;
    if (code == 1 || code == 2) return PartCloud;
    if (code == 3) return Cloud;
    if (code == 45 || code == 48) return Fog;
    if (code >= 51 && code <= 57) return Drizzle;
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return Rain;
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return Snow;
    if (code >= 95 && code <= 99) return Storm;
    return Unknown;
}

struct Report {
    bool valid = false;
    uint32_t receivedMs = 0;
    float nowC = 0;
    int nowCode = -1;
    float tomorrowMaxC = 0;
    int tomorrowCode = -1;
};

constexpr uint32_t refreshSeconds = 30 * 60, retrySeconds = 5 * 60;
// Forecasts change slowly; hide them only after several missed refreshes.
constexpr uint32_t staleSeconds = 3 * 60 * 60;
inline bool usable(const Report& r, uint32_t now) {
    return r.valid && static_cast<uint32_t>(now - r.receivedMs) <= staleSeconds * 1000UL;
}
}  // namespace weather
