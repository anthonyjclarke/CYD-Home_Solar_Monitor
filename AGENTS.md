# CYD Home Solar Monitor – agent and contributor guide

Applies throughout this repository. Read this before changing firmware, tooling or documentation. The current version is `1.0.2`; read `include/config.h` for the actual build identity rather than copying version labels from screenshots.

Run commands in the checkout containing `platformio.ini` and `src/main.cpp`; a chat workspace can be empty. Check `git status --short` before editing.

The `1.0.2` release was published on 10-11-2026 by owner decision, ahead of full post-flash acceptance; its [post-flash test plan](docs/TEST_PLAN_1.0.2.md) records acceptance separately from desktop checks. Do not mark previous-build evidence as acceptance of this build.

The runtime state-machine inventory and transition ownership are in [STATE_MACHINES.md](docs/STATE_MACHINES.md). Panel navigation uses `include/panel_state.h`; Wi-Fi policy uses `include/wifi_lifecycle.h`. Browser history uses a settings generation and a queued single-flight refresh; retain its deferred-request tests.

The latest source review is [the 8 November follow-up](docs/REVIEW_2026-11-08.md), against `9805d26`. It distinguishes implemented fixes from transport/heap gaps and physical acceptance still owed. Preserve unrelated working changes.

---

## Architecture and source of truth

This is a PlatformIO Arduino application for the ESP32-2432S028R: 320 × 240 landscape ILI9341, resistive XPT2046 touch, no PSRAM. `cyd28` is the reference environment; `cyd24`, `e32r28t-st7789`, `e32r28t-ili9341` and `cyd40` build the other supported boards (see the board constraints below). All use `espressif32@6.12.0`, Arduino core `2.0.17` and C++17. There is no Node package, frontend bundler, filesystem upload or direct inverter adapter.

`src/main.cpp` starts services and runs Improv, Wi-Fi, ArduinoOTA, touch, HTTP, display and serial diagnostics. `src/app/app.cpp` owns the sprite, screen state, calibration and night dim/off/on scheduling; portable rules live in `include/night_mode.h`. Keep legacy hour-based dim-only migration, minute precision, master enable, midnight ordering, clock fail-safe and the fully consumed one-minute wake contact. Screen-off must never stop HA/history, HTTP, serial or OTA. `include/boot_status.h` provides the shared seven-row CYD/serial startup model; `src/serial_diagnostics.cpp` gathers it and reports changes until ready or a 60-second deadline. Boot logs follow the saved debug level and never repeat for clock ticks or RSSI jitter. Start diagnostics before hardware services; rendering only reads the report. `src/data/ha_source.cpp` runs the core-0 FreeRTOS reader with a 16 KB stack, mutex-protected configuration revisions and published snapshots. Keep network I/O out of rendering and the normal main loop.

`include/energy_model.h`, `include/weather.h` and `include/diagnostic_helpers.h` hold portable parsing, freshness and diagnostic rules. `src/ui/flow_layout.cpp` renders through `ui::Canvas`, shared by the TFT adapter and host SVG renderer. These files must remain Arduino-free.

`src/web/monitor_web.cpp` owns HTTP routes and validation; `src/web/web_page.h` contains the whole dashboard (HTML/CSS/JavaScript). DOM IDs, six-source array indices and JSON names form a shared contract. `src/config/monitor_config.cpp` owns persisted application settings; the other `*_service.cpp` files own board services. `src/network/` and vendored `lib/ImprovWiFi/` provide USB provisioning.

`docs/index.html` is the user handbook and is embedded with its referenced images by `tools/embed_docs.py`. Every file immediately inside `web/assets/` is embedded too. Never edit or commit generated `src/web/handbook_data.h`. New asset types need a MIME entry in the embedding script. Change the dashboard asset's `?v=` when replacing a cached asset.

See [development reference](docs/DEVELOPMENT.md) for routes, settings, commands and the flash budget, [history design](docs/HISTORY.md) for sampling and storage, [hardware checks](docs/HARDWARE_VALIDATION.md) for recorded board evidence and [web installer](docs/WEB_INSTALLER.md) for the release contract. Superseded plans and reviews live in Git history only.

---

## Build and validation

Run from the repository root:

```bash
python3 tools/check_docs.py
python3 tools/check_host.py
node tests/test_web_history.js
node tests/test_web_settings.js
node tests/test_web_dashboard.js
pio run -e cyd28
python3 tools/preview_server.py
```

Host checks require Python 3 and a C++17 `c++` compiler; JavaScript checks require Node.js 22 or later. If `pio` is absent from `PATH`, check the existing PlatformIO virtual environment at `$HOME/.platformio/penv/bin/pio` before installing anything. Host checks compile assertions with `-Wall -Wextra -Werror`, run the portable model and available integration suites, and render 22 SVG previews; `rsvg-convert`, if installed, also refreshes thirteen PNGs. Review generated changes before including them. The preview fixture listens only at `http://127.0.0.1:8765` and uses synthetic data, except history, which replays the owner's real capture in `tools/fixtures/webui-history.json` for the handbook charts; never enter real credentials there.

The firmware build generates `.pio/build/cyd28/firmware.bin`, `firmware-merged.bin` and `flash_parts.json`. Handbook images and web artwork use significant app flash. This repository uses the custom host script, not `pio test`; it has no PlatformIO Unity suite.

`tools/flash_budget.py` is attached to app-image generation and fails it when `firmware.bin` would leave less than 64 KiB of the 1,835,008-byte (`0x1C0000`) OTA slot; PlatformIO's app percentage under-reports the image. Recent 1.0.1 builds leave about 184 KiB free; record exact image bytes for the build being reviewed rather than treating one build's count as permanent. Treat embedded asset growth as a budget: encode captures as the development reference describes, and never enlarge partitions to hide an oversized app. An incremental build can reuse old outputs; use a fresh build directory when auditing a complete rebuild.

Run `node tests/test_web_history.js` for the actual embedded history chart, `node tests/test_web_settings.js` for settings load/save, including Off=0, and `node tests/test_web_dashboard.js` for live tile transitions, request deadlines and stale replies. Once PlatformIO project libraries are installed, `python3 tools/check_history_response.py` also compiles the actual history HTTP handler with the pinned ArduinoJson String writer and validates its streamed JSON; host checks include it when those headers are available, along with `tools/check_debug_config.py` for real config load/save and debug API validation, and `tools/check_night_config.py` for night schedule migration/API validation. For dashboard or handbook changes, syntax-check extracted scripts with `node --check` and review the affected page in a browser. The host script does not run JavaScript tests or physical HTTP/NVS/TLS/OTA tests. Its integration harnesses exercise real extracted API validation and configuration code against host stand-ins. Verify local links and re-run the firmware build after handbook edits because the handbook is compiled into firmware. Do not add tests that merely mirror documentation; add regression coverage when changing model behavior.

For model changes, cover signs, accepted/rejected units, stale/unknown/future timestamps, rollover, heartbeat behavior and solar input coherence. For rendering changes inspect both layouts and unavailable/offline scenes. Record actual checks and limitations. Never mark a hardware checklist complete based on compilation or the synthetic fixture.

For dashboard changes exercise transitions, not just initial scenes: configured to cleared, fresh to unavailable, request pending to timeout, disconnect to recovery, and older responses arriving late. Reset optional text/pills and disable flow arms on failure. For network/provisioning changes add malformed-length and truncated-input cases with host sanitizers where portable. The existing config harnesses extract selected validation blocks; they do not test the entire settings route, upload state machine or Wi-Fi lifecycle.

Transport checks must exercise the actual pinned HTTP body loop, including a connected peer sending no bytes, a partial body followed by silence, chunk metadata waits, cancellation and allocation failure. A bounded sink test alone does not establish transport deadlines or safe out-of-memory handling. Application C++ uses `-fno-exceptions`; avoid allocation paths that can throw instead of reporting failure. For history refresh, test an in-flight old response overlapping a settings/mapping save and queue a fresh request rather than silently discarding it.

---

## Data and persistence invariants

Read six individual Home Assistant `/api/states/<entity>` endpoints. Internal grid power is positive for import; battery power is positive for discharge. `energy::parseValue()` negates the upstream export-positive/charge-positive values. Solar is `max(inverter_DC - battery_internal, 0)`. Inverter AC is diagnostic only; household demand comes from its configured helper. Never mix battery DC into the household AC balance.

