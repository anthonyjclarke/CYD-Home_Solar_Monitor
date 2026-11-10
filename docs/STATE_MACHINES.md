# Application state machines

Source audit on 10 November 2026, starting at `8531932` (`dev`, then firmware `1.0.1`). The changes are included in the prepared `1.0.2` patch release; use its [post-flash test plan](TEST_PLAN_1.0.2.md) for hardware acceptance. This inventory covers application-owned runtime lifecycles across all firmware sources, portable models, the embedded dashboard and the vendored Improv implementation. Library internals, build tooling and the preview fixture are outside the runtime inventory. Entries group related substates by owner; computed freshness and rendering states are included because time and prior observations affect behavior.

The decision is to give mutually exclusive states one owner, keep independent lifecycles separate, and put transition rules in small portable models where regression tests can exercise them. This change makes panel navigation, Wi-Fi provisioning and browser history request ownership explicit. Existing APIs, persisted schemas, entity order, board configuration, layout geometry and partitions retain their contracts.

For users this prevents old-source charts after saves and protects setup recovery. For developers, portable policies separate decisions from hardware effects. For agents, named states, ownership, invariants and executable failure scenarios make later changes reviewable without guessing which combinations of flags are legal.

---

## Panel and input

### 1. Screen navigation – explicit

Owner: [`include/panel_state.h`](../include/panel_state.h), integrated by [`src/app/app.cpp`](../src/app/app.cpp). `Screen` is exactly one of `Boot`, `Flow`, `History`, `Settings` or `Calibration`. A boot tap only skips boot; a Settings tap only returns to Flow. The History button enters History, its Home button returns, a hold opens Settings and a separate hold there starts Calibration. Boot completion cannot displace Settings or Calibration.

Previously four independent booleans represented these mutually exclusive screens. `tests/test_lifecycles.cpp` now exercises navigation boundaries, invalid completion events, calibration return and consumed gestures. History metric selection remains independent and retained across visits.

### 2. Startup screen timing – implicit

Owner: `bootStart` and `connectedAt` in `src/app/app.cpp`. Waiting for Wi-Fi transitions to a connected grace period or an offline maximum, then Flow. A tap or hold closes the startup screen early. The first observed connection latches the grace-period origin; later disconnects do not restart boot. This screen timeout is separate from diagnostic readiness (entry 27).

Preserve rollover-safe elapsed timing and bounded boot display. Host startup models are covered, but physical startup and touch timing still need the panel.

### 3. Calibration acquisition and persistence – implicit

Owners: `target`, `calibrationHeld`, `waitCalibrationRelease` and `raw` in `src/app/app.cpp`; limits in [`src/touch_service.cpp`](../src/touch_service.cpp). Enter by consuming the initiating hold, wait for release, capture each of four press/release contacts, extrapolate limits, then save or reject and return to Settings. Inactivity after 45 seconds also returns to Settings. Invalid saved limits retain board defaults; reversed axes are valid.

Do not confuse completion of four samples with a successful NVS write. A future extraction should model contact phase and target together and test timeout during a held contact; this patch preserves the existing acquisition behavior. Physical calibration remains required.

### 4. Touch contact recognition – compact model

Owner: [`include/touch_gesture.h`](../include/touch_gesture.h). Released becomes pressed, then either a short tap on release, one hold while pressed, or a consumed wake contact. A consumed contact cannot generate a later tap or another hold. `wasPressed` in the app separately latches initial coordinates and wake time.

Keep one action per contact and unsigned elapsed arithmetic. Existing history/night tests and the new lifecycle suite cover taps, holds, wake consumption and navigation handoff.

### 5. Night schedule and temporary wake – computed

Owner: [`include/night_mode.h`](../include/night_mode.h), applied in `app::update()`. Local time selects Normal, Dimmed or Off; master enable, mode and minute ordering govern transitions. Unknown clock fails safe to Normal. A touch overrides dim/off for one minute and consumes that contact; continued contact extends the wake period.

This is independent of the current screen. Off suppresses backlight/rendering while the rest of the application runs. Existing night and configuration suites cover midnight, disabled schedules, legacy migration, precise times and rollover; hardware/DST acceptance remains separate.

### 6. Rendering capability and cadence – implicit

