# Changelog

## [1.0.1] 08-11-2026

First published release of the 1.0 line. The `v1.0.0` tag failed CI before its release step, so nothing was published from it; 1.0.1 is 1.0.0 plus the fix below.

### Fixed

- The vendored Improv parser compared its `size_t` frame position with an `int` frame length. Linux GCC rejects that under `-Werror` (`-Wsign-compare`); macOS clang accepted it. The frame length is now a `size_t`. Firmware behaviour is unchanged.

---

## [1.0.0] 08-11-2026

First stable release. It fixes all eight findings of the 7 November code audit (F1–F8), adds a flash budget, moves device weather to plain HTTP after a heap failure found on the board, and tidies the documentation. It includes the unpublished `0.9.0` and `0.9.1` work. Running on the owner's board since 08-11-2026; see `docs/HARDWARE_VALIDATION.md` for the evidence and the checks still open.

### Fixed

- **Improv serial provisioning (F1).** Frames advertising a payload larger than the 128-byte receive buffer are rejected before any byte is stored, and the Wi-Fi settings parser bounds every length byte and string span by the received payload. Malformed or truncated credentials return `ERROR_INVALID_RPC` instead of reading past the payload. The library's callback pointers now start as null, and `sprintf` became `snprintf`.
- **Dashboard tiles (F2).** Solar today, Tomorrow and Weather are rewritten on every status reply. Clearing an entity or the location, or losing the forecast and weather together, shows "–" with *not configured* or *unavailable* instead of leaving the last value looking current.
- **Live polling (F3).** Every browser request has a deadline: 12 s for status and 15 s otherwise, above the 5 s macOS can spend resolving `solar.local`. Only one timer poll runs at a time, and a reply older than the one already shown is dropped. A failure or timeout stops every flow arm as well as greying the tiles, and the status line says how long ago the last update was.
- **HTTP response bodies (F4).** HA entity and weather replies stream into a buffer that refuses bytes past 16 KiB or 8 KiB. An oversized chunked or unknown-length reply is cut off and its connection closed instead of being read whole into heap first. A body that trickles in past 5 s (HA) or 10 s (weather) is cut off the same way. Truncated bodies and failed buffer allocations are reported as read errors. Keep-alive reuse after a complete HA read is unchanged. The settings upload limit is still checked after the web server has read the request.
- **Setup portal (F5).** The Wi-Fi setup portal no longer reopens during a later outage after the device has connected in that boot. Portal-assisted and saved-network boots both recover by reconnection alone.
- **Weather start (F6).** Weather no longer waits indefinitely when Home Assistant fails from boot. The first fetch waits for one completed Wi-Fi-connected HA cycle, successful or not.
- **Weather transport.** Weather failed with `X509 - Certificate verification failed` (`-9984`). An on-board diagnostic showed each root's signature verifying offline, but the handshake failing with `BIGNUM - Memory allocation failed`: its peak near 60 KB no longer fits in the roughly 96 KB left beside the frame buffer. Weather now uses plain HTTP to `api.open-meteo.com`, so coordinates travel unencrypted and the reply is unauthenticated. This removes the 4–9 s blocking handshake, its watchdog pause and the bundled ISRG Root X1 header, and raised the lowest free heap from about 41 KB to about 64–73 KB. The Location page hint no longer claims verified HTTPS.
- **Unknown source age on the panel (F8).** The centre layout used to show *All readings live* or *Good time to use power* whenever readings were usable, even without a Home Assistant report time. It now shows *Live / source age unknown* or *Spare power / age unknown*; the Cards layout already did.
- Persisted touch calibration is validated on restore with the same span rules used when saving; an invalid record keeps the built-in defaults rather than reaching `map()` with equal endpoints.
- Panel freshness is evaluated after copying the source snapshot, so a publication newer than the frame's timestamp can no longer wrap to a huge receive age.

### Changed

