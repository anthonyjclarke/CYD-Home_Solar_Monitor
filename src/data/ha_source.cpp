#include "ha_source.h"
#include "diagnostic_helpers.h"
#include "debug.h"
#include "ca_isrg_root_x1.h"
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
bool beginAttempted = false;

// An mbedTLS handshake runs its elliptic-curve maths as one long step that
// cannot yield. On this core-0 task it starved IDLE0 for over 5 s during the
// Open-Meteo handshake (ECDSA chain) and the task watchdog aborted the device.
// Take IDLE0 off the watchdog only while a TLS session is open. Plain HTTP is
// unaffected, and every request still has connect/read/handshake timeouts.
struct TlsWatchdogPause {
    const bool active;
    explicit TlsWatchdogPause(bool tls) : active(tls) { if (active) disableCore0WDT(); }
    ~TlsWatchdogPause() { if (active) enableCore0WDT(); }
};

// One keep-alive connection serves all entity reads in a cycle, so the six
// readings arrive close together instead of over six separate handshakes.
struct Connection {
    bool secure;
    TlsWatchdogPause watchdog{secure};  // Declared after secure, so initialised after it.
    WiFiClient plain;
    WiFiClientSecure tls;
    HTTPClient http;
    explicit Connection(const monitor::Config& c) : secure(c.url.startsWith("https://")) {
        if (secure) { tls.setCACert(c.caCertificate.c_str()); tls.setHandshakeTimeout(APP_TLS_HANDSHAKE_S); }
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

// metric selects the power parser; Count means a kWh total. label numbers the
// entity in user-facing messages.
bool readEntity(const monitor::Config& c, Connection& conn, const String& entity, energy::Metric metric, unsigned label,
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
    String url = c.url + "/api/states/" + entity;
    const bool opened = conn.secure ? http.begin(conn.tls, url) : http.begin(conn.plain, url);
    if (!opened) { trace.error=ReadError::Url; snprintf(message, messageSize, "Invalid Home Assistant URL"); return false; }
    http.addHeader("Authorization", "Bearer " + c.token);
    http.addHeader("Accept", "application/json");
    const int status = http.GET();
    trace.httpStatus = status;
    if (status != 200) {
        trace.error = ReadError::Http;
        snprintf(message, messageSize, "HA HTTP %d / reading %u", status, label);
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
        snprintf(message, messageSize, "Invalid entity JSON / reading %u", label);
        conn.close(); return false;
    }
    const char* state = doc["state"] | "";
    const char* unit = doc["attributes"]["unit_of_measurement"] | "";
    trace.state = diagnostics::stateKind(state,trace.rawValue,trace.numeric);
    trace.unit = diagnostics::safeUnit(unit);
    float value = 0;
    const bool parsed = metric == energy::Count ? energy::parseEnergy(state, unit, value)
                                                : energy::parseValue(state, unit, metric, value);
    if (!parsed) {
        // The body was fully read, so the connection stays usable.
        trace.error = ReadError::Value;
        snprintf(message, messageSize, "Unavailable or wrong unit / reading %u", label);
        http.end(); return false;
    }
    reading = {value, true, millis(), energy::parseTimestamp(doc["last_reported"] | "")};
    trace.error = ReadError::None;
    http.end();
    return true;
}

// Current conditions and tomorrow's maximum from Open-Meteo for the configured
// coordinates. HTTPS is verified against ISRG Root X1; only the coordinates
// leave the LAN. A failure keeps the previous report but marks it invalid.
void fetchWeather(const monitor::Config& c, weather::Report& report, RequestTrace& trace) {
    const uint32_t start = millis();
    trace = RequestTrace();
    if (!WiFi.isConnected()) trace.error = ReadError::Wifi;
    else if (time(nullptr) < 1700000000) trace.error = ReadError::Clock;
    if (trace.error != ReadError::NotRead) { trace.elapsedMs = millis() - start; return; }
    report.valid = false;  // Only an attempted request can invalidate the last report.
    TlsWatchdogPause watchdog(true);
    WiFiClientSecure tls;
    tls.setCACert(ISRG_ROOT_X1);
    tls.setHandshakeTimeout(APP_TLS_HANDSHAKE_S);
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    char url[260];
    snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max&timezone=auto&forecast_days=2",
             c.latitude, c.longitude);
    if (!http.begin(tls, url)) { trace.error = ReadError::Url; trace.elapsedMs = millis() - start; return; }
    trace.httpStatus = http.GET();
    if (trace.httpStatus != 200) trace.error = ReadError::Http;
    else if (http.getSize() > 8192) trace.error = ReadError::TooLarge;
    else {
        JsonDocument filter;
        filter["current"]["temperature_2m"] = true;
        filter["current"]["weather_code"] = true;
        filter["daily"]["weather_code"] = true;
        filter["daily"]["temperature_2m_max"] = true;
        JsonDocument doc;
        const String body = http.getString();
        if (body.length() > 8192) trace.error = ReadError::TooLarge;
        else if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) trace.error = ReadError::Json;
        else if (!doc["current"]["temperature_2m"].is<float>() || !doc["daily"]["temperature_2m_max"][1].is<float>() ||
                 !doc["current"]["weather_code"].is<int>() || !doc["daily"]["weather_code"][1].is<int>()) trace.error = ReadError::Value;
        else {
            report = {true, millis(), doc["current"]["temperature_2m"].as<float>(), doc["current"]["weather_code"].as<int>(),
                      doc["daily"]["temperature_2m_max"][1].as<float>(), doc["daily"]["weather_code"][1].as<int>()};
            trace.error = ReadError::None;
        }
    }
    http.end();
    tls.stop();
    trace.elapsedMs = millis() - start;
}

