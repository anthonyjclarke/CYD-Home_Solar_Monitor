#pragma once
#include "energy_model.h"

namespace diagnostics {
inline const char* metricName(energy::Metric metric) {
    const char* names[] = {"inverter_ac", "home", "grid_import_signed", "battery_discharge_signed",
                           "battery_soc", "inverter_dc", "solar"};
    return metric < energy::Count ? names[metric] : "unknown";
}

// Never echo arbitrary HA strings. Values are numeric only; units and state
// descriptions come from fixed vocabulary so response text cannot leak secrets.
inline const char* safeUnit(const char* unit) {
    if (!unit) return "missing";
    if (!std::strcmp(unit,"W") || !std::strcmp(unit,"Watt") || !std::strcmp(unit,"Watts") ||
        !std::strcmp(unit,"watt") || !std::strcmp(unit,"watts")) return "W";
    if (!std::strcmp(unit,"kW")) return "kW";
    if (!std::strcmp(unit,"%")) return "%";
    if (!std::strcmp(unit,"Wh")) return "Wh";
    if (!std::strcmp(unit,"kWh")) return "kWh";
    return *unit ? "unexpected" : "missing";
}

inline const char* stateKind(const char* state, float& number, bool& numeric) {
    numeric = false;
    if (!state || !*state) return "missing";
    if (!std::strcmp(state,"unknown")) return "unknown";
    if (!std::strcmp(state,"unavailable")) return "unavailable";
    char* end = nullptr;
    const double n = std::strtod(state,&end);
    if (end == state) return "invalid";
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end || !std::isfinite(n) || std::abs(n)>1e9) return "invalid";
    number = static_cast<float>(n); numeric = true;
    return "numeric";
}

inline const char* readingReason(const energy::Reading& r,uint32_t now,uint64_t epoch,uint32_t stale) {
    if (!r.valid) return "invalid";
    if (uint32_t(now-r.receivedMs)>stale*1000UL) return "receive-stale";
    if (r.reportedMs && epoch) {
        if (r.reportedMs>epoch+60000) return "report-in-future";
        if (epoch>r.reportedMs && epoch-r.reportedMs>stale*1000ULL) return "report-stale";
    }
    return r.reportedMs && epoch ? "fresh" : "source-age-unknown";
}

inline const char* solarReason(const energy::Snapshot& s,uint32_t now,uint64_t epoch,uint32_t stale,uint32_t maxSkew) {
    if (!s.readings[energy::InverterDc].valid) return "dc-invalid";
    if (!s.readings[energy::Battery].valid) return "battery-invalid";
    if (!energy::fresh(s.readings[energy::InverterDc],now,epoch,stale)) return "dc-not-fresh";
    if (!energy::fresh(s.readings[energy::Battery],now,epoch,stale)) return "battery-not-fresh";
    if (!energy::solarSourcesCoherent(s,now,maxSkew)) return "input-timestamp-skew";
    return readingReason(s.readings[energy::Solar],now,epoch,stale);
}
}  // namespace diagnostics