- The browser history picker offers the power flows, battery level, inverter AC/DC and today's solar kWh. Solar forecast, temperature, weather code, tomorrow's maximum and tomorrow's sky code are no longer listed; the device still records them and the panel's Prev/Next still shows them.
- **Handbook HA helper recipes (F7).** The house power and solar production templates check both inputs with `has_value()` and become *unknown* when either is missing, instead of `float(0)` turning a missing grid or battery reading into a plausible but wrong number. Existing helpers must be edited by hand.
- The dashboard footer art is 960 × 320 px at WebP quality 85 instead of 1600 × 533 (112,064 to 80,860 bytes); its cache-busting query is now `?v=2`.
- Every handbook browser capture is re-encoded at WebP quality 75 and at most 932 px wide. Seven full-page captures were retaken to show `1.0.0`, and the two history charts now show a real 24-hour capture from the owner's board, replayed by the preview server from `tools/fixtures/webui-history.json`.

### Added

- **Flash budget.** `tools/flash_budget.py` runs after every build, locally and in CI, and fails it when `firmware.bin` would leave less than 64 KiB of the OTA slot. It measures the actual image, which the linker's percentage under-reports. With the image work above, headroom went from 10,912 bytes to 188,224 bytes.
- Host regression `tests/test_improv.cpp` under AddressSanitizer and UndefinedBehaviorSanitizer: valid, empty-password, maximum-size, oversized, bad-checksum and every truncated Wi-Fi-settings boundary, plus recovery on the next frame. It fails against the `0.9.0` parser.
- Host regression `tests/test_body_limit.cpp` under the same sanitizers: exact limit, one byte over, a chunked flood, an oversized first block, an empty body and refusal staying set.
- Host regression `tests/test_flow_status.cpp`: verified, unknown-age and stale status in both layouts, including the spare-power case.
- Browser regression `tests/test_web_dashboard.js`, run in CI: configured-to-cleared, fresh-to-invalid, forecast-to-weather fallback and loss of both optional sources, plus explicit rejection, a hung request reaching its deadline, overlapping timer ticks, a late superseded reply and recovery. It fails against the `0.9.0` page.

### Documentation

- Retired the code audit, the review and roadmap, the documentation review, the `0.9.0` release record, the original build plan and the Rayhub research brief; they remain in Git history. Remaining feature ideas moved to the README.
- `docs/HISTORY_IMPLEMENTATION.md` became `docs/HISTORY.md`, a current design reference without one-off release notes.
- `docs/HARDWARE_VALIDATION.md` now opens with a dated evidence log from the owner's board and ticks only items with evidence.
- `AGENTS.md`, `CLAUDE.md`, `README.md`, `docs/DEVELOPMENT.md`, `docs/WEB_INSTALLER.md` and the handbook were brought up to date for `1.0.0`, including the flash budget, browser deadlines, the macOS `solar.local` delay and the capture encoding rule.

---

## [0.9.0] 05-11-2026

Source release prepared and committed locally; not published. Its verification record has since been retired (see Git history).

### Added

- Shared CYD/serial startup diagnostics: seven status rows cover Wi-Fi, IP, web address, signal, actual Home Assistant freshness/coherence, history restore and clock sync. Serial also reports firmware/reset/app slot and each service initialization stage before the TFT is available. Startup output follows the saved log level, prints changes once and ends when ready or after 60 seconds; runtime logging remains limited to its normal cadence.
- Persistent rolling 24-hour history for all seven energy metrics, optional daily totals and weather fields, using five-minute actual snapshots and alternating CRC32-verified LittleFS checkpoints every 15 minutes on the existing data partition.
- A 320 × 240 History screen in both themes with metric tabs, other-metric navigation, signed trends, missing-data gaps and sampled statistics. History replaces Settings on the flow screen; a hold longer than two seconds opens Settings, and a separate hold there retains calibration.
- Streaming `GET /api/history?hours=24`, shared by the panel and WebUI. Browser history now survives reloads; per-metric charts keep units separate and demo samples are excluded.
- Portable regression checks for retention, stale/unknown/future readings, timestamp correction, checkpoint corruption and interrupted writes, touch gestures and browser history rendering; synthetic history previews and a hardware validation checklist.