void worker(void*) {
    uint32_t seenRevision = 0, lastCycle = 0, waitMs = 0, totalsRevision = 0, lastTotals = 0;
    uint32_t weatherRevision = 0, lastWeather = 0;
    bool haPublished = false;  // Live readings have reached the display at least once.
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
            bool cycleHasData = false;
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
                const weather::Report kept = s.weather;  // Weather does not need HA.
                s = energy::Snapshot();
                s.weather = kept;
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
                    if (readEntity(c, conn, c.entities[i], static_cast<energy::Metric>(i), i + 1, s.readings[i], fieldError, sizeof(fieldError),trace.requests[i])) {
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
                    if (i == energy::SourceCount - 1 &&
                        (rev != totalsRevision || static_cast<uint32_t>(millis() - lastTotals) >= 60000UL)) {
                        // Totals ride on the same connection after the live readings.
                        // A failure keeps the previous number but marks it invalid.
                        for (int t = 0; t < energy::TotalCount; ++t) {
                            if (c.totalEntities[t].isEmpty()) { s.totals[t] = energy::Reading(); continue; }
                            char ignored[80];
                            readEntity(c, conn, c.totalEntities[t], energy::Count, energy::SourceCount + t + 1,
                                       s.totals[t], ignored, sizeof(ignored), trace.totals[t]);
                        }
                        trace.totalsRead = true; totalsRevision = rev; lastTotals = millis();
                    }
                }
                s.skewSeconds = newest && oldest != UINT64_MAX ? (newest - oldest) / 1000 : 0;
                conn.close();
                s.heartbeatMs = energy::newestReport(s);
                energy::deriveSolar(s, millis(), energy::skewLimitSeconds(c.pollSeconds));
                snprintf(s.message, sizeof(s.message), "%s", valid != energy::SourceCount ? error :
                         !s.readings[energy::Solar].valid ? "Solar inputs have different timestamps" : "Home Assistant connected");
                failures = valid ? 0 : std::min(failures + 1, 4U);
                waitMs = std::min<uint32_t>(c.pollSeconds * 1000UL * (1UL << failures), 60000UL);
                if (!failures) waitMs = c.pollSeconds * 1000UL;
                // Wi-Fi not up yet (typical at boot) is not a Home Assistant
                // failure: retry soon instead of backing off.
                if (!WiFi.isConnected()) { failures = 0; waitMs = 1000; }
                cycleHasData = valid > 0;
                s.cycleMs = millis();
            }
            // Weather runs after the HA connection is closed, so only one TLS
            // session is ever open. It is independent of the HA token.
            const uint32_t weatherWait = (s.weather.valid ? weather::refreshSeconds : weather::retrySeconds) * 1000UL;
            // The first weather fetch (an ~8 s TLS handshake) waits until live
            // readings have been published once, so they reach the panel first.
            if (!c.demo && !std::isnan(c.latitude) && (haPublished || c.token.isEmpty()) &&
                (rev != weatherRevision || static_cast<uint32_t>(millis() - lastWeather) >= weatherWait)) {
                fetchWeather(c, s.weather, trace.weather);
                trace.weatherRead = true;
                // Without a clock TLS cannot start; retry on the next cycle instead.
                if (trace.weather.error != ReadError::Clock && trace.weather.error != ReadError::Wifi) {
                    weatherRevision = rev; lastWeather = millis();
                }
            }
            s.revision = rev;
            trace.id=++cycleId; trace.elapsedMs=millis()-now; trace.nextPollMs=waitMs;
            xSemaphoreTake(mutex, portMAX_DELAY);
            if (revision == rev && !paused) { latest = s; latestTrace=trace; if (cycleHasData) haPublished = true; }
            xSemaphoreGive(mutex);
            lastCycle = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
}  // namespace

void begin() {
    beginAttempted = true;
    mutex = xSemaphoreCreateMutex();
    if (!mutex) { DBG_ERROR("[HA] Could not create source mutex"); return; }
    settings = monitor::config(); revision = 1;
    running = xTaskCreatePinnedToCore(worker, "ha-reader", 16384, nullptr, 1, nullptr, 0) == pdPASS;
    if (!running) snprintf(latest.message, sizeof(latest.message), "HA task could not start");
    if (!running) DBG_ERROR("[HA] Reader task could not start");
    else DBG_INFO("[HA] Background reader started; %u source entities",unsigned(energy::SourceCount));
}

ReaderState readerState() { return !beginAttempted ? ReaderState::NotStarted : running ? ReaderState::Running : ReaderState::Failed; }

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

