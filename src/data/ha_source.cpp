#include "ha_source.h"
#include "diagnostic_helpers.h"
#include "debug.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <ctime>
#include <algorithm>

namespace source {
namespace {
SemaphoreHandle_t mutex = nullptr;
monitor::Config settings;
energy::Snapshot latest;
CycleTrace latestTrace;
uint32_t revision = 0;
bool paused = false;
bool running = false;

// One keep-alive connection serves all entity reads in a cycle, so the six
// readings arrive close together instead of over six separate handshakes.
struct Connection {
    WiFiClient plain;
    WiFiClientSecure tls;
    HTTPClient http;
    bool secure;
    explicit Connection(const monitor::Config& c) : secure(c.url.startsWith("https://")) {
        if (secure) { tls.setCACert(c.caCertificate.c_str()); tls.setHandshakeTimeout(4); }
        http.setConnectTimeout(1500);
        http.setTimeout(1500);
        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setReuse(true);  // HTTP/1.1; the body is read with getString(), which handles chunking.
    }
    ~Connection() { close(); }
    // Drop the socket after a failed request so an unread body cannot leak
    // into the next response.
    void close() { if (secure) tls.stop(); else plain.stop(); http.end(); }
};

bool readEntity(const monitor::Config& c, Connection& conn, energy::Metric metric,
                energy::Reading& reading, char* message, size_t messageSize, RequestTrace& trace) {
    struct Timer {
        RequestTrace& trace;
        uint32_t start = millis();
        ~Timer() { trace.elapsedMs = millis()-start; }
    } timer{trace};
    reading.valid = false;
    if (!WiFi.isConnected()) {
        trace.error = ReadError::Wifi;
        snprintf(message, messageSize, "WiFi disconnected"); return false;
    }
    if (conn.secure) {
        if (c.caCertificate.isEmpty()) {
            trace.error = ReadError::MissingCa;
            snprintf(message, messageSize, "HTTPS needs a CA certificate"); return false;
        }
        if (time(nullptr) < 1700000000) {
            trace.error = ReadError::Clock;
            snprintf(message, messageSize, "Waiting for clock / TLS"); return false;
        }
    }
    HTTPClient& http = conn.http;
    String url = c.url + "/api/states/" + c.entities[metric];
    const bool opened = conn.secure ? http.begin(conn.tls, url) : http.begin(conn.plain, url);
    if (!opened) { trace.error=ReadError::Url; snprintf(message, messageSize, "Invalid Home Assistant URL"); return false; }
    http.addHeader("Authorization", "Bearer " + c.token);
    http.addHeader("Accept", "application/json");
    const int status = http.GET();
    trace.httpStatus = status;
    if (status != 200) {
        trace.error = ReadError::Http;
        snprintf(message, messageSize, "HA HTTP %d / reading %u", status, static_cast<unsigned>(metric + 1));
        conn.close(); return false;
    }
    if (http.getSize() > 16384) {
        trace.error = ReadError::TooLarge;
        snprintf(message, messageSize, "Entity response too large"); conn.close(); return false;
    }
    const String body = http.getString();
    if (body.length() > 16384) {
        trace.error = ReadError::TooLarge;
        snprintf(message, messageSize, "Entity response too large"); conn.close(); return false;
    }
    JsonDocument filter;
    filter["state"] = true;
    filter["attributes"]["unit_of_measurement"] = true;
    filter["last_reported"] = true;
    JsonDocument doc;
    const auto error = deserializeJson(doc, body, DeserializationOption::Filter(filter),
                                      DeserializationOption::NestingLimit(12));
    if (error) {
        trace.error = ReadError::Json;
        snprintf(message, messageSize, "Invalid entity JSON / reading %u", static_cast<unsigned>(metric + 1));
        conn.close(); return false;
    }
    const char* state = doc["state"] | "";
    const char* unit = doc["attributes"]["unit_of_measurement"] | "";
    trace.state = diagnostics::stateKind(state,trace.rawValue,trace.numeric);
    trace.unit = diagnostics::safeUnit(unit);
    float value = 0;
    if (!energy::parseValue(state, unit, metric, value)) {
        // The body was fully read, so the connection stays usable.
        trace.error = ReadError::Value;
        snprintf(message, messageSize, "Unavailable or wrong unit / reading %u", static_cast<unsigned>(metric + 1));
        http.end(); return false;
    }
    reading = {value, true, millis(), energy::parseTimestamp(doc["last_reported"] | "")};
    trace.error = ReadError::None;
    http.end();
    return true;
}

void worker(void*) {
    uint32_t seenRevision = 0, lastCycle = 0, waitMs = 0;
    unsigned failures = 0;
    uint32_t cycleId = 0;
    monitor::Config c;
    for (;;) {
        energy::Snapshot s;
        uint32_t rev;
        bool hold;
        xSemaphoreTake(mutex, portMAX_DELAY);
        rev = revision; hold = paused;
        // Settings hold the token and CA (up to 8 KB); copy them only when they
        // change rather than allocating on every 100 ms tick.
        if (rev != seenRevision) c = settings;
        xSemaphoreGive(mutex);
        const uint32_t now = millis();
        if (!hold && (rev != seenRevision || static_cast<uint32_t>(now - lastCycle) >= waitMs)) {
            CycleTrace trace;
            seenRevision = rev;
            xSemaphoreTake(mutex, portMAX_DELAY);
            s = latest;
            xSemaphoreGive(mutex);
            if (c.demo) {
                trace.mode=SourceMode::Demo;
                s = energy::demoSnapshot(now, (now / 20000) % 4);
                waitMs = 1000;
            } else if (c.token.isEmpty()) {
                trace.mode=SourceMode::TokenMissing;
                s = energy::Snapshot();
                snprintf(s.message, sizeof(s.message), "Configure Home Assistant token");
                waitMs = 1000;
            } else {
                // Preserve last numeric values for a grey last-known display,
                // but invalidate each failed field immediately.
                s.demo = false;
                unsigned valid = 0;
                char error[80] = "";
                uint64_t oldest = UINT64_MAX, newest = 0;
                Connection conn(c);
                for (int i = 0; i < energy::SourceCount; ++i) {
                    char fieldError[80] = "";
                    if (readEntity(c, conn, static_cast<energy::Metric>(i), s.readings[i], fieldError, sizeof(fieldError),trace.requests[i])) {
                        ++valid;
                        if (s.readings[i].reportedMs) {
                            oldest = std::min(oldest, s.readings[i].reportedMs);
                            newest = std::max(newest, s.readings[i].reportedMs);
                        }
                    } else if (!*error) snprintf(error, sizeof(error), "%s", fieldError);
                    // Configuration or update changes cancel the remaining requests.
                    xSemaphoreTake(mutex, portMAX_DELAY);
                    bool cancel = revision != rev || paused;
                    xSemaphoreGive(mutex);
                    if (cancel) break;
                }
                s.skewSeconds = newest && oldest != UINT64_MAX ? (newest - oldest) / 1000 : 0;
                conn.close();
                energy::deriveSolar(s, millis(), energy::skewLimitSeconds(c.pollSeconds));
                snprintf(s.message, sizeof(s.message), "%s", valid != energy::SourceCount ? error :
                         !s.readings[energy::Solar].valid ? "Solar inputs have different timestamps" : "Home Assistant connected");
                failures = valid ? 0 : std::min(failures + 1, 4U);
                waitMs = std::min<uint32_t>(c.pollSeconds * 1000UL * (1UL << failures), 60000UL);
                if (!failures) waitMs = c.pollSeconds * 1000UL;
                s.cycleMs = millis();
            }
            s.revision = rev;
            trace.id=++cycleId; trace.elapsedMs=millis()-now; trace.nextPollMs=waitMs;
            xSemaphoreTake(mutex, portMAX_DELAY);
            if (revision == rev && !paused) { latest = s; latestTrace=trace; }
            xSemaphoreGive(mutex);
            lastCycle = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
}  // namespace

void begin() {
    mutex = xSemaphoreCreateMutex();
    if (!mutex) { DBG_ERROR("[HA] Could not create source mutex"); return; }
    settings = monitor::config(); revision = 1;
    running = xTaskCreatePinnedToCore(worker, "ha-reader", 12288, nullptr, 1, nullptr, 0) == pdPASS;
    if (!running) snprintf(latest.message, sizeof(latest.message), "HA task could not start");
    if (!running) DBG_ERROR("[HA] Reader task could not start");
    else DBG_INFO("[HA] Background reader started; %u source entities",unsigned(energy::SourceCount));
}

void configure(const monitor::Config& c) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    settings = c; ++revision;
    latest = energy::Snapshot(); latest.revision = revision;
    latestTrace=CycleTrace();
    xSemaphoreGive(mutex);
}

energy::Snapshot snapshot() {
    if (!mutex) return energy::Snapshot();
    xSemaphoreTake(mutex, portMAX_DELAY);
    energy::Snapshot result = latest;
    xSemaphoreGive(mutex);
    return result;
}

DiagnosticSnapshot diagnosticSnapshot() {
    DiagnosticSnapshot result;
    if (!mutex) return result;
    xSemaphoreTake(mutex, portMAX_DELAY);
    result.data=latest; result.trace=latestTrace;
    xSemaphoreGive(mutex);
    return result;
}

void pause(bool value) {
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    const bool changed=paused!=value;paused = value;
    xSemaphoreGive(mutex);
    if(changed) DBG_INFO("[HA] Reader %s for firmware update",value?"paused":"resumed");
}
}  // namespace source

