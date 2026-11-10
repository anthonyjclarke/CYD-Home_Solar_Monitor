# Development reference

The [state-machine inventory](STATE_MACHINES.md) maps runtime ownership, transitions, invariants and regression coverage. `include/panel_state.h` owns exclusive panel navigation; `include/wifi_lifecycle.h` owns provisioning/reconnection policy. The browser history request controller invalidates by saved-settings generation and queues a forced refresh while loading.

The [8 November review](REVIEW_2026-11-08.md) records current follow-up findings, closure evidence for the previous audit and validation limits.

Current for `1.0.2`. The [post-flash test plan](TEST_PLAN_1.0.2.md) contains release-specific checks, result fields and recovery guidance. See [AGENTS.md](../AGENTS.md) for architecture and contribution rules, [HISTORY.md](HISTORY.md) for the history design, and the [handbook](index.html) for end-user setup.

---

## Prerequisites and commands

Install PlatformIO Core, Python 3 and a C++17 compiler named `c++`. Node.js 22 or later runs the JavaScript regressions and syntax checks; no frontend build is needed. `rsvg-convert` is optional for PNG previews. Pinned dependencies are declared in `platformio.ini`: Espressif32 6.12.0, TFT_eSPI 2.5.43, WiFiManager 2.0.17, XPT2046 v1.4 and ArduinoJson 7.4.2. Improv 0.0.2 is vendored with local fixes.

Use the checkout containing `platformio.ini`; a companion chat directory need not contain the repository. If `pio` is missing from `PATH`, the existing installation may be at `$HOME/.platformio/penv/bin/pio`.

```bash
python3 tools/check_docs.py
python3 tools/check_host.py
node tests/test_web_history.js
node tests/test_web_settings.js
node tests/test_web_dashboard.js
pio run -e cyd28
python3 tools/preview_server.py
```

`default_envs` is `cyd28`, so a bare `pio run` builds only that; pass `-e` for the others: `cyd24`, `e32r28t-st7789`, `e32r28t-ili9341` and `cyd40`. Each env sets one `BOARD_*` flag that selects its pin, driver and geometry block in `include/config.h`; no flag means the classic 2.8″, which is also what the host tools compile. The UI frame stays 320 × 240 on every board and reaches the panel through `display::pushFrame()`, which centres it at 1× on the 4.0″; direct TFT drawing uses `display::panelX()`/`panelY()`. The 4.0″ defines `TOUCH_CS` and reads touch through TFT_eSPI on the display bus; the others keep their own VSPI touch bus. The shared installer workflow builds every `[env:]` and lists each by its `custom_installer_label`.

After PlatformIO installs project libraries, run `python3 tools/check_history_response.py` to validate the real history handler with the pinned ArduinoJson String writer. `python3 tools/check_debug_config.py` also checks the actual configuration loader/saver and debug API validation with host NVS stand-ins. `python3 tools/check_night_config.py` checks night schedule migration and API validation. `tools/check_host.py` includes these checks when the headers are installed; on a clean checkout it reports that this integration check is skipped.

`tools/check_docs.py` checks local documentation links, handbook anchors, Markdown structure, preview dimensions and version identity. `tools/check_host.py` compiles and runs `tests/test_energy.cpp`, `tests/test_history.cpp`, `tests/test_debug.cpp`, `tests/test_boot.cpp`, `tests/test_night.cpp`, `tests/test_lifecycles.cpp` and `tests/test_flow_status.cpp`, plus `tests/test_body_limit.cpp` and `tests/test_improv.cpp` under AddressSanitizer and UndefinedBehaviorSanitizer, and compiles the shared layout renderer with warnings as errors. It writes six scene SVGs for each of two layouts, a weather-only centre scene, four boot scenes and five history scenes into `docs/images/`. If `rsvg-convert` is available, it refreshes thirteen PNGs too. It does not update browser captures. Boot previews and the browser fixture both read `FIRMWARE_VERSION` from `include/config.h`. All readings in generated examples are synthetic.

