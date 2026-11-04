# CYD Home Solar Monitor – agent and contributor guide

Applies throughout this repository. Read this before changing firmware, tooling or documentation. Current source version is `0.9.0`; the `0.9.0` source release is prepared locally, with publication pending. The latest published release recorded by the project remains `0.8.0`. Read `include/config.h` for the actual build identity rather than copying version labels from screenshots.

---

## Architecture and source of truth

This is a PlatformIO Arduino application for the ESP32-2432S028R: 320 × 240 landscape ILI9341, resistive XPT2046 touch, no PSRAM. The only environment is `cyd28`, with `espressif32@6.12.0`, Arduino core `2.0.17` and C++17. There is no Node package, frontend bundler, filesystem upload or direct inverter adapter.

`src/main.cpp` starts services and runs Improv, Wi-Fi, ArduinoOTA, touch, HTTP, display and serial diagnostics. `src/app/app.cpp` owns the sprite, screen state, calibration and night dim/off/on scheduling; portable rules live in `include/night_mode.h`. Keep legacy hour-based dim-only migration, minute precision, master enable, midnight ordering, clock fail-safe and the fully consumed one-minute wake contact. Screen-off must never stop HA/history, HTTP, serial or OTA. `include/boot_status.h` provides the shared seven-row CYD/serial startup model; `src/serial_diagnostics.cpp` gathers it and reports changes until ready or a 60-second deadline. Boot logs follow the saved debug level and never repeat for clock ticks or RSSI jitter. Start diagnostics before hardware services; rendering only reads the report. `src/data/ha_source.cpp` runs the core-0 FreeRTOS reader with a 16 KB stack, mutex-protected configuration revisions and published snapshots. Keep network I/O out of rendering and the normal main loop.

`include/energy_model.h`, `include/weather.h` and `include/diagnostic_helpers.h` hold portable parsing, freshness and diagnostic rules. `src/ui/flow_layout.cpp` renders through `ui::Canvas`, shared by the TFT adapter and host SVG renderer. These files must remain Arduino-free.

`src/web/monitor_web.cpp` owns HTTP routes and validation; `src/web/web_page.h` contains the whole dashboard (HTML/CSS/JavaScript). DOM IDs, six-source array indices and JSON names form a shared contract. `src/config/monitor_config.cpp` owns persisted application settings; the other `*_service.cpp` files own board services. `src/network/` and vendored `lib/ImprovWiFi/` provide USB provisioning.

`docs/index.html` is the user handbook and is embedded with its referenced images by `tools/embed_docs.py`. Every file immediately inside `web/assets/` is embedded too. Never edit or commit generated `src/web/handbook_data.h`. New asset types need a MIME entry in the embedding script. Change the dashboard asset's `?v=` when replacing a cached asset.

See [development reference](docs/DEVELOPMENT.md) for routes, settings and commands, [hardware checks](docs/HARDWARE_VALIDATION.md) for recorded observations, and [current roadmap](docs/REVIEW_AND_ROADMAP.md) for unfinished work. Historical research and build plans are context, not current implementation instructions.

---

## Build and validation

Run from the repository root:

```bash
python3 tools/check_host.py
pio run -e cyd28
python3 tools/preview_server.py
```

Host checks require Python 3 and a C++17 `c++` compiler. They compile assertions with `-Wall -Wextra -Werror`, run the energy and history regression suites, and render 22 SVG previews; `rsvg-convert`, if installed, also refreshes thirteen PNGs. Review generated changes before including them. The preview fixture listens only at `http://127.0.0.1:8765` and uses synthetic data; never enter real credentials there.

The firmware build generates `.pio/build/cyd28/firmware.bin`, `firmware-merged.bin` and `flash_parts.json`. Check the app still fits a `0x1C0000` (1,835,008 byte) OTA slot. Handbook images and web artwork use significant app flash. This repository uses the custom host script, not `pio test`; it has no PlatformIO Unity suite.

