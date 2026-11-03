#pragma once
#include <Arduino.h>
#include "energy_model.h"
#include "config.h"
#include <cmath>

namespace monitor {
struct Config {
    String url = "http://192.168.1.254:8123";
    String token;
    String entities[energy::SourceCount] = {
        "sensor.solaredge_i1_ac_power", "sensor.home_power_usage",
        "sensor.solaredge_i1_m1_ac_power", "sensor.solaredge_i1_b1_dc_power",
        "sensor.solaredge_i1_b1_state_of_energy", "sensor.solaredge_i1_dc_power"
    };
    uint16_t pollSeconds = 5;
    uint16_t staleSeconds = 180;
    uint8_t brightness = 180;
    uint8_t nightStart = 19;
    uint8_t nightEnd = 7;
    bool nightDim = true;
    bool demo = false;
    String batteryLabel = "LG 9.8 kWh";  // Panel space allows 14 characters.
    uint8_t layout = 1;
    // Optional kWh entities (blank = not shown): a daily solar meter and
    // tomorrow's solar forecast, e.g. from Forecast.Solar or Solcast.
    String totalEntities[energy::TotalCount];
    // Location for weather (NaN = not set) and the local timezone as a POSIX
    // rule; tzName is the Olson name it was chosen from, for display only.
    float latitude = NAN, longitude = NAN;
    String place;
    String tzName = APP_TZ_NAME;
    String tzPosix = APP_TZ_POSIX;                  // ui::Layout: 0 cards, 1 centre flow.
    String caCertificate;
};
void loadConfig();
const Config& config();
bool saveConfig(const Config& next);
const String& accessCode();
bool validTimezone(const String& tz);
}  // namespace monitor