Owner: `buffered`, `flowShown`, `lastPaint` and `lastMotion` in `src/app/app.cpp`. Sprite allocation selects buffered rendering or a low-memory boot message. A complete Flow frame enables partial Centre-layout pipe updates; another screen or screen-off invalidates that eligibility. Full frames run at 4 Hz and partial motion at 20 Hz, with a separate 1 Hz Settings cadence. The Centre renderer in `src/ui/flow_layout.cpp` retains `cometPhase`, `cometMs` and measured `textHalf` widths across frames. `src/backlight_service.cpp` separately caches brightness to avoid repeated PWM writes.

Keep frame eligibility distinct from navigation and brightness. Portable layout tests/rendered previews cover output, not bus timing or heap allocation on hardware.

---

## Networking and acquisition

### 7. Wi-Fi setup and reconnection – explicit

Owner: [`include/wifi_lifecycle.h`](../include/wifi_lifecycle.h), effects in [`src/wifi_service.cpp`](../src/wifi_service.cpp). Joining opens a nonblocking Portal after 20 seconds without connection. WiFiManager owns the 180-second portal timeout; observing closure enters RetryWait. After another 20 seconds without connection, Portal reopens. Online closes any portal; later outages enter Reconnecting and never reopen it during that boot. Reconnect attempts have their own 30-second clock.

The previous shared `lastAttempt` let reconnect reset the portal retry deadline first on sufficiently delayed loop passes. Separate clocks and a portal-first decision remove that starvation case. `tests/test_lifecycles.cpp` covers normal and delayed servicing, failed portal start, saved/USB-style connections, post-connection outages, recovery and `millis()` rollover. It verifies production policy, not WiFiManager/RF behavior; actual portal/provisioning acceptance remains open.

### 8. Improv serial frame parser – implicit cursor

Owner: `_position` and `_buffer` in [`lib/ImprovWiFi/src/ImprovWiFiLibrary.cpp`](../lib/ImprovWiFi/src/ImprovWiFiLibrary.cpp). Header, version, type, length, payload and checksum are recognized by cursor position. Rejection or a complete frame resets the cursor. Nested credential spans are validated before access.

Retain the advertised-size bound before storing bytes and parser recovery to a following frame. `tests/test_improv.cpp` exercises the actual parser with ASan/UBSan, including malformed and truncated input. No parser rewrite is needed here.

### 9. Improv provisioning transaction – mixed

Owners: Improv protocol states and [`src/network/improv_setup.cpp`](../src/network/improv_setup.cpp). Authorized becomes Provisioning, then Provisioned or Stopped/error. The custom connection saves attempted credentials, waits up to 15 seconds, and restores previous credentials after failure. Success latches `improvSucceeded`, flushes the reply, checkpoints history and restarts.

This is a bounded blocking lifecycle; it can delay ordinary loop work. Preserve AP+STA mode and never log credentials. The parser suite does not establish physical credential rollback, reply delivery or restart acceptance.

### 10. Reader initialization, pause and publication – mixed

Owner: `mutex`, `beginAttempted`, `running`, `paused` and `revision` in [`src/data/ha_source.cpp`](../src/data/ha_source.cpp); public `ReaderState` in [`include/ha_source.h`](../include/ha_source.h). Initialization ends Running or Failed. Configuration increments a revision and clears published data. A cycle publishes only if its captured revision remains current and pause is false.

Pause is a request, not an idle acknowledgement. It cancels publication and is checked between power requests, but does not interrupt an active transport or fully gate totals/weather. Before changing updater coordination, establish explicit ownership and acknowledged quiescence. Existing tests do not close that concurrency risk.

### 11. Power polling and backoff – implicit

Owner: `seenRevision`, `lastCycle`, `waitMs` and `failures` in the reader. Ready cycles select Demo, TokenMissing or Live. Demo/token waits are one second; all-source failures back off with a capped counter and 60-second ceiling. Any valid source resets failures; absent Wi-Fi retries soon. Changed revisions trigger an immediate next eligible cycle.

Preserve last numeric values with invalidity on failed reads, and never publish old-revision results. A future worker harness should exercise revision/pause changes during each request and partial-source recovery; portable parsing tests alone do not cover scheduling.

### 12. Daily-total scheduling – implicit

Owner: `totalsRevision` and `lastTotals` in the reader. After all six power requests, changed configuration or at least 60 seconds since the last attempt makes totals due. Empty mappings clear the corresponding reading; failed reads retain numbers but invalidate them. Totals remain usable through 600 seconds of receive age.