Keep indices `Inverter`, `Home`, `Grid`, `Battery`, `Soc`, `InverterDc` stable. The first five are legacy persisted order; `Solar` is derived. Daily totals are separate `energy::Total` entries, not additional sources. Preserve schema-1 migration (five mappings plus the default DC source) and schema-2 settings. Optional fields currently share schema 2. Test compatibility when changing storage.

Failed reads retain the last numeric value but invalidate it. Use `energy::metricUsable()` for live/stale decisions. `last_reported`, never `last_changed`, feeds the newest valid source heartbeat; `effective()` adjusts known report times to that heartbeat so unchanged battery readings stay usable. That shared heartbeat assumes related reporting sources; one independently updating helper is not proof that every mapped integration is healthy. Unknown source age is not proof of upstream freshness. Solar requires fresh/coherent DC and battery inputs with `skewLimitSeconds(poll) = max(10, 2 * poll)`. Keep receive-age checks rollover-safe and obtain the evaluation time after copying a worker snapshot.

Totals are attempted after the six power reads, on configuration changes or at least 60 seconds since the previous attempt when the next cycle runs, and remain usable through 600 seconds receive age. Weather uses a separate report: 30-minute refresh, 5-minute retry after failure and 3-hour age limit. Failed attempted weather requests invalidate the report immediately. These are not guarantees of exact wall-clock cadence; requests and polling add time.

NVS namespaces are `solar-monitor` (`access`, `settings` JSON), `solar-touch` (`limits`, four `uint16_t` values; `swap`, bool, axis swap saved with them – records without it are ignored on boards whose `TOUCH_SWAP_XY` default is 1; `frame`, `uint16_t` UI frame width they were sampled on – records without it count as 320 and are ignored on the 4.0″) and `cyd-core` (`boot_count`, `device_name`; legacy `debug_level` is read for migration). Optional `debugLevel` (0–4) now lives in schema-2 application settings, applies atomically to both tasks and persists with the settings JSON. Keep default Info logging at roughly 60 seconds; Verbose enables every-poll diagnostics. Preserve Off=0 and old clients omitting the field. Wi-Fi credentials use framework NVS. `src/history_service.cpp` mounts the existing `spiffs` partition as LittleFS for 288 five-minute history slots. Keep the 15-minute alternating CRC32 checkpoints, blank-only first format, corrupt-bank fallback, mapping identity, timestamp/gap rules and controlled reboot/OTA saves. `include/history_model.h` and `src/ui/history_layout.cpp` stay Arduino-free. Never persist demo readings or stale numeric remnants. See [HISTORY.md](docs/HISTORY.md).

---

## Board and responsiveness constraints

Pins and TFT_eSPI setup live in force-included `include/config.h`, one block per board selected by the env's `BOARD_*` flag; no flag is the classic 2.8″, which host tools compile. A new board is a new env and config block, not a code change. Display uses HSPI (MOSI 13, MISO 12, CLK 14, CS 15, DC 2) on every board. Own-bus boards use dedicated VSPI touch (CLK 25, MISO 39, MOSI 32, CS 33, IRQ 36) and keep TFT_eSPI `TOUCH_CS` undefined; its resulting warning is expected. Only the shared-bus 4.0″ defines `TOUCH_CS`, so TFT_eSPI reads touch there. Preserve touch pressure/rail rejection and calibration limits; reversed limits are a valid mirrored axis.

The ESP32-32E boards (`e32r28t-*`, `cyd40`) move red LED to GPIO 22; GPIO 4 is their speaker-amplifier enable, pulled up to off, and must stay untouched; GPIO 34 is battery voltage, not an LDR. The 4.0″ uses ST7796S and backlight GPIO 27, and its UI frame (`CYD_EXPECTED_WIDTH` × `CYD_EXPECTED_HEIGHT`) is a native 480 × 320 layout (`ui::Panel480`). This is the flow cross plus an information rail; cards, history, boot and settings each have their own 480 × 320 geometry. It renders through the same 76.8 KB sprite, created as 480 × 160: `paintFrame()` in `src/app/app.cpp` draws the whole layout twice through `TftCanvas::top` and pushes each band. A motion frame draws once with the band starting at the pipe box's top, so `drawFlow`'s box must stay at most 160 px tall. Do not scale a 320 × 240 frame: a 4/3 nearest-neighbour stretch was tried on the owner's board and made the built-in fonts jagged and small text unreadable. A full-panel 480 × 320 RGB332 sprite (153.6 KB) does not fit beside Wi-Fi and TLS. Push frames only through `display::pushFrame()`. Touch targets per frame size live in `include/panel_state.h` (`ui::targets320`/`targets480`) and must match the drawn buttons. Every other board's 320 × 240 output must stay pixel-identical: `tools/check_host.py` regenerates the 320 × 240 previews, and they must show no diff. Only `cyd28` has board evidence; record others in `docs/HARDWARE_VALIDATION.md`.