Run `node tests/test_web_history.js` for the actual embedded history chart and `node tests/test_web_settings.js` for settings load/save, including Off=0. Once PlatformIO project libraries are installed, `python3 tools/check_history_response.py` also compiles the actual history HTTP handler with the pinned ArduinoJson String writer and validates its streamed JSON; host checks include it when those headers are available, along with `tools/check_debug_config.py` for real config load/save and debug API validation, and `tools/check_night_config.py` for night schedule migration/API validation. For dashboard or handbook changes, syntax-check extracted scripts with `node --check` and review the affected page in a browser. The host script does not run JavaScript tests or physical HTTP/NVS/TLS/OTA tests. Its integration harnesses exercise real extracted API validation and configuration code against host stand-ins. Verify local links and re-run the firmware build after handbook edits because the handbook is compiled into firmware. Do not add tests that merely mirror documentation; add regression coverage when changing model behavior.

For model changes, cover signs, accepted/rejected units, stale/unknown/future timestamps, rollover, heartbeat behavior and solar input coherence. For rendering changes inspect both layouts and unavailable/offline scenes. Record actual checks and limitations. Never mark a hardware checklist complete based on compilation or the synthetic fixture.

---

## Data and persistence invariants

Read six individual Home Assistant `/api/states/<entity>` endpoints. Internal grid power is positive for import; battery power is positive for discharge. `energy::parseValue()` negates the upstream export-positive/charge-positive values. Solar is `max(inverter_DC - battery_internal, 0)`. Inverter AC is diagnostic only; household demand comes from its configured helper. Never mix battery DC into the household AC balance.

Keep indices `Inverter`, `Home`, `Grid`, `Battery`, `Soc`, `InverterDc` stable. The first five are legacy persisted order; `Solar` is derived. Daily totals are separate `energy::Total` entries, not additional sources. Preserve schema-1 migration (five mappings plus the default DC source) and schema-2 settings. Optional fields currently share schema 2. Test compatibility when changing storage.

Failed reads retain the last numeric value but invalidate it. Use `energy::metricUsable()` for live/stale decisions. `last_reported`, never `last_changed`, feeds the newest valid source heartbeat; `effective()` adjusts known report times to that heartbeat so unchanged battery readings stay usable. Unknown source age is not proof of upstream freshness. Solar requires fresh/coherent DC and battery inputs with `skewLimitSeconds(poll) = max(10, 2 * poll)`. Keep receive-age checks rollover-safe.

Totals are attempted after the six power reads, on configuration changes or at least 60 seconds since the previous attempt when the next cycle runs, and remain usable through 600 seconds receive age. Weather uses a separate report: 30-minute refresh, 5-minute retry after failure and 3-hour age limit. Failed attempted weather requests invalidate the report immediately. These are not guarantees of exact wall-clock cadence; requests and polling add time.

NVS namespaces are `solar-monitor` (`access`, `settings` JSON), `solar-touch` (`limits`, four `uint16_t` values) and `cyd-core` (`boot_count`, `device_name`; legacy `debug_level` is read for migration). Optional `debugLevel` (0–4) now lives in schema-2 application settings, applies atomically to both tasks and persists with the settings JSON. Keep default Info logging at roughly 60 seconds; Verbose enables every-poll diagnostics. Preserve Off=0 and old clients omitting the field. Wi-Fi credentials use framework NVS. `src/history_service.cpp` mounts the existing `spiffs` partition as LittleFS for 288 five-minute history slots. Keep the 15-minute alternating CRC32 checkpoints, blank-only first format, corrupt-bank fallback, mapping identity, timestamp/gap rules and controlled reboot/OTA saves. `include/history_model.h` and `src/ui/history_layout.cpp` stay Arduino-free. Never persist demo readings or stale numeric remnants. See `docs/HISTORY_IMPLEMENTATION.md`.

---

## Board and responsiveness constraints

Pins and TFT_eSPI setup live in force-included `include/config.h`. Display uses HSPI (MOSI 13, MISO 12, CLK 14, CS 15, DC 2); touch uses dedicated VSPI (CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36). Keep TFT_eSPI `TOUCH_CS` undefined; its resulting warning is expected. Preserve touch pressure/rail rejection and calibration limits.

LEDC channel 0 belongs to backlight GPIO 21. RGB LED pins 4/16/17 are active-low and held off. Rotation is 1 (USB right). Rendering uses a 320 × 240 RGB332 8-bit sprite; retain all palette `static_assert`s and exact RGB332 levels. Built-in fonts 1/2/4 are deliberate. Do not reintroduce font-6 numerals that remove labels; smooth fonts need a deliberate buffer redesign. Font 2 maps the grave character to a degree glyph.