The preview server runs at `http://127.0.0.1:8765`. It serves the dashboard, fixed-date synthetic status/configuration, history from `tools/fixtures/webui-history.json` (a real 24-hour `/api/history` capture from the owner's board; synthetic if the file is absent), the handbook with its images, a subset of preview images and footer artwork. Append `?theme=dark` for a deterministic dark-theme fixture. Settings are changed in memory only. It does not implement reboot, Wi-Fi reset, upload, authentication, persistence or device validation. Do not use real credentials. Stop with Ctrl-C.

To check both inline JavaScript programs without creating files in the source tree:

```bash
python3 - <<'PYCODE'
from pathlib import Path
import re, subprocess, tempfile
for name in ('src/web/web_page.h', 'docs/index.html'):
    scripts = re.findall(r'<script[^>]*>(.*?)</script>', Path(name).read_text(), re.S)
    for script in scripts:
        with tempfile.NamedTemporaryFile(suffix='.js', mode='w') as f:
            f.write(script)
            f.flush()
            subprocess.run(['node', '--check', f.name], check=True)
    print(name, len(scripts), 'inline scripts checked')
PYCODE
```

The handbook currently has no script. Syntax checks do not exercise browser events or device APIs. For UI changes inspect the fixture in light/dark and narrow/wide views, verify settings navigation and synthetic saves, reduced-motion behavior, and unavailable data. Confirm real API, NVS, provisioning and OTA behavior separately with the hardware checklist.

---

## Firmware outputs and hardware operations

`pio run -e cyd28` generates the embedded handbook before compilation and a merged image/part map after building. App capacity is 1,835,008 bytes (`0x1C0000`). `partitions_custom.csv` defines NVS at `0x9000`, OTA data at `0xE000`, app slots at `0x10000` and `0x1D0000`, LittleFS history data at `0x390000`, and coredump at `0x3F0000`. History mounts the existing 384 KiB `spiffs` partition as LittleFS; no partition offsets or sizes change.

Every build checks image headroom against the actual `firmware.bin`, not the linker's app percentage, which under-reports it. `tools/flash_budget.py` reads the `ota_0` size from the partition table, prints a `Flash budget:` line and fails the build (deleting the oversized image) when less than 64 KiB would remain. That margin is a team policy, not a hardware limit; retain the deployed partition contract rather than moving offsets to recover space.

The [1.0.2 test plan](TEST_PLAN_1.0.2.md) records exact image sizes from the prepared build. For comparison, an earlier `1.0.1` build was 1,646,704 bytes, leaving 188,304 bytes; the symbol-size measurements below belong to that earlier build.

| Item                                      | Bytes     |
| :---------------------------------------- | --------: |
| Embedded handbook, 25 assets in total     | 302,647   |
| – footer art `solar-footer.webp`          | 80,860    |
| – nine browser captures                   | 179,742   |
| – handbook HTML (gzip)                    | 20,185    |
| Dashboard page `WEB_PAGE`, uncompressed   | 46,720    |
| `printf`/`scanf` family (newlib)          | 58,155    |
| mbedTLS client and server handshake       | 13,776    |

The footer art is stored at 960 px, twice its largest displayed width of 480 px, at WebP quality 85 because its fine line work softens at 75. One further saving is known but not taken: serving `WEB_PAGE` gzipped, as the handbook already is, would save about 30 KB. Captures follow the encoding rule in [Refreshing visual documentation](#refreshing-visual-documentation).

The app update image is `.pio/build/cyd28/firmware.bin`. The clean USB image is `.pio/build/cyd28/firmware-merged.bin`, written at `0x0`; its padding overwrites NVS, including settings and the device code. `flash_parts.json` lists the separate flash images and offsets for the shared installer. A merged write is not a full-chip erase and does not guarantee removal of the separate coredump partition.

After authorization for a physical device, substitute the actual serial port:

```bash
pio run -e cyd28 -t upload --upload-port /dev/cu.usbserial-10
pio device monitor -b 115200 --port /dev/cu.usbserial-10
```

Use the browser's Firmware update for an app update over the LAN. ArduinoOTA is also advertised at `solar.local`, port 3232, protected by the device code shown on the panel. The browser firmware upload needs the mutation header but no password. Wi-Fi reset clears framework Wi-Fi credentials and restarts; application settings and calibration remain. This repository has no factory-reset-all-settings endpoint.

---

## Runtime and network behavior

The reader copies settings only when their revision changes, and publishes a snapshot only if its revision is still current and it is not paused. Six sequential entity requests share a keep-alive connection. Default wait is 5 seconds after a completed cycle, not an exact five-second start-to-start period. If all six reads fail, exponential waits are capped at 60 seconds; any valid source resets backoff. Offline Wi-Fi retries use a one-second wait. Demo and missing-token modes also use one-second waits.

Configured totals are attempted on a new revision or when at least a minute has elapsed, after the six live reads; no token means no total reads. Weather does not require the token. With a token configured, its first request waits for one completed Wi-Fi-connected HA cycle, successful or not, so a failing HA no longer blocks weather. Weather uses plain HTTP to `api.open-meteo.com`; see `AGENTS.md` for why. Clearing location resets the snapshot and disables device weather requests. External time synchronization is required for HTTPS HA and local night scheduling.

Startup diagnostics begin immediately after application configuration loads, before display/touch/network initialization. `include/boot_status.h` supplies a portable seven-row report and change tracker shared by serial and the CYD. `src/serial_diagnostics.cpp` gathers inputs on the main task every 250 ms while startup is active. HA status uses actual usable source/solar readings, not the configured URL as proof of connectivity, and explicitly reports a failed reader task via `source::readerState()`. `history::restoreState()` distinguishes pending, empty, restored, mapping reset, corrupt and unavailable storage; restored counts exclude new RAM captures, future values and (after clock sync) expired slots.

Serial emits the initial rows, status/value changes and an ending summary once ready or at a 60-second deadline. Clock text/RSSI changes alone are ignored for log deduplication. Setup stages log before potentially blocking initialization, so the last line identifies the stage reached if initialization stalls. Messages report software initialization rather than verifying physical peripherals. The saved debug level remains authoritative; Off does not stop CYD status collection. `app::update()` signals boot-screen dismissal so shared status refresh stops once both outputs are finished. Rendering reads the shared report without doing network I/O. All startup strings are bounded and secret-free; Wi-Fi text is sanitized for single-line ASCII output and long row values are clipped to the panel's actual font width.

`include/night_mode.h` contains portable dim/off/on scheduling, modulo-day time validation and rollover-safe touch-wake brightness rules. The existing `nightDim` API flag remains the master enable. Optional schema-2 fields are `nightMode` (0 dim, 1 off, 2 dim then off) and `nightDimAt`, `nightOffAt`, `nightOnAt` (local minutes 0–1439). Staged off must lie inside the dim-to-on arc, including dim but excluding on. Equal start/on disables single-action modes; disabled schedules never darken the panel. The WebUI uses native time inputs and converts HH:MM to minutes.

Legacy hour fields `nightStart` and `nightEnd` remain in config responses and requests. Old records migrate to dim-only with their original hours. New saves keep the legacy hours synchronized to the active start/on times; older clients round-tripping unchanged hours preserve minute precision. Changing a legacy hour updates the active start/on time without changing unrelated new settings. Invalid API types/ranges/order fail before writes; a malformed saved staged schedule falls back to its legacy dim interval without discarding HA configuration.

Only the display backlight/rendering sleep. Touch, source/history collection, serial, HTTP and OTA stay serviced. A wake contact is fully consumed and grants 60 seconds of full brightness; a separate contact can use controls. The initial 60 seconds after restart also remain awake. Missing clock means full brightness; automatic on resumes a complete frame. Physical backlight, midnight/DST transitions and wake gestures require board checks.

`python3 tools/check_night_config.py` tests the real configuration loader/saver and extracted optional night API logic against host NVS stand-ins; host checks include it when ArduinoJson headers are installed. `tests/test_night.cpp` covers minute boundaries, overnight/daytime periods, all modes, disabled/unknown-clock fail-safe behavior, wake consumption and rollover. The settings JavaScript tests cover time conversions, midnight zero, old API fallbacks, unused disabled fields and saving/reloading every mode.

The optional `debugLevel` field in schema-2 settings and `/api/config` accepts integers 0–4: Off, Error, Warn, Info and Verbose. It defaults to Info (3); old settings can migrate the scaffold's `cyd-core/debug_level` value. New saves persist it atomically with the application JSON; the legacy key is left untouched. Invalid API types/ranges are rejected before writes, and clients omitting the field preserve the saved level. Failed saves do not alter the active level. The shared runtime level is atomic because logs run on both tasks.

`include/debug_levels.h` holds portable level and cadence rules. Routine cycle details and health are limited to roughly 60 seconds at Info. New mode/level changes and occasional totals/weather attempts may report sooner; Verbose logs each new poll snapshot. Repeated snapshots are suppressed, including when an optional request flag stays set between polls. Off suppresses application macros without disabling Improv protocol responses; framework/bootloader logs are separate.

The browser polls status every three seconds and shared device history every minute. Timer status polls are guarded; a forced refresh after saving may overlap, and replies older than the one shown are dropped. Requests through `api()` give up after 12 s for status and 15 s otherwise. Direct firmware-upload and place-search fetches do not use that helper. A status timeout or failure marks the page disconnected and stops the flow animation. Keep these above the 5 s macOS can spend resolving `solar.local` (an unanswered IPv6 mDNS query). History uses 288 UTC five-minute slots, actual capture timestamps, per-field missing values and report-time availability. Two alternating verified LittleFS banks checkpoint every 15 minutes while dirty, plus controlled restart/update saves. See [the history design](HISTORY.md) for the binary format, identity reset rules, failure behavior and pending board tests. Chart clock labels use the browser timezone; the displayed device clock uses the configured POSIX timezone. Reduced Motion is read when the dashboard loads.

The device serves HTTP on port 80 after station Wi-Fi connects. ArduinoOTA owns mDNS startup; do not call `MDNS.begin()` independently. The separate WiFiManager portal opens after 20 seconds without an initial successful connection, times out after 180 seconds and retries. Once a connection has succeeded in that boot, later Wi-Fi loss uses reconnection alone, whether setup used the portal or a saved network.

---

## HTTP contract

| Method | Path              | Purpose                 |
| ------ | ----------------- | ----------------------- |
| GET    | `/`               | Dashboard HTML          |
| GET    | `/api/config`     | Editable settings       |
| POST   | `/api/config`     | Validate/save settings  |
| GET    | `/api/status`     | Snapshot and telemetry  |
| GET    | `/api/history`    | Rolling 24-hour samples |
| POST   | `/api/reboot`     | Delayed restart         |
| POST   | `/api/wifi-reset` | Clear Wi-Fi/restart     |
| POST   | `/update`         | Multipart app upload    |
| GET    | `/guide`          | Redirect to `/guide/`   |
| GET    | `/guide/`         | Embedded handbook       |
| GET    | `/guide/images/*` | Referenced images only  |
| GET    | `/assets/*`       | Embedded web assets     |

Every POST requires `X-CYD-Request: 1`; firmware upload checks it before starting a write. The server provides no permissive CORS handling. There is no login, read authorization, signed update verification or automatic update/rollback mechanism configured here. A custom header does not restrict a LAN client. Unknown routes return JSON 404.

`GET /api/config` returns `tokenSaved`, never the token. It returns the editable `ca` certificate and non-secret settings, including URL, entities and location. `POST /api/config` expects URL, exactly six entity IDs, integer timing/brightness/night fields and boolean `demo`/`nightDim`; the browser submits the entire form. Empty `token` preserves it; `clearToken: true` clears it. Saving resets the source snapshot and schedules a new revision.

Validation limits: JSON body 12,288 characters, URL 180, token 2,048, CA 6,000, entity IDs 8–100, battery label 14 printable ASCII characters, place 48, timezone name 40 and POSIX rule 3–60. Power mappings must start `sensor.` and use lower-case letters, digits or underscores. Optional total IDs may be blank. Poll is 2–300 seconds; stale is 15–3,600 and at least three polls; brightness is 10–255; legacy night hours are 0–23; new mode is 0–2 and new local minute fields are 0–1439. Coordinates must both be absent or within latitude ±90 and longitude ±180. POSIX validation is a character/length check, not a full timezone parser.

URLs allow HTTP/HTTPS and an optional base path; the reader appends `/api/states/`. Whitespace, userinfo, queries and fragments are rejected. HTTPS requires a PEM CA marker on save and actual certificate verification when connecting. Entity responses over 16 KiB and weather responses over 8 KiB are rejected as they arrive, including chunked and close-delimited bodies, so memory for a body never exceeds the cap. Bodies also have a 5 s (HA) or 10 s (weather) deadline, checked whenever data arrives. The 12,288-character settings check is still made after the web server has read the request.

`GET /api/status` includes six source readings plus derived solar in that order, `hasValue`, `valid`, usable `fresh`, raw report time/age, totals, weather, version, IP, uptime and heap. Raw report age may look old for unchanged sensors while heartbeat-adjusted freshness is true. `complete` covers the five displayed metrics, not the optional totals/weather or diagnostic AC reading. Use the shared model to interpret it.

---

## Documentation and release workflow

Update `docs/index.html` for device help; Markdown guides are not embedded. `tools/embed_docs.py` packages referenced `images/` files and immediate files under `web/assets/` into gitignored `src/web/handbook_data.h`. Handbook routes use `no-cache`; assets use a one-year immutable cache and need a new `?v=` when replaced. Rebuild and check flash use after changing images or text.

`.github/workflows/firmware.yml` triggers on pushes to `dev`/`main`, `v*` tags, pull requests and manual dispatch. Its check job uses the v7 checkout/setup actions, whose internal runtime is Node.js 24. The test tools remain Node.js 22 and Python 3.12; the setup action's internal runtime is independent of the version it installs. The job installs PlatformIO 6.2.0 and ArduinoJson 7.4.2 headers, then runs documentation, host integration/model and JavaScript regressions before the reusable external release workflow at `main`. This ensures the serialization and config harnesses run on a clean CI checkout. A branch push validates/builds without publishing; publication requires the matching release tag. This checkout does not contain that workflow's implementation; review it for release/Pages behavior when publishing. Release history is in `CHANGELOG.md`; pending real-board installer cases are in [WEB_INSTALLER.md](WEB_INSTALLER.md).

Keep a contribution focused, explain the resulting behavior, provide commands/results and distinguish host tests from hardware observations. Use synthetic test data and redact logs/screenshots before sharing. Preserve licenses and existing pin/partition/protocol contracts. A documentation-only review does not require flashing, publishing or contacting a live Home Assistant instance.


---

## Refreshing visual documentation

Run `python3 tools/check_host.py` to regenerate all 22 SVG and thirteen optional PNG previews from the actual shared renderers. Start `python3 tools/preview_server.py` and capture each dashboard/settings/device page in a browser. Save captures as WebP at most 932 px wide, quality 75 (`cwebp -q 75 -m 6 -sharp_yuv`): the handbook is embedded in the firmware, so image bytes cost OTA headroom. Use the fixed synthetic profile; only history comes from the committed real capture, which holds power readings and no credentials or identifiers. The fixture labels itself Preview and never talks to HA or a CYD. Its Home Assistant URL is a reserved documentation address. Staged night times in captures illustrate the available controls; the firmware default and migrated profiles remain dim-only.

Keep full dashboard and Display captures, plus focused flow, combined-history, battery-history, debug and night-control views. Refresh connection, location, entities, diagnostics and device pages too. Inspect each encoded capture for readable text. Update every reference when filenames change, run `python3 tools/check_docs.py` and rebuild: the flash budget check reports the remaining headroom.