### Changed

- Expanded Display → Night mode with dim-only, off-only and staged dim/off/on actions, minute-precision local times, automatic morning turn-on and retained settings. Legacy hour-based schedules migrate to dim-only. Screen-off pauses only backlight/rendering; touch wakes for a minute while acquisition, history, WebUI and OTA stay active. Added model, configuration/API and WebUI regression coverage.
- Reduced routine serial polling and health output to about once per minute at the default Info level; Verbose retains every-poll diagnostics. Mode/state changes and occasional optional-reading events remain visible.
- Added Settings → Display → Serial debug with Off, Errors only, Warnings and errors, Info and Verbose. The selection applies immediately, persists in the existing schema-2 JSON and migrates the old scaffold debug key. Added configuration, API, cadence and WebUI regressions, including Off=0 and failed saves.

### Fixed

- Updated the model-check job to Node.js 24 action runtimes with `actions/checkout@v7`, `actions/setup-node@v7` and `actions/setup-python@v7`, removing GitHub's Node.js 20 deprecation warning. The previous run's checks and firmware build had already passed; publication still requires a release tag.
- Fixed malformed history JSON: ArduinoJson clears its output string, so row separators are now appended to the response packet after serialization. Added host integration coverage using the actual firmware handler and pinned ArduinoJson String writer, including full, missing and mixed samples across chunk boundaries.

### Documentation

- Added repository-specific `AGENTS.md` and `docs/DEVELOPMENT.md` with architecture, build/test commands, persistence/API contracts, board constraints, security/privacy safeguards and contribution guidance.
- Updated README and embedded handbook for current development/release identity, actual network traffic, certificate exposure, timing and persistent device/browser history; expanded local validation instructions.
- Reconciled the roadmap with implemented features, marked original research/build plans as historical, and retained unchecked hardware/installer cases separately from desktop results.
- Consolidated Claude guidance onto the shared agent guide and recorded the 5 November scan, follow-up code observations and validation in `docs/DOCUMENTATION_REVIEW.md`.
- Documented 24-hour history with screenshots in the README and handbook: all five panel history views (solar, battery, dark, other metric, empty) and the browser's combined and single-metric charts. Regenerated all 22 panel SVGs and thirteen PNGs; retook every browser screenshot at 0.9.0, including staged night settings, serial debug and Device & firmware. WebP captures reduce embedded asset cost; the decorative footer is unchanged.

Firmware dependencies, partition offsets/sizes and settings schema numbers are unchanged. CI now installs the pinned ArduinoJson headers for real history/config regression harnesses and runs both JavaScript suites. Physical-device history, flash interruption, night transitions and OTA/installer checks remain pending.

---

## [0.8.0] 04-11-2026

### Added

- Web dashboard footer artwork: an Australian solar illustration (sun to rooftop panels, house to grid, house to battery, native plants and wildlife) between the 30-minute chart and the footer links. It's decorative only (`alt=""`), shown centred at up to 480 px wide (full width on narrow phones), keeps its 3:1 shape, and is lightened in dark mode with a CSS filter.
- `web/assets/` for static web files. `tools/embed_docs.py` embeds them at build time and the device serves them at `/assets/<name>`, with a one-year browser cache; the page adds `?v=` to the address, so a new version gets fetched. The transparent WebP is 112 KB; the PNG fallback in the handoff package (215 KB) is left out, since every current browser shows WebP. App flash is now 92%.

### Changed

- Documentation sweep:
    - Reduce Motion explained in the handbook (energy flow caption and a troubleshooting row) and the README: with it on, the web comets stay still by design.
    - Footer artwork described in the handbook and README.
    - "Layout" setting named as in the web UI.
    - Comets replace chevrons and arrows in the hardware checklist.
    - Items the owner has verified are ticked: panel layout, iPhone access, CI releases.
- Every web screenshot retaken for 0.8.0 with the footer artwork, including the README's full-page dashboard capture.

---

## [0.7.0] 04-11-2026

