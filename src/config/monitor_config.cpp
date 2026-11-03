#include "monitor_config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include "debug.h"

namespace monitor {
namespace { Config current; String code; }
const Config& config() { return current; }
const String& accessCode() { return code; }

void loadConfig() {
    Preferences p;
    p.begin("solar-monitor", false);
    code = p.getString("access", "");
    if (code.length() != 12) {
        char buf[13];
        snprintf(buf, sizeof(buf), "%08lX%04lX", static_cast<unsigned long>(esp_random()),
                 static_cast<unsigned long>(esp_random() & 0xffff));
        code = buf; p.putString("access", code);
    }
    const String saved = p.getString("settings", "");
    p.end();
    if (saved.isEmpty()) { DBG_INFO("[CFG] No saved settings; using defaults"); return; }
    JsonDocument d;
    if (deserializeJson(d, saved)) { DBG_WARN("[CFG] Invalid saved settings JSON; using defaults"); return; }
    const bool legacy = d["schema"] == 1 && d["entities"].size() == 5;
    if (!legacy && !(d["schema"] == 2 && d["entities"].size() == energy::SourceCount)) { DBG_WARN("[CFG] Unsupported settings schema or entity count; using defaults"); return; }
    Config c;
    c.url = d["url"] | c.url;
    c.token = d["token"] | "";
    c.caCertificate = d["ca"] | "";
    // Legacy settings retain URL, token and all five mappings. The sixth
    // source starts at its confirmed default until the next settings save.
    for (size_t i = 0; i < d["entities"].size(); ++i) c.entities[i] = d["entities"][i].as<String>();
    const int poll = d["poll"] | 5, stale = d["stale"] | 180;
    const int bright = d["brightness"] | 180, start = d["nightStart"] | 19, end = d["nightEnd"] | 7;
    if (poll < 2 || poll > 300 || stale < 15 || stale > 3600 || stale < poll * 3 ||
        bright < 10 || bright > 255 || start < 0 || start > 23 || end < 0 || end > 23 ||
        c.url.length() > 180 || c.token.length() > 2048 || c.caCertificate.length() > 6000) { DBG_WARN("[CFG] Saved settings failed validation; using defaults"); return; }
    c.pollSeconds = poll; c.staleSeconds = stale; c.brightness = bright;
    c.nightStart = start; c.nightEnd = end;
    c.nightDim = d["nightDim"] | true; c.demo = d["demo"] | false;
    // Optional field added in 0.2.2; older records keep the default label.
    if (d["battery"].is<const char*>() && d["battery"].as<String>().length() <= 14) c.batteryLabel = d["battery"].as<String>();
    current = c;
    DBG_INFO("[CFG] Loaded settings%s",legacy?"; migrated five mappings and added DC default":" (schema 2)");
}

bool saveConfig(const Config& next) {
    JsonDocument d;
    d["schema"] = 2; d["url"] = next.url; d["token"] = next.token; d["ca"] = next.caCertificate;
    for (const auto& entity : next.entities) d["entities"].add(entity);
    d["poll"] = next.pollSeconds; d["stale"] = next.staleSeconds; d["brightness"] = next.brightness;
    d["nightStart"] = next.nightStart; d["nightEnd"] = next.nightEnd;
    d["nightDim"] = next.nightDim; d["demo"] = next.demo; d["battery"] = next.batteryLabel;
    String body;
    if (!serializeJson(d, body)) return false;
    Preferences p;
    if (!p.begin("solar-monitor", false)) { DBG_ERROR("[CFG] Could not open NVS to save settings"); return false; }
    // One NVS record keeps settings together across an interrupted write.
    const bool ok = p.putString("settings", body) == body.length();
    p.end();
    if (ok) { current = next;DBG_INFO("[CFG] Settings saved (schema 2)"); }
    else DBG_ERROR("[CFG] Could not write settings to NVS");
    return ok;
}
}  // namespace monitor
