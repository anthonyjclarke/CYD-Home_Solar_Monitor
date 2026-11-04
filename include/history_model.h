#pragma once
#include "energy_model.h"
#include <algorithm>

// Portable history rules and wire format. No Arduino or filesystem dependency.
namespace history {
enum class RestoreState : uint8_t { Starting, Empty, Restored, MappingChanged, Corrupt, Unavailable };
constexpr uint32_t interval = 300, duration = 86400, capacity = duration / interval;
constexpr uint32_t clockMinimum = 1700000000;
enum Field : uint8_t {
    Inverter = energy::Inverter, Home = energy::Home, Grid = energy::Grid,
    Battery = energy::Battery, Soc = energy::Soc, InverterDc = energy::InverterDc,
    Solar = energy::Solar, Today = energy::Count, Tomorrow, Temperature, WeatherCode,
    TomorrowTemperature, TomorrowCode, FieldCount
};
constexpr uint16_t allFields = (1U << FieldCount) - 1;
constexpr Field tabs[] = {Solar, Home, Grid, Battery, Soc};
constexpr Field order[] = {Solar, Home, Grid, Battery, Soc, Inverter, InverterDc,
                          Today, Tomorrow, Temperature, WeatherCode, TomorrowTemperature, TomorrowCode};
inline const char* name(unsigned field) {
    const char* names[] = {"Inverter AC", "Home load", "Grid (+import)", "Battery (+discharge)",
        "Battery level", "Inverter DC", "Solar generation", "Solar today", "Solar forecast",
        "Temperature", "Weather code", "Tomorrow max", "Tomorrow sky code"};
    return field < FieldCount ? names[field] : "";
}
inline const char* unit(unsigned field) {
    return field == Soc ? "%" : field < energy::Count ? "W" : field <= Tomorrow ? "kWh" :
           field == Temperature || field == TomorrowTemperature ? "C" : "code";
}
inline uint32_t bucket(uint32_t epoch) { return epoch - epoch % interval; }
struct Record {
    uint32_t slot = 0, capturedAt = 0;
    uint16_t valid = 0, sourceTimeKnown = 0;
    float values[FieldCount] = {};
    bool has(unsigned field) const { return field < FieldCount && (valid & (1U << field)); }
};
inline Record capture(const energy::Snapshot& s, bool online, uint32_t ms, uint32_t epoch,
                      uint32_t stale, uint32_t poll) {
    Record out;
    if (epoch <= clockMinimum || s.demo) return out;
    out.slot = bucket(epoch); out.capturedAt = epoch;
    for (unsigned i = 0; i < energy::Count; ++i) {
        if (!energy::metricUsable(s, static_cast<energy::Metric>(i), online, ms, uint64_t(epoch)*1000, stale, poll) ||
            !std::isfinite(s.readings[i].value)) continue;
        out.valid |= 1U << i; out.values[i] = s.readings[i].value;
        if (s.readings[i].reportedMs) out.sourceTimeKnown |= 1U << i;
    }
    for (unsigned i = 0; i < energy::TotalCount; ++i) {
        if (!energy::totalUsable(s, static_cast<energy::Total>(i), online, ms) || !std::isfinite(s.totals[i].value)) continue;
        out.valid |= 1U << (Today+i); out.values[Today+i] = s.totals[i].value;
    }
    if (online && weather::usable(s.weather, ms)) {
        const float values[] = {s.weather.nowC, float(s.weather.nowCode), s.weather.tomorrowMaxC, float(s.weather.tomorrowCode)};
        for (unsigned i = 0; i < 4; ++i) if (std::isfinite(values[i])) {
            out.valid |= 1U << (Temperature+i); out.values[Temperature+i] = values[i];
        }
    }
    return out;
}
class Store {
public:
    Record records[capacity];
    void clear() { for (auto& r : records) r = Record(); }
    const Record* at(uint32_t slot) const {
        const auto& r = records[(slot / interval) % capacity];
        return r.slot == slot && r.valid ? &r : nullptr;
    }
    // A clock correction never leaves points from the future on a graph.
    bool prune(uint32_t epoch) {
        bool changed = false;
        const uint32_t first = bucket(epoch) - (capacity-1)*interval;
        for (auto& r : records) if (r.slot && (r.slot < first || r.capturedAt > epoch)) {
            r = Record(); changed = true;
        }
        return changed;
    }
    bool add(const Record& r) {
        if (!r.valid || r.capturedAt <= clockMinimum || r.slot != bucket(r.capturedAt) || at(r.slot)) return false;
        prune(r.capturedAt);
        records[(r.slot / interval) % capacity] = r;
        return true;
    }
};
struct Stats {
    unsigned count = 0;
    float minimum = 0, maximum = 0, mean = 0, latest = 0;
    uint32_t latestAt = 0;
};
inline Stats stats(const Store& store, unsigned field, uint32_t epoch) {
    Stats out;
    if (epoch <= clockMinimum) return out;
    const uint32_t first = bucket(epoch) - (capacity-1)*interval;
    double sum = 0;
    for (uint32_t slot = first; slot <= bucket(epoch); slot += interval) {
        const auto* r = store.at(slot);
        if (!r || r->capturedAt > epoch || !r->has(field)) continue;
        const float v = r->values[field];
        if (!out.count) out.minimum = out.maximum = v;
        out.minimum = std::min(out.minimum, v); out.maximum = std::max(out.maximum, v);
        ++out.count; sum += v; out.latest = v; out.latestAt = r->capturedAt;
    }
    if (out.count) out.mean = float(sum / out.count);
    return out;
}

// CRC32 also covers header/identity/generation. Explicit LE fields avoid struct
// padding, compiler and float aliasing dependencies in persisted files.
inline uint32_t crcByte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (unsigned i = 0; i < 8; ++i) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
    return crc;
}
constexpr uint32_t magic = 0x31485943, format = 1;
constexpr size_t encodedSize = 24 + capacity * (12 + 4 * FieldCount) + 4;
struct Header { uint32_t identity = 0, generation = 0; };
inline bool newer(uint32_t a, uint32_t b) { return int32_t(a-b) > 0; }
template<class Writer> bool encode(Writer write, const Store& store, Header header) {
    uint32_t crc = 0xffffffff; bool ok = true;
    auto wireNumber = [&](uint32_t value, unsigned bytes = 4) {
        for (unsigned i = 0; i < bytes; ++i) { uint8_t b = value >> (i*8); crc = crcByte(crc, b); ok = write(b) && ok; }
    };
    wireNumber(magic); wireNumber(format); wireNumber(capacity); wireNumber(FieldCount); wireNumber(header.identity); wireNumber(header.generation);
    for (const auto& r : store.records) {
        wireNumber(r.slot); wireNumber(r.capturedAt); wireNumber(r.valid, 2); wireNumber(r.sourceTimeKnown, 2);
        for (float v : r.values) { uint32_t bits; static_assert(sizeof(v) == sizeof(bits), "32-bit floats required"); std::memcpy(&bits, &v, 4); wireNumber(bits); }
    }
    crc ^= 0xffffffff;
    for (unsigned i = 0; i < 4; ++i) ok = write(uint8_t(crc >> (i*8))) && ok;
    return ok;
}
// Pass nullptr to verify a bank with only one Record of scratch space. A
// verified bank is read a second time to load it during boot.
template<class Reader> bool decode(Reader read, Header& header, Store* store = nullptr) {
    uint32_t crc = 0xffffffff; bool ok = true;
    auto wireNumber = [&](unsigned bytes = 4) {
        uint32_t v = 0;
        for (unsigned i = 0; i < bytes; ++i) { int b = read(); if (b < 0) { ok = false; b = 0; } crc = crcByte(crc, uint8_t(b)); v |= uint32_t(uint8_t(b)) << (8*i); }
        return v;
    };
    const auto m = wireNumber(), f = wireNumber(), n = wireNumber(), fields = wireNumber();
    if (!ok || m != magic || f != format || n != capacity || fields != FieldCount) return false;
    Header next; next.identity = wireNumber(); next.generation = wireNumber();
    for (unsigned i = 0; i < capacity; ++i) {
        Record r; r.slot = wireNumber(); r.capturedAt = wireNumber(); r.valid = wireNumber(2); r.sourceTimeKnown = wireNumber(2);
        if ((r.valid & ~allFields) || (r.sourceTimeKnown & ~r.valid)) return false;
        for (auto& v : r.values) { uint32_t bits = wireNumber(); std::memcpy(&v, &bits, 4); if (!std::isfinite(v)) return false; }
        if (r.slot && (r.capturedAt <= clockMinimum || r.slot != bucket(r.capturedAt) || (r.slot/interval)%capacity != i)) return false;
        if (!r.slot && (r.valid || r.capturedAt || r.sourceTimeKnown)) return false;
        if (store) store->records[i] = r;
    }
    const uint32_t expected = crc ^ 0xffffffff;
    uint32_t saved = 0;
    for (unsigned i = 0; i < 4; ++i) { int b = read(); if (b < 0) return false; saved |= uint32_t(uint8_t(b)) << (8*i); }
    if (!ok || saved != expected || read() != -1) return false;
    header = next; return true;
}
}  // namespace history