Full frames run at 4 fps; centre-layout motion frames push only the pipe box at 20 fps. Keep labels outside that box and do not increase full-screen push frequency: this previously starved Wi-Fi. Night wake consumes the first touch. Keep ordinary loop work bounded and yielding; the existing Improv provisioning connection has a bounded blocking wait and needs separate hardware validation when modified.

HA reads share one keep-alive `HTTPClient` per cycle. Preserve HTTP/1.1, chunk-aware `getString()` handling and socket cleanup after transport/body failures. Do not introduce `useHTTP10()`. TLS sessions use `TlsWatchdogPause` to temporarily remove IDLE0 from the watchdog during long handshakes; preserve RAII restoration, timeouts and one TLS session at a time. Weather starts after the HA connection closes.

---

## Security and privacy safeguards

The HTTP UI has no login and no TLS listener. Anyone who can reach port 80 can read non-secret configuration and telemetry, change settings, reset Wi-Fi, restart or upload firmware. Keep deployment on a trusted LAN. Every mutation, including upload start, must retain `X-CYD-Request: 1`. This is a browser cross-site request guard, not authentication or authorization for LAN clients. Do not enable permissive CORS.

Never log or return the HA token, Wi-Fi password, device code or raw HTTP bodies. The token is write-only via the API: blank preserves it and `clearToken` explicitly removes it. The public CA certificate is intentionally returned as `ca` by `/api/config` so it can be edited; never accept or store a private key in that field. Serial logs include entity IDs, network details, configured place and coordinates; redact these before sharing. The diagnostic URL formatter logs only an origin.

Settings and Wi-Fi credentials reside in NVS; this project configures no flash encryption or secure boot. Browser token submission and HTTP HA requests are unencrypted. HTTPS HA needs the supplied PEM CA and a synchronized clock; weather uses the bundled ISRG Root X1. Never add `setInsecure()` or redirects that could forward a bearer token to another host.

Optional place search sends the search text from the browser to Open-Meteo geocoding. Optional device weather sends coordinates to Open-Meteo. NTP contacts the two configured time servers; mDNS advertises locally. Do not claim that all traffic stays on the LAN. Never add analytics, upload real data or probe the owner's HA/inverter as a documentation/build check.

Use synthetic fixtures; do not commit credentials, local settings, serial captures, `.pio/`, `.vscode/` or generated headers. Preserve dependency licenses and the vendored Improv parser-reset and reply-newline patches. Treat malformed serial frames as untrusted input; the review records pending bounds hardening.

---

## Contributions and releases

Match the surrounding C++ style: most app code uses four spaces, while inherited services include two-space files. Do not reformat unrelated code. Python uses four spaces; the dashboard deliberately uses compact inline CSS/JS. Preserve host portability and small device memory/flash use.

Markdown uses spaces only, four-space nested indentation, aligned table pipes and cells no longer than 45 characters. Use prose if a table would exceed that limit. Use en-dashes for prose dashes/ranges, sequential heading levels and `---` between major sections. Put file paths, field names and constants in inline code.

Keep patches focused and preserve existing user changes. Explain the behavior, validation and remaining hardware risks in the contribution description. Update the handbook and README for user-facing changes, this guide/development reference for architecture or command changes, and the current development changelog entry. Do not rewrite historical release claims as newly tested results.

The local workflow installs pinned ArduinoJson headers and runs host integration/model checks plus both JavaScript regression suites before calling the shared `anthonyjclarke/cyd-web-installer` workflow at `main`; the external workflow is a separate dependency. Preserve `PROJECT_NAME` as `CYD-Home-Solar-Monitor` and the GitHub repository spelling `CYD-Home_Solar_Monitor`, or installer Update recognition can break. Keep Improv vendored rather than adding a registry dependency, and service `improvTick()` every loop pass.

Preserve the existing partition offsets and sizes. `firmware.bin` is the app update image; `firmware-merged.bin` is a clean USB image written at `0x0` that overwrites NVS. Installer manifests use separate parts from `flash_parts.json`, including `boot_app0` to handle an active `app1`; never use the merged image as an update manifest part.

Release convention is a `v*` tag on `main` matching a suffix-free `FIRMWARE_VERSION`. Publish release images from CI, never a local configured device/build. Clear the outstanding real-board installer checks in `docs/WEB_INSTALLER.md` before a release. Publishing, flashing hardware and modifying live HA/inverter settings require task-specific authorization; a documentation review authorizes none of those operations.
