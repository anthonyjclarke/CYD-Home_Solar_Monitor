#include "history_service.h"
#include "ha_source.h"
#include "monitor_config.h"
#include "monitor_web.h"
#include "debug.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_partition.h>
#include <ctime>

namespace history {
namespace {
Store store;
Header current;
int activeBank = -1;
bool mounted = false, healthy = false, dirty = false;
RestoreState restored = RestoreState::Starting;
uint32_t lastCheck = 0, lastSave = 0, savedEpoch = 0;
const char* paths[] = {"/history0.bin", "/history1.bin"};

uint32_t identity() {
    const auto& c = monitor::config();
    uint32_t crc = 0xffffffff;
    auto text = [&](const String& s) { for (char ch : s) crc = crcByte(crc, uint8_t(ch)); crc = crcByte(crc, 0); };
    text(c.url);
    for (const auto& entity : c.entities) text(entity);
    for (const auto& entity : c.totalEntities) text(entity);
    // Stable decimal coordinates, including a canonical "off" marker.
    text(std::isnan(c.latitude) ? String("off") : String(c.latitude, 6));
    text(std::isnan(c.longitude) ? String("off") : String(c.longitude, 6));
    return crc ^ 0xffffffff;
}
bool readBank(int bank, Header& h, Store* target = nullptr) {
    File file = LittleFS.open(paths[bank], "r");
    if (!file || file.size() != encodedSize) return false;
    // Buffered reads avoid tens of thousands of filesystem calls per bank.
    uint8_t bytes[512]; size_t used = 0, count = 0;
    return decode([&]() -> int {
        if (used == count) { count = file.read(bytes, sizeof(bytes)); used = 0; }
        return used < count ? bytes[used++] : -1;
    }, h, target);
}
bool erasedPartition() {
    const auto* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, "spiffs");
    if (!p) return false;
    uint8_t bytes[512];
    for (size_t offset = 0; offset < p->size; offset += sizeof(bytes)) {
        if (esp_partition_read(p, offset, bytes, sizeof(bytes)) != ESP_OK) return false;
        for (uint8_t b : bytes) if (b != 0xff) return false;
    }
    return true;
}
}
uint32_t now() { const time_t t = time(nullptr); return t > clockMinimum ? uint32_t(t) : 0; }
const Store& data() { return store; }
bool storageReady() { return mounted; }
bool storageHealthy() { return healthy; }
RestoreState restoreState() { return restored; }
uint32_t savedAt() { return savedEpoch; }

void begin() {
    current.identity = identity(); lastSave = millis();
    mounted = LittleFS.begin(false);
    // Provision only a completely blank partition. Never auto-format corrupt
    // storage: keep the files recoverable and continue with RAM history.
    if (!mounted && erasedPartition()) mounted = LittleFS.format() && LittleFS.begin(false);
    healthy = mounted;
    if (!mounted) { restored = RestoreState::Unavailable; DBG_ERROR("[HISTORY] LittleFS unavailable; RAM history only (no automatic erase)"); return; }
    restored = RestoreState::Empty;
    Header h[2]; bool valid[2] = {readBank(0,h[0]), readBank(1,h[1])};
    // Select newest valid bank before checking identity: an older mapping must
    // never be resurrected just because it matches the current config.
    int best = valid[1] && (!valid[0] || newer(h[1].generation,h[0].generation)) ? 1 : valid[0] ? 0 : -1;
    if (best >= 0) {
        activeBank = best; current.generation = h[best].generation;
        if (h[best].identity == current.identity && readBank(best,h[best],&store)) {
            restored = RestoreState::Restored;
            for (const auto& r : store.records) savedEpoch = std::max(savedEpoch,r.capturedAt);
            DBG_INFO("[HISTORY] Restored checkpoint generation %lu", static_cast<unsigned long>(current.generation));
        } else {
            restored = h[best].identity == current.identity ? RestoreState::Corrupt : RestoreState::MappingChanged;
            if (restored == RestoreState::Corrupt) healthy = false;
            store.clear(); dirty = true;
        }
    } else if (LittleFS.exists(paths[0]) || LittleFS.exists(paths[1])) {
        restored = RestoreState::Corrupt; healthy = false; DBG_WARN("[HISTORY] No valid checkpoint; collecting a new history");
    }
}
bool checkpoint() {
    if (!dirty) return mounted && healthy;
    if (!mounted) return false;
    const int bank = activeBank == 0 ? 1 : 0;
    Header next{current.identity,current.generation+1};
    File file = LittleFS.open(paths[bank], "w");
    if (!file) { healthy = false; return false; }
    uint8_t bytes[512]; size_t used = 0;
    bool ok = encode([&](uint8_t b) {
        bytes[used++] = b;
        if (used != sizeof(bytes)) return true;
        const bool written = file.write(bytes,used) == used; used = 0;
        return written;
    }, store, next);
    if (used) ok = file.write(bytes,used) == used && ok;
    file.flush(); file.close();
    Header checked;
    ok = ok && readBank(bank,checked) && checked.identity == next.identity && checked.generation == next.generation;
    healthy = ok;
    if (!ok) { DBG_ERROR("[HISTORY] Checkpoint failed; previous bank retained"); return false; }
    activeBank = bank; current = next; dirty = false; savedEpoch = 0; lastSave = millis();
    for (const auto& r : store.records) savedEpoch = std::max(savedEpoch,r.capturedAt);
    DBG_INFO("[HISTORY] Saved generation %lu", static_cast<unsigned long>(current.generation));
    return true;
}
void configure() {
    const uint32_t key = identity();
    if (key != current.identity) { store.clear(); current.identity = key; dirty = true; savedEpoch = 0; }
}
void update() {
    const uint32_t ms = millis();
    if (uint32_t(ms-lastCheck) < 1000 || web::updating()) return;
    lastCheck = ms;
    configure();
    const uint32_t epoch = now();
    if (epoch) {
        dirty = store.prune(epoch) || dirty;
        if (!monitor::config().demo && !store.at(bucket(epoch))) {
            const auto s = source::snapshot();
            // Copy first, then evaluate receive ages (the worker runs on core 0).
            const auto& c = monitor::config();
            dirty = store.add(capture(s,WiFi.isConnected(),millis(),epoch,c.staleSeconds,c.pollSeconds)) || dirty;
        }
    }
    // Rate-limit failed attempts too, rather than hammering unhealthy flash.
    if (dirty && uint32_t(ms-lastSave) >= 15*60*1000UL) { lastSave = ms; checkpoint(); }
}
}  // namespace history