Do not treat this as an exact wall-clock timer. Add cancellation between totals if worker coordination is revised. Portable energy tests cover usability, not this network lifecycle.

### 13. Weather startup, refresh and failure – implicit

Owner: `haCycleDone`, `weatherRevision`, `lastWeather` and report validity in the reader; age rules in [`include/weather.h`](../include/weather.h). With a token, weather waits for one completed, published Wi-Fi-connected HA cycle, successful or not. Without a token, weather can run independently. Valid reports refresh after 30 minutes, invalid reports retry after five minutes, and receive age expires after three hours. Absent Wi-Fi does not count as an attempted fetch.

A failed attempted request invalidates immediately. Weather starts after the HA connection closes; demo skips network weather. A worker lifecycle harness and hardware heap/network evidence are still needed for scheduling/coordination changes.

### 14. Reading freshness and derived solar – computed

Owner: [`include/energy_model.h`](../include/energy_model.h) and `include/weather.h`. Missing, valid-known-age, valid-unknown-age, invalid-last-known, receive-stale, report-stale/future and offline states depend on flags and timestamps. The heartbeat adjusts known report times for unchanged related sensors. Solar additionally requires coherent DC/battery inputs and stays unavailable if either fails.

Keep these as shared computed rules, not another mutable status flag. Unknown source age must stay visible. Energy, history and flow-status tests cover signs, units, timestamps, rollover, heartbeat and coherence. Independent HA integrations sharing a heartbeat remain a documented modeling limitation.

### 15. HTTP response acquisition – implicit in client and sink

Owners: `Connection`, `LimitedBody` in the reader and [`include/body_limit.h`](../include/body_limit.h), alongside pinned HTTPClient framing. Preconditions lead to headers, bounded chunk-aware body collection, parse/value validation, then publication or socket cleanup. Oversize refusal is sticky. TLS watchdog removal/restoration follows the connection lifetime through RAII.

Open findings R1/R2 remain: a connected silent body can outlive the sink deadline, and `std::string` allocation failure is not recovered. Fix at the actual transport/allocation layers with failure injection; adding an enum around the call cannot make it cancellable. Preserve HTTP/1.1, chunk handling and secure CA verification. Current body tests establish byte limits, not idle deadlines or allocation recovery.

---

## History and persisted settings

### 16. Sampling and retention – implicit bucket lifecycle

Owners: [`src/history_service.cpp`](../src/history_service.cpp) and [`include/history_model.h`](../include/history_model.h). Clock unknown means no samples. A current empty five-minute bucket can accept the first usable actual snapshot; demo and invalid readings do not fill it. Expiry and backward clock corrections prune records; a mapping identity change clears history.

Keep source-invalid values as gaps, and evaluate ages after copying the worker snapshot. Existing history tests cover capture, bucket retention, clock correction, demo exclusion and gaps.

### 17. Restore and alternating checkpoints – mixed

Owner: `RestoreState`, `mounted`, `healthy`, `dirty`, `activeBank` and checkpoint generations in the history service. Boot mounts or formats only a blank partition, verifies banks, selects the newest valid generation, then compares identity. Unavailable storage uses RAM. Dirty state writes the other bank; only read-back verification commits its identity/generation and clears dirty. Failure keeps the previous bank and rate-limits retries.

Restore outcome describes boot provenance; storage health is a separate changing property. Existing codec tests cover corrupt/truncated writes and fallback semantics; full filesystem/NVS power-cut acceptance still needs hardware.

### 18. Configuration migration and commit – implicit transaction

Owners: [`src/config/monitor_config.cpp`](../src/config/monitor_config.cpp), settings validation in [`src/web/monitor_web.cpp`](../src/web/monitor_web.cpp), and [`src/storage_service.cpp`](../src/storage_service.cpp). Defaults can become legacy schema-1 migration or validated schema-2 settings. New settings are validated and serialized before NVS; current settings/debug level change only after a successful write. Then history identity and reader revision are reconfigured. Device identity/access code and touch limits have separate persistence lifecycles.

Preserve optional-field compatibility, Off=0, write-only token semantics and stable source indices. Real extracted validation/configuration harnesses cover debug/night migration and failed writes; they do not execute every complete HTTP route or physical NVS failure.

