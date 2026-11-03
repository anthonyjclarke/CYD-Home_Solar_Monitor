# CYD Home Solar Monitor – Claude context

## Target

ESP32-2432S028R (CYD 2.8″), ILI9341, landscape rotation `1` (USB right), no PSRAM. `espressif32@6.12.0` → Arduino core `2.0.17`, **C++17** (overrides the global C++11 rule). Single env `cyd28`.

## What it is

`0.7.0-dev` (0.6.0 released). Energy-flow display (solar / home / grid / battery + SOC) that reads a SolarEdge SE6000 + LG battery **via Home Assistant's REST API** (`/api/states/<entity>`), never the inverter directly. A background FreeRTOS task polls six entities; the main loop renders, handles touch, web UI, OTA and Improv. Running on the owner's panel; open checks in `docs/HARDWARE_VALIDATION.md`.

---

## Hardware quirks (non-standard vs global CYD rules)

- All TFT_eSPI config is in `include/config.h`, force-included via `-include include/config.h` – **not** `build_flags`. Edit pins there.
- Display on HSPI (`USE_HSPI_PORT`, MISO `12`); touch on its own VSPI bus: CLK `25`, MISO `39`, MOSI `32`, CS `33`, IRQ `36`.
- `TOUCH_CS` must stay **undefined** – TFT_eSPI's shared-bus touch path breaks this board. The resulting compile warning is expected.
- Touch rejects `z < 350`, `z >= 4000` and rail coordinates (phantom-tap guard copied from `CYD_AnimatedPixelClock`).
- RGB LED is held off (active LOW, R4/G16/B17). Backlight is LEDC channel `0`; apps must not reuse it.
- Frame buffer is a 320×240 **8-bit (RGB332)** sprite. Every palette colour in `flow_layout.cpp` must be an exact RGB332 level (R/G steps of 255/7, B steps of 85); a `static_assert` enforces it. Off-level colours shift hue on hardware.
- Fonts are built-in GLCD 1/2/4 (known deviation from the VLW rule). Font 6 (48 px) was tried in 0.5.0 and rejected by the owner as unreadable and label-less – don't reintroduce large numerals at the cost of labels. Preview widths come from TFT_eSPI tables in `tools/render_preview.cpp`.
- Web UI (`src/web/web_page.h`) deliberately mirrors the owner's SpendAnalysis app (`../SpendAnalysis/.streamlit/config.toml`): paper palette, amber chrome, green/red for meaning only. Element IDs are the API contract with `monitor_web.cpp`. Smooth fonts would anti-alias into off-palette RGB332 colours; they need 16-bit sprites, not the shared 8-bit buffer.
- Two layouts share `ui::drawFlow()`: `Cards` (dark) and `Centre` (light, white is the nearest RGB332 off-white). Both palettes live in `flow_layout.cpp` under the same `static_assert`.

---

## Data-model rules – do not break

- Internal signs: grid **+import**, battery **+discharge**. HA sources are the opposite (grid +export, battery +charge); `energy::parseValue()` negates them.
- Solar = `max(inverter_DC − battery_internal, 0)`. **Never** derive solar from inverter AC; AC is diagnostic only.
- Never add battery DC into the AC household balance; `sensor.home_power_usage` is authoritative.
- Invalid/stale readings keep the last number (grey) – never silently become 0.
- Freshness uses HA `last_reported`, not `last_changed`, judged via `energy::effective()` against the snapshot `heartbeatMs` (newest valid report). HA does **not** refresh `last_reported` for unchanged battery states – never revert to per-entity report age. Unknown report time is labelled, not assumed fresh.
- Solar is invalid if DC/battery timestamps differ by more than `energy::skewLimitSeconds(poll)`. Use `energy::metricUsable()` for live/stale decisions – never re-derive it.
- Daily totals (`energy::Total`) are separate from `Metric` and the six-entity array – do not add them to `SourceCount`. Their freshness is receive-age only (`totalUsable`), never `last_reported`.
- `energy::Metric` indices 0–4 are persisted order – append only (`InverterDc` = 5). `Solar` is derived, not a source.
- `energy_model.h`, `diagnostic_helpers.h` and `flow_layout.cpp` must stay Arduino-free – they compile on the host in `tools/check_host.py`.