### Added

- Panel (centre layout) re-laid out to match the web flow:
    - pipes of equal width end exactly 6 px from each text block;
    - comets (head, mid and pale tail) ease along each pipe the way power flows, faster as power rises, with a second comet above 2.5 kW;
    - a sine-wave hub at (140,115);
    - grid and battery blocks centred under their labels;
    - an upright battery gauge with the percentage beneath.
- Panel redraw: the whole screen 4 times a second (`APP_FULL_FRAME_MS`); between full frames, only the pipe area is redrawn and pushed at 20 fps (`APP_FLOW_FRAME_MS`). Pushing the full screen at 10–20 fps starved Wi-Fi: TCP connects took 0.4 s and page loads 2.2 s. With the partial redraw, loads are typically 0.3–0.7 s, with occasional slower ones.
- Wi-Fi modem sleep turned off (`WiFi.setSleep(false)`); the panel is mains-powered. With sleep on, pings to the panel swung between 40 and 450 ms.
- README "Browser dashboard" section: a full-page capture of the dashboard, plus settings, location and diagnostics screenshots, each with notes on what it's for. Settings, location, entities and diagnostics screenshots retaken to show 0.7.0-dev and the Help link.
- Web dashboard: banner at the foot linking to the GitHub repository.
- Sun icon for the browser tab, on the device page and the handbook.
- Handbook served by the device at `http://solar.local/guide/` (works offline), linked from a new Help group in the sidebar and from the dashboard banner. `tools/embed_docs.py` embeds `docs/index.html` and its images at build time (about 270 KB; app flash now 84%). Served with `Cache-Control: no-cache`, so browsers pick up the new handbook after a firmware update.
- Handbook screenshots retaken with the comet flow, the Help link and the footer banner; the 30-minute chart has its own figure.
- Handbook "What you need" section: SolarEdge inverter, battery and meter with Modbus TCP enabled, Home Assistant with SolarEdge Modbus Multi, the house power template, an access token, and the hardware (CYD board, data cable, 5 V supply, browser, 2.4 GHz Wi-Fi). README carries the same summary.
- Handbook appendix and README acknowledgements, with links to every integration, library and service used.
- Handbook recipes for the house power template and a true daily solar energy sensor (template, integral, utility meter).

### Changed

- Web flow diagram: comets (pale core, coloured head, fading tail) travel each arm in the direction of flow, easing in and out, replacing the blunt dashes and triangle arrowheads. Arm thickness, comet size and speed scale with power (capped at 5 kW); browsers set to reduce motion show still comets. The hub shows a sine wave instead of "INV".
- Handbook "Choosing a layout" shows both layouts side by side and says where to switch (Settings → Display → Layout); the README shows both too.
- The web UI hint for "Solar energy today" warns against inverter AC energy on battery systems.
- References to the author's other projects removed from the README.

### Fixed

- Web dashboard: after a single failed refresh (for example a Wi-Fi blip), the Solar today, Tomorrow and Weather tiles stayed grey until the page was reloaded. They now recover on the next good refresh.

---

## [0.6.0] 04-11-2026

First public release, with a browser installer at https://anthonyjclarke.github.io/CYD-Home_Solar_Monitor/.

### Changed

- Web UI rebuilt in the SpendAnalysis style: warm paper palette with matching dark mode, amber accent for controls only, green/red for meaning, mono uppercase micro-labels, cream cards, grouped sidebar (Live · Settings · Device) and a CRT "Device status" panel.
- Settings split into Home Assistant, Display, Location & weather and Entities sections; Save on any section saves all, and an invalid field elsewhere switches to its section.
- Diagnostics shown as a table, with the raw status JSON behind a disclosure.
- Handbook display section: annotated with the clock-and-weather top row (the common setup) and cut from 17 markers to 9; arrows explained once instead of per arm, battery gauge and percentage combined. The kWh top row is shown separately.
- Handbook previews are drawn as live readings rather than demo, so the status line shows "All readings live" / "Good time to use power" as on a working panel.

### Added