---

## Browser request and presentation lifecycles

All entries in this section are owned by [`src/web/web_page.h`](../src/web/web_page.h).

### 19. Navigation and settings form – implicit

Hash routing chooses one visible section with dashboard fallback; invalid hidden fields route to their section. Configuration loads populate one shared form across sections. Submit validates night times, posts, clears token controls on success, reloads settings and requests live/history refresh. Errors display a toast; `toastTimer` replaces the previous five-second notification timeout. Night controls derive hidden/disabled fields from the selected mode and master enable, preserving unused saved times.

Successful saves now invalidate history immediately before any awaited reload. Failed saves preserve it. `tests/test_web_settings.js` covers the actual submit/load code. Duplicate submissions and edits overlapping a pending reload remain unguarded; a future change should define dirty/loading/saving ownership and preserve unsaved edits.

### 20. Live status request ordering – compact sequence model

Owner: `pollSeq`, `shownSeq`, `polling` and `last`. Ordinary polls allow one request; forced polls may overlap. Only responses at least as new as the one already shown may render or report failure. The `api()` helper aborts status after 12 seconds and other small requests after 15 seconds. Failures grey values and stop flows; recovery restores fresh rendering.

`tests/test_web_dashboard.js` covers deadlines, overlap, superseded replies and recovery. These are per-request deadlines, not an exact age guarantee since the last successful reply. The ordering rule also permits an older request to render while a newer request is still pending; it only rejects replies older than one already accepted.

### 21. History request ownership – explicit generation model

Owner: `historyRequest` with `idle`/`loading`, a settings generation, a coalesced follow-up flag and the active promise. Normal requests cache for one minute. Forced requests during loading queue one fresh fetch and await the chain. Successful settings saves advance the generation, clear the chart and reset its cache; old replies and old errors cannot publish afterward.

`tests/test_web_history.js` now drives deferred requests through save-before-reply, save-after-reply, old failure, repeated forced refreshes, fresh failure and recovery. It verifies at most one active history request. Together with the settings harness this closes review R3 at the source/host level. Device API identity handling still has its own storage acceptance requirements.

### 22. Tiles, optional fallbacks and animation – computed

Freshness and values select Live/Waiting/Demo, flow direction/idle, stale last-known values and forecast/weather fallback. Every optional tile is rewritten on each response. Disconnect stops every arm and greys values; reduced motion uses fixed comet positions. History selection/resize rerenders the cached chart and preserves gaps and field units.

Keep presentation derived from accepted data. Dashboard and history suites exercise these transitions, with portable renderer checks for the panel. Avoid introducing persistent status booleans that can disagree with the readings.

### 23. Place search – implicit, incomplete ownership

Search starts a direct Open-Meteo fetch, populates `places` and results, then selection fills coordinates/place/timezone; Clear removes location inputs. There is no deadline, generation or cancellation. Concurrent searches can resolve out of order, and a response after Clear can repopulate results.

This is a static finding, not a reproduced host failure in this change. A focused future correction should invalidate older searches, bound requests and test slow-old/fast-new replies plus Clear while pending. No external place search or owner telemetry was used for this audit.

### 24. Browser firmware upload – implicit, uncertain outcome

Owner: `busy` and `uploadStatus`. Confirmation/file selection enters uploading and suppresses subsequent status/history requests. Success stays busy through restart; error clears busy. A direct fetch has no upload-specific deadline/progress model. A lost response can mean installation succeeded even when the browser reports failure. An already pending status reply is not explicitly invalidated by upload start.

This is a design/acceptance gap, unchanged here. Model selecting/uploading/restarting/uncertain/recoverable failure alongside device update ownership before changing it. Use an upload-appropriate inactivity policy and real invalid/interrupted/successful firmware acceptance.

---

## Device service and diagnostic coordination

### 25. Web firmware upload – implicit, shared-resource risk

Owner: `uploading`, `uploadOK`, `uploadAuth`, `updateError` in `src/web/monitor_web.cpp`. Start checks the mutation header, checkpoints and requests reader pause, then begins Update. Write errors latch refusal; end validates or aborts/resumes; success leaves the reader paused until reboot. Abort cleans up. The final route checks upload outcome before scheduling restart.