LEDC channel 0 belongs to the backlight (GPIO 21; 27 on the 4.0″). RGB LED pins (red 4 or 22, green 16, blue 17) are active-low and held off. Rotation is 1 (USB right). Rendering uses a 76.8 KB RGB332 8-bit sprite (320 × 240, or 480 × 160 bands on the 4.0″); retain all palette `static_assert`s and exact RGB332 levels. Built-in fonts 1/2/4 are deliberate. Do not reintroduce font-6 numerals that remove labels; smooth fonts need a deliberate buffer redesign. Font 2 maps the grave character to a degree glyph.

Full frames run at 4 fps; centre-layout motion frames push only the pipe box at 20 fps. Keep labels outside that box and do not increase full-screen push frequency: this previously starved Wi-Fi. Night wake consumes the first touch. Keep ordinary loop work bounded and yielding; the existing Improv provisioning connection has a bounded blocking wait and needs separate hardware validation when modified.

HA reads share one keep-alive `HTTPClient` per cycle. Preserve HTTP/1.1, chunk-aware bounded body reads and socket cleanup after transport/body failures. The present `writeToStream()`/`LimitedBody` integration caps accepted bytes but does not enforce an idle-body deadline; fix that at a layer that runs while no bytes arrive. Do not introduce `useHTTP10()`. TLS sessions use `TlsWatchdogPause` to temporarily remove IDLE0 from the watchdog during long handshakes; preserve RAII restoration, timeouts and one TLS session at a time. Weather starts after the HA connection closes.

---

## Security and privacy safeguards

The HTTP UI has no login and no TLS listener. Anyone who can reach port 80 can read non-secret configuration and telemetry, change settings, reset Wi-Fi, restart or upload firmware. Keep deployment on a trusted LAN. Every mutation, including upload start, must retain `X-CYD-Request: 1`. This is a browser cross-site request guard, not authentication or authorization for LAN clients. Do not enable permissive CORS.

Never log or return the HA token, Wi-Fi password, device code or raw HTTP bodies. The token is write-only via the API: blank preserves it and `clearToken` explicitly removes it. The public CA certificate is intentionally returned as `ca` by `/api/config` so it can be edited; never accept or store a private key in that field. Serial logs include entity IDs, network details, configured place and coordinates; redact these before sharing. The diagnostic URL formatter logs only an origin.

Settings and Wi-Fi credentials reside in NVS; this project configures no flash encryption or secure boot. Browser token submission and HTTP HA requests are unencrypted. HTTPS HA needs the supplied PEM CA and a synchronized clock; device weather uses plain HTTP to Open-Meteo by design (0.9.1): the TLS handshake no longer fits in heap beside the frame buffer, and mbedTLS then fails signature checks with MPI allocation errors that it reports as X509 `-9984`. Do not move weather back to HTTPS without measuring handshake heap on the board. Never add `setInsecure()` or redirects that could forward a bearer token to another host.

Optional place search sends the search text from the browser to Open-Meteo geocoding. Optional device weather sends coordinates to Open-Meteo unencrypted, and its reply is unauthenticated. NTP contacts the two configured time servers; mDNS advertises locally. Do not claim that all traffic stays on the LAN. Never add analytics, upload real data or probe the owner's HA/inverter as a documentation/build check.