- Web installer: CI builds release images on a `v*` tag and publishes the ESP Web Tools page; Improv-Serial answers on every boot so the installer offers Update (settings kept) on a provisioned panel. Boot log reports the running app slot (`Running from app0`).
- Animated energy-flow diagram: solar, grid, battery and house around the inverter, with moving dots and arrowheads showing direction, faster at higher power, and a battery icon filled to the state of energy.
- "At a glance" tiles with direction pills, a battery-level bar, solar today, tomorrow and current weather.
- "Last 30 minutes" chart of solar, house, grid and battery, collected in the browser (no device storage).
- Chart axes: kW Y axis on rounded steps with gridlines and a solid zero line; X axis ticked every 5 minutes in local time, ending at "now".
- Boot screen in the panel's theme: Wi-Fi state and network, IP address, `solar.local`, signal strength, Home Assistant host (flags a missing token) and clock, with a progress bar. Held until 6 s after Wi-Fi connects (15 s cap without Wi-Fi); a tap skips it. Replaces the start-up message that vanished before it could be read.

### Fixed

- Boot loop since 0.4.0: the Open-Meteo TLS handshake (ECDSA chain, about 7.6 s on the device) blocked core 0 without yielding, so the task watchdog aborted about 19 s after boot. Core 0's idle task is now taken off the watchdog only while a TLS session is open (weather, and HTTPS Home Assistant). Verified on hardware: weather `http=200`, no resets, all readings fresh.
- First readings now reach the panel about 2 s after boot instead of about 19 s: a Home Assistant read attempted before Wi-Fi is up no longer triggers a 10 s backoff, and the first weather fetch waits until readings have been published.
- Weather TLS handshake limit raised from 8 s to 20 s (`APP_TLS_HANDSHAKE_S`); measured handshakes on the device take 4–9 s, so 8 s failed intermittently.
- Handbook: the not-configured preview and caption claimed the panel asks for a token; the panel shows "Waiting / stale or unavailable" and the token prompt is in the browser. The solar formula now reads DC less battery discharge.
- Reverted the 0.5.0 panel re-layout: font 6 values made the panel hard to read and dropped the SOLAR/GRID/BATTERY/HOUSE labels and card subtitles. Both layouts are back to the labelled 0.4.0 design.

---

## [0.5.0-dev] 03-11-2026

### Added

- mDNS: the device answers at `http://solar.local/`, shared with the ArduinoOTA responder.
- `docs/index.html` handbook: install, first setup, Home Assistant setup, an annotated display guide, state gallery, full settings reference, freshness rules, serial diagnostics, troubleshooting and security notes, with web UI screenshots.

### Changed

- Centre layout re-laid for the full canvas: 48 px font 6 values on all four arms, full-length bolder chevrons, larger state text and a large battery gauge with its percentage.
- Cards layout uses font 6 values and drops the engineering subtitles.
- Removed the browser login (trusted LAN only). Changes still require the `X-CYD-Request` header to block cross-site requests; ArduinoOTA and the setup hotspot keep the device code.
- Preview renderer uses TFT_eSPI's real glyph widths for fonts 2, 4 and 6.

---

## [0.4.0-dev] 03-11-2026

### Added

- Weather from Open-Meteo for a configured location: current temperature and conditions, and tomorrow's maximum and conditions, fetched every 30 minutes over HTTPS verified against ISRG Root X1.
- Drawn weather icons (clear, partly cloudy, cloudy, fog, drizzle, rain, snow, storm) on both layouts.
- Browser **Location, timezone and weather** settings: place search (Open-Meteo geocoding, run in the browser), latitude/longitude, and a POSIX timezone filled from a built-in Olson table or entered manually.
- Battery gauge icon filled to state of energy, coloured by level, on both layouts.
- `[WEATHER]` serial diagnostics; browser shows local time and current/tomorrow weather.

### Changed

- Timezone is a setting applied immediately on save, instead of the fixed Australia/Sydney default.
- The centre layout shows state of energy beside the gauge on its own row, separate from "Charging"/"Supplying".
- Reader task stack raised to 16 KB for TLS.