The flags admit combinations whose legality depends on callback order. Pause is not acknowledged quiescence, and ArduinoOTA has separate update ownership. No concurrent-update corruption was reproduced. A safe follow-up needs one update owner across transports, start/write/end/abort tests on the actual handler, and hardware failure acceptance.

### 26. Service activation and controlled restart – implicit latches

Owners: `started`/`rebootAt` in `monitor_web.cpp`, OTA `enabled` in [`src/ota_service.cpp`](../src/ota_service.cpp), `timeStarted`/applied timezone in [`src/main.cpp`](../src/main.cpp). First Wi-Fi connection enables HTTP, ArduinoOTA/mDNS and SNTP. Timezone changes reapply TZ independently of starting SNTP. Web restart/Wi-Fi reset schedules reboot after checkpoint; Improv and ArduinoOTA have their own restart paths. ArduinoOTA start checkpoints/requests pause; error resumes it.

An enabled service does not prove current connectivity. Keep screen-off unrelated to all these lifecycles. A later update coordinator must cover ArduinoOTA and web upload together, including errors and reboot-pending ownership. Real network and update acceptance remains required.

### 27. Startup and runtime diagnostics – mixed

Owners: [`include/boot_status.h`](../include/boot_status.h), [`include/debug_levels.h`](../include/debug_levels.h) and [`src/serial_diagnostics.cpp`](../src/serial_diagnostics.cpp). Startup rows change until ready or a 60-second deadline; closing the boot screen is an independent latch. Clock ticks/RSSI jitter do not repeat milestones. Runtime changes and unseen cycles feed a log gate, with level changes, minute cadence and Verbose/Off behavior.

Retain diagnostics even when rendering or logs are disabled. Existing boot/debug suites cover deadline, jitter, rollover, duplicate suppression and level transitions. Diagnostics must never imply physical self-test or leak tokens, passwords or device codes.

---

## Finding register and verification

The [8 November review](REVIEW_2026-11-08.md) remains the historical source of R1–R3. R1/R2 stay open; R3 is fixed here with permanent deferred-request regression coverage. The Wi-Fi delayed-servicing case is fixed in entry 7. Place-search ordering, form request ownership and update quiescence are static findings/design gaps, not claims of reproduced device failures.

Run `python3 tools/check_docs.py`, `python3 tools/check_host.py`, all three `node tests/test_web_*.js` suites individually, and the supported firmware builds. The host runner now includes `tests/test_lifecycles.cpp`. It tests the policies used by the app, not physical WiFiManager, touch or update drivers. Review the actual browser fixture for dashboard changes and retain unchecked physical cases in [`docs/HARDWARE_VALIDATION.md`](HARDWARE_VALIDATION.md).

No new framework or dependency is needed. Future work should keep the owner/rules/tests for a lifecycle together and update this inventory when transitions change. Do not collapse independent states such as navigation, night brightness, storage health and worker pause into one global machine.

### Pre-version-bump verification on 10 November 2026

`python3 tools/check_docs.py`, `python3 tools/check_host.py`, the three browser regression suites, the embedded script syntax check and `git diff --check` passed. Host checks included the installed-header history/debug/night integration harnesses, Improv/body-limit sanitizers and the new panel/Wi-Fi policy tests. Regenerated previews had no tracked image changes.

All five supported PlatformIO environments built successfully, including app/merged images, part maps and the flash-budget guard. Exact app-image sizes from this build are below; the OTA slot is 1,835,008 bytes and the policy floor is 65,536 free bytes.

| Environment      | Image bytes | Free bytes |
| ---------------- | ----------: | ---------: |
| cyd28            | 1,648,288   | 186,720    |
| cyd24            | 1,648,288   | 186,720    |
| e32r28t-st7789   | 1,648,256   | 186,752    |
| e32r28t-ili9341  | 1,648,368   | 186,640    |
| cyd40            | 1,646,016   | 188,992    |

The local browser fixture rendered the dashboard/history, navigated to Display, confirmed a successful in-memory settings save and showed the reloaded history afterward. Browser evidence covers this ordinary save path; deferred-request tests establish the race behavior. The fixture uses the existing documented history capture and synthetic live/config data. No new owner telemetry was collected.

No physical device was flashed or contacted. Portal RF behavior, calibration, touch/night scheduling, transport failure injection, NVS/LittleFS power cuts, runtime heap, both updater transports and endurance remain subject to the existing hardware acceptance checklist. No release, commit or push was created.