Use synthetic fixtures by default; the committed real history capture is an existing documented exception, not permission to collect or commit more owner telemetry. Do not commit credentials, local settings, serial captures, `.pio/`, `.vscode/` or generated headers. Preserve dependency licenses and the vendored Improv parser-reset and reply-newline patches. Since 0.9.1 the parser bounds the advertised frame length before storing bytes and validates every nested offset; keep `tests/test_improv.cpp` covering both and recovery to a following valid frame. HTTP accepted body bytes are capped by `include/body_limit.h`: 16 KiB for HA, 8 KiB for weather, with a 5 s / 10 s sink deadline checked only when data arrives. An open silent connection can block the reader indefinitely in the pinned client. `std::string` buffer growth can throw on allocation failure; current tests do not prove recovery. The inbound settings limit is still a post-read check, because the pinned `WebServer` reads the whole body first.

Home Assistant helper examples must preserve unavailable inputs rather than silently turn them into zero. When revising handbook recipes, include availability conditions and upstream-outage checks for both house power and solar energy integration. Keep the deployed HA configuration outside a repository review's mutation scope.

---

## Review and acceptance expectations

Report findings with severity, a tight source location, triggering conditions, user impact, a proposed correction and a regression case. Separate reproduced host failures, static findings, design tradeoffs and feature ideas. Existing passing tests do not close findings outside their coverage. Record board evidence with a date in `docs/HARDWARE_VALIDATION.md` rather than rewriting earlier results.

Keep Improv input bounded, HTTP bodies capped as they arrive and disconnected/unavailable states explicit in both the panel and browser; each has a regression test. The setup portal opens only before the first connection in a boot. Weather waits for one completed HA cycle, not a successful one. A `pause(true)` request does not acknowledge that the reader has finished its current network operation; update coordination needs explicit review if changed.

Track the current review's `R1`–`R3` separately from the earlier `F1`–`F8`: HTTP byte limiting is implemented, but idle timeout and allocation recovery remain open; history cache invalidation (R3) now has a settings-save race regression; see `docs/STATE_MACHINES.md` for current closure evidence. Keep a short current finding register when retiring dated reviews so partially resolved work stays discoverable. Existing board evidence is evidence from its recorded build, not blanket acceptance of every later version.

Close a finding only with its relevant regression evidence. Installer, power-cut, NVS, TLS, night/DST, touch and endurance acceptance still require the actual board. Record board/version/date and retain unchecked cases when a desktop test cannot establish the result.

---

## Contributions and releases

Match the surrounding C++ style: most app code uses four spaces, while inherited services include two-space files. Do not reformat unrelated code. Python uses four spaces; the dashboard deliberately uses compact inline CSS/JS. Preserve host portability and small device memory/flash use.

Markdown uses spaces only, four-space nested indentation, aligned table pipes and cells no longer than 45 characters. Use prose if a table would exceed that limit. Use en-dashes for prose dashes/ranges, sequential heading levels and `---` between major sections. Put file paths, field names and constants in inline code.

Keep patches focused and preserve existing user changes. Explain the behavior, validation and remaining hardware risks in the contribution description. Update the handbook and README for user-facing changes, this guide/development reference for architecture or command changes, and the current development changelog entry. Do not rewrite historical release claims as newly tested results.

The local workflow installs pinned ArduinoJson headers and runs host integration/model checks plus all three JavaScript regression suites before calling the shared `anthonyjclarke/cyd-web-installer` workflow at `main`; the external workflow is a separate dependency. Preserve `PROJECT_NAME` as `CYD-Home-Solar-Monitor` and the GitHub repository spelling `CYD-Home_Solar_Monitor`, or installer Update recognition can break. Keep Improv vendored rather than adding a registry dependency, and service `improvTick()` every loop pass.

Preserve the existing partition offsets and sizes. `firmware.bin` is the app update image; `firmware-merged.bin` is a clean USB image written at `0x0` that overwrites NVS. Installer manifests use separate parts from `flash_parts.json`, including `boot_app0` to handle an active `app1`; never use the merged image as an update manifest part.

Release convention is a `v*` tag on `main` matching a suffix-free `FIRMWARE_VERSION`. Publish release images from CI, never a local configured device/build. Clear the outstanding real-board installer checks in `docs/WEB_INSTALLER.md` before a release. Publishing, flashing hardware and modifying live HA/inverter settings require task-specific authorization; a documentation review authorizes none of those operations.