---

## [0.3.1-dev] 03-11-2026

### Fixed

- Battery, state of energy and solar no longer go grey when the battery is idle and full. HA left the battery entities' `last_reported` unchanged for over ten minutes while the inverter kept reporting, so per-entity report age wrongly marked them stale and solar was rejected for timestamp skew.

### Changed

- Report age is now judged from the snapshot heartbeat (newest report among valid sources). Unavailable entities and a stalled integration are still rejected.
- Serial `[READ]` reasons include `fresh-unchanged`; `[POLL]` logs `heartbeat_age`; `[SOLAR]` labels the raw report skew.
- Removed the "Source times differ" panel and browser warnings; raw skew remains in diagnostics.

---

## [0.3.0-dev] 03-11-2026

### Added

- Optional **Solar energy today** and **Solar forecast tomorrow** kWh entities, read once a minute on the existing connection.
- Centre layout top row shows today's kWh (falling back to the clock) and "Tomorrow ~N kWh"; the cards layout title shows today's kWh.
- Browser dashboard shows both totals; `/api/status` reports them under `totals`.
- `[TOTAL]` serial diagnostics and host tests for kWh parsing and total freshness.

---

## [0.2.3-dev] 03-11-2026

### Added

- Light centre-flow panel layout: solar above, grid and battery either side and house below a central hub, with animated chevrons, a battery level bar, the clock and a plain-English status line ("Good time to use power" while exporting or charging a full battery).
- **Panel layout** browser setting to choose centre flow (default) or the original dark cards; saved settings without it get the centre layout.
- Preview renders of both layouts from `tools/check_host.py`.

---

## [0.2.2-dev] 03-11-2026

### Fixed

- Panel palette now uses exact RGB332 levels; the 8-bit frame buffer previously turned the navy cards dark green and the background black. A compile-time check enforces this.
- The touch that wakes a dimmed panel at night no longer also opens the setup page.
- The setup page is drawn into the frame buffer, removing its once-per-second flash.
- The "Source times differ" status now uses the same 10-second minimum skew limit as solar validity.

### Changed

- All six Home Assistant reads in a cycle share one keep-alive connection instead of six separate connections.
- The reader task copies settings only when they change, not every 100 ms.
- Power shows watts below 1 kW and two decimals above, on the panel and in the browser.
- The battery label beside state of energy is a setting (default `LG 9.8 kWh`).
- Freshness and skew-limit rules are shared helpers used by the panel, browser API and serial log.
- Timezone and NTP servers moved to `config.h`; the backlight is written only when its level changes.

---

## [0.2.1-dev] 03-11-2026

- Added serial poll diagnostics with HTTP status, timing, numeric values, units and freshness reasons.
- Added solar input/skew reporting and AC balance checks to explain mismatched readings.
- Added timestamped boot, configuration, Wi-Fi, clock, data-state and periodic heap summaries.
- Kept credentials and arbitrary response text out of serial output and centralised poll logging on the main loop.

---

## [0.2.0-dev] 03-11-2026

- Added inverter DC acquisition and calculated solar generation on the panel and browser dashboard.
- Kept inverter AC in diagnostics and routed the flow arrows through an inverter hub.
- Added `Watt`/`Watts` unit support and source validity, freshness and timestamp-skew checks for solar.
- Added a legacy settings migration retaining existing credentials and the first five mappings.
- Verified charging, discharging, grid charging, night, missing-input and skewed-input calculations with host checks.

---

## [0.1.0-dev] 03-11-2026

- Added the CYD 2.8-inch PlatformIO firmware and existing installer build contract.
- Added Home Assistant entity configuration and background live power acquisition.
- Added an AC/DC flow screen, source validity handling and synthetic demo scenes.
- Added a local browser dashboard, persisted settings and manual authenticated OTA.
- Added touch calibration, night dimming and Improv Wi-Fi provisioning.
- Verified the firmware build and portable energy-model checks; hardware commissioning remains pending.