## Security rules

- HA token, Wi-Fi password, device access code, CA cert and HTTP bodies are never logged or returned by the API. Serial logs only the URL origin.
- Web UI has **no login** by the owner's decision (trusted LAN). Mutations still require header `X-CYD-Request: 1` (CSRF guard) – keep it. OTA and the setup AP still use the device code.
- mDNS name `APP_MDNS_NAME` ("solar") is set via `ArduinoOTA.setHostname()`; ArduinoOTA owns `MDNS.begin()`, so don't call it again.
- Font 2 draws `` ` `` as a degree sign (`TFT_ESPI_GRAVE_IS_DEGREE` is set inside TFT_eSPI's `Font16.c`).
- HTTPS to HA requires a pasted PEM CA – no `setInsecure()` fallback.
- HA reads share one keep-alive `HTTPClient` per cycle. Do not re-enable `useHTTP10()` (it disables reuse); read bodies with `getString()` (handles chunking) and call `Connection::close()` on any failure after the request is sent.

---

## NVS

| Namespace       | Keys                                          |
|:----------------|:----------------------------------------------|
| `solar-monitor` | `access` (12-hex device code), `settings`     |
| `solar-touch`   | `limits` (4×`uint16_t` raw x0,x1,y0,y1)       |
| `cyd-core`      | `boot_count`, `device_name`, `debug_level`    |

`settings` is one JSON blob, `schema: 2` (six entities; optional `battery` label, `layout` (0 cards, 1 centre), `today`/`forecast` kWh entity IDs, `lat`/`lon`/`place`, `tz` (POSIX) and `tzName` added without a schema bump). Loader still accepts `schema: 1` with five entities and appends the DC default – keep that migration.

## Build / release

- `firmware.bin` for browser/ArduinoOTA updates; `firmware-merged.bin` (from `tools/merge_bin.py`) erases NVS – USB clean install only.
- Partition layout must not change between releases (installer + OTA compatibility).
- CI uses `anthonyjclarke/cyd-web-installer` reusable workflow; `python3 tools/check_host.py` runs first and regenerates `docs/images/*.svg`.
- Timezone is a POSIX string from settings (default `APP_TZ_POSIX`), applied with `setenv`/`tzset` in `applyTimezone()`, not ezTime. The Olson→POSIX table lives in the web page JS only.
- TLS on the core-0 reader task must be wrapped in `TlsWatchdogPause`: an mbedTLS handshake (≈7.6 s to Open-Meteo) never yields and the IDLE0 task watchdog reboots the device. Plain HTTP doesn't need it.
- Weather: Open-Meteo over TLS verified with `include/ca_isrg_root_x1.h` (expires 2035). It runs in the reader task after the HA connection closes, so only one TLS session is open at a time. Do not add `setInsecure()`.

## Web installer and releases

- GitHub repo is `CYD-Home_Solar_Monitor` (underscores); `PROJECT_NAME` stays `CYD-Home-Solar-Monitor`. Both are frozen: the manifest `name` must match what Improv reports or the installer stops offering Update.
- Release images come only from CI on a `v*` tag on `main` matching `FIRMWARE_VERSION` with no suffix. Never put `firmware-merged.bin` in a manifest.
- Improv is vendored in `lib/ImprovWiFi` – never add it to `lib_deps`. `improvTick()` must run at least every ~1 s.
- Before the next release, clear *Tests owed* in `docs/WEB_INSTALLER.md`.
- `src/web/handbook_data.h` is generated (gitignored) by the `pre:tools/embed_docs.py` script from `docs/index.html` and the images it references, served at `/guide/`. Keep doc images small: app flash is ~84% of the 1.79 MB slot.

## Style

Two coexisting styles: scaffold services (`*_service.cpp`) use 2-space, one statement per line; app code (`app/`, `data/`, `ui/`, `web/`) is dense 4-space. Match the file you are editing.
