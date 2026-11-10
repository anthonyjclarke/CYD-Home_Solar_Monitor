# Version 1.0.2 post-flash test plan

Prepared 10 November 2026 for the next patch release. All physical cases below are **not run**. Use this plan after flashing the correct board image; it does not perform or authorize flashing, erasure, live Home Assistant changes or publication.

Scope: explicit panel navigation, independent Wi-Fi portal retries, generation-safe history refresh, mirrored-axis calibration, board identity logging and four additional board profiles. See the [changelog](../CHANGELOG.md) and [27-group state-machine inventory](STATE_MACHINES.md). Add dated hardware results to [HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md); live installer results also belong in [WEB_INSTALLER.md](WEB_INSTALLER.md).

---

## Preparation and run sheet

Use stable power, physical USB recovery access, serial at `115200` baud, a browser with developer tools and a device on the trusted LAN. Record the exact environment: `cyd28`, `cyd24`, `e32r28t-st7789`, `e32r28t-ili9341` or `cyd40`. Have the previous known-good matching-board app image available locally.

Privately note the original version/app slot, settings, night schedule, timezone, mappings, location enablement and whether history/calibration exist. The token is write-only; `/api/config` does not back it up. Keep needed credentials in their existing secure location. Preservation comparisons require an app-only update: a merged image overwrites NVS. Do not commit real telemetry, raw captures, device codes or credentials.

Use a spare board or isolated test setup for invalid credentials, mapping/location changes that reset history, interrupted updates, clock changes and storage faults. An ordinary production smoke test must not erase the baseline it is supposed to verify.

| Field             | Value                       |
| ----------------- | --------------------------- |
| Date/time/tester  | Not recorded                |
| Board/environment | Not recorded                |
| Image source/SHA  | Not recorded                |
| Prior version     | Not recorded                |
| Flash method      | Not recorded                |
| Prior/new slot    | Not recorded                |
| Observed version  | Must be 1.0.2               |
| Browser/device    | Not recorded                |
| HA transport      | HTTP or verified HTTPS      |
| Restored baseline | Not recorded                |

For each case record ID, Pass/Fail/Blocked/Not run, elapsed time, expected versus actual behavior and a redacted local evidence reference. Record failure/reset details before rebooting. Never infer Pass from a successful build. Every case starts Not run; fill in this execution record:

| Case | Result  | Date/build | Evidence/reason |
| ---- | ------- | ---------- | --------------- |
| PF01 | Not run | Pending    | Pending         |
| PF02 | Not run | Pending    | Pending         |
| PF03 | Not run | Pending    | Pending         |
| PF04 | Not run | Pending    | Pending         |
| PF05 | Not run | Pending    | Pending         |
| PF06 | Not run | Pending    | Pending         |
| PF07 | Not run | Pending    | Pending         |
| PF08 | Not run | Pending    | Pending         |
| PF09 | Not run | Pending    | Pending         |
| PF10 | Not run | Pending    | Pending         |
| PF11 | Not run | Pending    | Pending         |
| PF12 | Not run | Pending    | Pending         |
| PF13 | Not run | Pending    | Pending         |
| PF14 | Not run | Pending    | Pending         |
| PF15 | Not run | Pending    | Pending         |

---

## First 15 minutes

Run PF01 and PF02, the ordinary save in PF10, reconnect in PF06 and the live-reading comparison in PF08. Verify retained touch calibration before changing it. Stop for wrong pins/colours, boot loops, lost settings after an app-only update, repeated resets or a stuck reader.

This smoke pass does not establish portal timeout/reopen, delayed history-response ordering, night transitions, all updater transports, extra board acceptance or endurance.

### PF01 – Identity, preserved settings and startup

Power on with serial already attached. Confirm `[BOOT]` says `1.0.2`, `[BOARD]` identifies the intended driver/touch bus and Device/Diagnostics and the embedded handbook agree. Compare saved Wi-Fi, token presence, six mappings, brightness, timezone, label, debug level and calibration with the baseline. Check checkpointed history restores after clock sync.

Watch the seven startup rows and serial milestones. Tap Boot: it must only skip Boot. On another boot, hold longer than two seconds to open Settings, then wait beyond the boot grace period: Settings must remain open. With a missing service, the diagnostic window ends after about 60 seconds while reconnection continues, without per-frame log flooding.

Pass: identities agree, the baseline survives an app-only update, no unexpected reset and boot completion cannot override navigation. Recovery: record reset/version details and use the established USB procedure with the known-good matching-board app image if the build cannot boot.

### PF02 – Navigation, gesture consumption and both layouts

In Centre and Cards, exercise Flow → History, each metric tab, Prev/Next, Home → Flow and hold → Settings. A hold release must not also tap. Tap Settings at the History-button position: that contact only returns to Flow. Repeat from History and during continuous animation.

Check labels, colours, directions, clipping and hit targets. Settings must refresh without flashing; Centre animation must not make touch/HTTP unusable. On `cyd40`, every screen fills the 480 × 320 panel natively: the centre flow has a right rail with the History button, and there must be no seam or clipped glyph at the y = 160 band boundary. Targets follow `ui::targets480`. Check RGB remains off and ESP32-32E speakers remain silent.

Pass: one screen and action per contact, correct geometry/themes and usable HTTP/touch. Recovery: restore the baseline layout; record failed hit targets before recalibrating.

### PF03 – Calibration completion and timeout

Open Settings, release, then hold again for more than two seconds. Keep holding briefly: the initiating contact must not capture target one. Release and tap each of four crosses. Verify save/reject feedback, return to Settings and correct taps after restart. On a mirrored-axis board, calibration must correct the axis.

Re-enter, wait more than 45 seconds without completing targets and verify return with previous limits retained. Also observe timeout during a held contact; record unexpected navigation/target behavior as an existing acquisition edge case rather than assuming this release fixes it.

Pass: only a completed valid calibration persists and timeout preserves the previous one. Recovery: carefully recalibrate through Settings using physical USB access if the mapping becomes hard to use.

### PF04 – Scheduled dim/off/on and wake

Note the original settings. After the first awake minute and clock sync, choose nearby minute boundaries to test Dim only, Off only and Dim then off. In staged mode place Off between Dim and On. Verify dimming, zero backlight and a complete frame at scheduled On. Restart to check minute precision, mode and enable persist.

First tap or long first contact on dim/off must only wake for one minute. Release, then use another contact to navigate. While Off, verify HA acquisition, history, WebUI and serial continue. Missing-clock, midnight and DST cases belong to LAB02; do not change the household clock for simulation.

Pass: boundaries and wake consumption match the handbook and services continue while Off. Recovery: disable night mode in the browser, restore brightness and the original schedule.

---

## Network and source transitions

### PF05 – Offline boot, portal timeout and retry

On a spare/unprovisioned board or a device isolated from its saved network before a new boot, verify Boot exits offline at about 15 seconds and the password-protected AP appears after about 20 seconds from Wi-Fi start. Join the AP and open the portal. Verify name/code locally; neither password nor device code belongs in logs.

Do not enter credentials. Observe the 180-second timeout, note closure, then verify reopening after about another 20 disconnected seconds plus loop/driver overhead. Repeat once, then provision successfully and confirm AP closure. Try a wrong password only on the test setup. Joining an AP, not a cached browser page, establishes availability.

Pass: repeated setup remains reachable before first connection and success closes AP. Portable tests cover delayed loop servicing/rollover; this normal hardware run does not prove every delayed boundary. Recovery: restore the test network or provision through USB; do not reset production Wi-Fi merely to run this case.

### PF06 – Outage after first connection

After a successful connection, interrupt only the device's Wi-Fi path for at least four minutes, then restore it. Check stale/unavailable presentation, stopped flows, usable local touch/screens and reconnection attempts. No setup AP may reopen during that boot, regardless of how the initial connection was made.

After recovery, check HTTP by IP, live readings, weather eligibility and history without restarting. Outage samples remain gaps. Cached weather need not become invalid merely because no request was attempted.

Pass: automatic recovery, no post-connection AP and no fabricated history. Recovery: inspect serial/reader state before reboot, restore network availability and record if manual reset was necessary.

### PF07 – USB Improv and credential rollback

Use the intended installer/Improv client on a test board. Query info and confirm project/version identity. Provision valid credentials while AP is active; observe the provisioned reply/device URL before restart, then connection with application settings/history retained. Verify Improv queries also work during live polling/logging.

On an isolated board with a known working saved network, attempt invalid credentials. The roughly 15-second connection attempt must restore previous credentials and leave recovery possible. Never include passwords/codes in evidence. Malformed/truncated frame memory safety remains covered by the host sanitizer suite; do not fuzz the installed monitor.

Pass: reply delivery, provisioning and rollback work. Recovery: restore valid credentials through AP/USB and record a missing reply or reconnection failure.

### PF08 – Source validity, signs, failures and TLS

Compare six source values and derived solar with existing HA readings at matched times. Verify grid/battery signs and `solar = max(inverter_DC - battery_internal, 0)`. An unchanged idle battery stays usable when related reports continue. Unknown reporting age remains labelled.

On a controlled endpoint/test mapping, test an unavailable entity, wrong unit, 401/404 and stale/skewed DC/battery timestamps. A failure must not invent zero or leave its arrow active, and later fields must still read. Restore valid data and observe recovery/backoff. Silent bodies are the separate open LAB01 issue.

For HTTPS HA, verify valid CA/clock operation and invalid CA rejection in isolation. Never use insecure TLS or send a real token to a mock host. Pass: values/freshness/directions agree with diagnostics and recover. Recovery: restore URL/mappings/CA/token locally; mapping changes reset history.

### PF09 – Totals, weather and optional fallbacks

Observe totals after the six source reads and at roughly minute intervals of eligible cycles, with kWh units. Clear a test optional mapping: old values/pills must clear on the next status, with forecast-to-weather fallback when available. Restore original mappings afterward.

Check weather after boot, with failing HA but connected Wi-Fi and with no HA token in a controlled setup. With a token, weather waits for a completed HA cycle, successful or not. A failed attempted weather request invalidates immediately. Clear location and verify weather disables/clears; restore it and verify re-enable. Observe normal refresh around 30 minutes and failure retry around five minutes, allowing for polling/network time. Weather remains plain HTTP by design.

Pass: no stale optional value looks current and source/weather dependencies match the handbook. Recovery: restore totals/location/token presence and timezone; use a test board when identity changes would erase wanted history.

---

## Browser, configuration and history

### PF10 – History save race and failed-save preservation

Save only a label/layout first: the chart briefly clears, fetches current history and preserves retained device records. Observe `GET /api/history?hours=24`: ordinary fetches are minute-cached and successful saves bypass that cache. Verify units, gap paths and selected metric after refresh.

For the race, use response throttling or a local test proxy that delays **only the history response**, allowing settings/config/status to proceed. Start an old GET, save a new test mapping/location, then release the old response. The prior chart must clear when save succeeds and never repopulate from the old reply. One queued follow-up must fetch current history after the old request settles, without concurrent history GETs. Empty history is correct after an identity change.

Repeat with an old request failure and repeated forced-refresh triggers. Reject a save, for example with stale time below three times poll: existing chart/settings must remain intact. Also save after an old GET has completed; its cache must not suppress the fresh GET. Whole-device throttling does not establish this ordering; if selective delay is unavailable, mark the race portion Blocked and cite the deferred-request host regressions.

Pass: no old-generation data/error after success, one fresh follow-up and failed-save preservation. Recovery: remove throttling/proxy and restore the original mapping/location; restoring identity starts empty history again, so use a test setup if retention matters.

### PF11 – Sampling, checkpoints and restart retention

Collect actual data for at least 20 minutes, spanning five-minute buckets and a 15-minute checkpoint. Compare panel/browser history for units, signs, original timestamps, fields and gaps. Restart through the device page and verify completed checkpoint restoration after time sync.

On a test setup enable Demo for at least one bucket, then disable it: older real history may remain, synthetic samples must not persist. Label/layout/timezone/token-only changes retain history; URL/source/total/coordinate changes start empty. Uncontrolled power loss can lose newer RAM samples; see LAB02 for fault tests.

Pass: usable actual data only, no source mixing and completed saves return after controlled restart. Recovery: restore baseline settings and record storage warnings; never format corrupt nonblank storage to hide a failure.

### PF12 – Persistence, serial levels and mutation guard

Save from every settings section, including Off=`0`, each night mode and unused disabled time fields. Blank token preserves the saved token; explicit clear removes it only on a disposable configuration. Restart and compare settings/calibration to baseline. Browser validation must reveal the section containing an invalid hidden input.

At Info verify minute-level routine logs/change events. Save Off, verify application logs stop, then save Verbose and verify every-poll detail. Restart with each selected level. Treat bootloader/library output separately; credentials/codes/raw bodies must not appear in application logs.

On a test configuration, send a mutation without `X-CYD-Request: 1`: expect 403 with no changes. Repeat a valid save with the header. This is a browser cross-site guard, not LAN authentication. Pass: successful saves persist, rejected saves preserve state and levels match. Recovery: restore settings/debug level; do not share credential-bearing request/network exports.

### PF13 – Browser disconnect, routing and place search

Open by IP and `solar.local`. Navigate all sections and Back/Forward, change chart metric and resize at phone/desktop widths in light/dark. Units/legend remain correct. Reduce Motion leaves comets still; normal animation follows power direction.

Interrupt browser access during a pending status request. The per-request deadline is about 12 seconds; failure greys last values and stops flows, recovery restores them. Timer ticks and roughly five-second macOS mDNS lookups can add delay; distinguish that from a frozen device.

Test ordinary place search/selection/Clear without saving over the baseline. Optionally test slow-old/fast-new searches and Clear while pending: ordering remains a documented unresolved browser gap. Restore inputs/reload afterward. Pass: normal routing/rendering/status recovery work; log any search race separately without claiming this release fixes it.

---

## Maintenance, duration and acceptance

### PF14 – Updates, restarts and installer

On a test board with USB recovery, record app slots before/after browser firmware upload and ArduinoOTA separately. Use the matching-board `firmware.bin`, stable power and only one active updater. After restart check `1.0.2`, settings, code, calibration and checkpointed history. Include OTA service start after late Wi-Fi connection and operation while the backlight is Off.

Test invalid/empty/interrupted upload only in isolation; verify rejection or safe recovery and resumed acquisition. Do not deliberately upload a merged image or cut production power. If a valid upload loses its completion response, inspect device/version before retrying: outcome may be uncertain. Reader pause is not idle acknowledgement and concurrent updater ownership is unresolved; production concurrency is not an acceptance experiment.

Test controlled Restart and Wi-Fi reset on a disposable network. Wi-Fi reset should return to setup while retaining application settings/calibration. Complete the three outstanding live installer cases in `WEB_INSTALLER.md`, including update from `app1`, with published credential-free CI artifacts when available. Mark unavailable artifacts/boards Blocked or Not run; local builds do not prove live installer behavior.

Pass: each tested transport boots the intended version/slot with settings retained and failures are recoverable. Recovery: use the previous known-good matching-board app image through USB, avoiding merged images when preservation is needed. Record forced reset/lost settings as failures.

### PF15 – Endurance and per-board acceptance

Run at least 24 hours, preferably 48, spanning night transitions/weather refreshes. At Info record local heap/minimum-heap, uptime/reset reasons, longest touch/HTTP pause and storage health at start, during weather/TLS and at end. Use multiple browser clients for part of the run. Inspect a complete rolling day and controlled-restart retention.

Pass: no unexpected watchdog/boot loop, growing resource exhaustion or unexplained missing recovery; touch/HTTP stay usable and data gaps remain honest. No universal new heap threshold is established; compare trends with the same-board baseline. Restore original schedule/layout/debug settings afterward.

Repeat relevant checks on each supported board. Four added profiles remain physically unverified until their own display/touch/pins, network/source/history and update checks pass. `cyd28` evidence cannot accept another panel/board.

### LAB01 – Transport, allocation and update ownership

R1/R2 remain open. With a mock endpoint and synthetic credentials, future fixes must test connected silent bodies, partial fixed/chunked bodies, stalled chunk metadata, slow progress, revision/pause cancellation and allocation failure at each growth step. Corrected behavior should be bounded cleanup followed by a successful request. Current sink limits do not guarantee that result.

Use host injection first, then isolated hardware with heap/watchdog observation. Concurrent updater ownership/acknowledged idle requires implementation and a dedicated harness before physical acceptance. Record Blocked against the open finding; a reboot is not evidence of graceful recovery.

### LAB02 – Clock, low memory and storage faults

On a spare board/mock time source test no NTP, time correction, midnight and DST. Expect full brightness without a known clock, UTC-ordered history, future points pruned and correct local scheduling. Inject display-buffer allocation failure and observe continuing diagnostics/services. Check calibration timeout during a held contact.

Test blank-only formatting, nonblank mount failure, corrupt-bank fallback and power loss during alternating checkpoint writes. Expect the newest complete valid bank, no automatic erasure of corrupt nonblank storage and visible RAM-only/storage warnings. Ordinary restarts cannot establish these fault properties.

### Acceptance decision and cleanup

Record per-board outcomes and skipped cases. Release acceptance needs PF01–PF15 where applicable, including normal browser paths and the outstanding installer cases. Do not replace night/provisioning/endurance evidence with a desktop smoke pass. Document any owner-approved exception with its scope/reason. Keep unresolved transport/search/form/update gaps disclosed rather than marking them fixed.

Keep `1.0.2` in prepared status until physical results and the publication decision are recorded. Add dated redacted results to `HARDWARE_VALIDATION.md`; clear installer checks only with live installer evidence. Restore baseline settings/network/debug level, remove test proxies/delays and verify normal operation before leaving.

---

## State-machine coverage map

Numbers refer to `STATE_MACHINES.md`; mappings below are not test results.

| Group | Lifecycle                   | Cases                 |
| ----- | --------------------------- | --------------------- |
| 1     | Screen navigation           | PF01, PF02, PF03      |
| 2     | Boot screen timing          | PF01, PF05            |
| 3     | Calibration                 | PF03, LAB02           |
| 4     | Touch consumption           | PF02, PF03, PF04      |
| 5     | Night and wake              | PF04, PF15, LAB02     |
| 6     | Frame and PWM state         | PF02, PF04, LAB02     |
| 7     | Wi-Fi lifecycle             | PF05, PF06            |
| 8     | Improv parser               | Host sanitizers       |
| 9     | USB provisioning            | PF07                  |
| 10    | Reader pause and revision   | PF08, PF14, LAB01     |
| 11    | Poll and backoff            | PF06, PF08            |
| 12    | Daily totals                | PF09                  |
| 13    | Weather gate and retry      | PF09, PF15            |
| 14    | Freshness and solar         | PF08, PF11            |
| 15    | HTTP body and allocation    | PF08, LAB01           |
| 16    | History sampling            | PF11, PF15, LAB02     |
| 17    | Restore and checkpoints     | PF11, PF14, LAB02     |
| 18    | Settings transaction        | PF01, PF12            |
| 19    | Browser form and routing    | PF10, PF12, PF13      |
| 20    | Live request ordering       | PF13, host JS         |
| 21    | History generation          | PF10, host JS         |
| 22    | Tiles and flow              | PF02, PF09, PF13      |
| 23    | Place search                | PF13                  |
| 24    | Browser upload outcome      | PF14                  |
| 25    | Device upload callbacks     | PF14, LAB01           |
| 26    | Service start and reboot    | PF01, PF06, PF14      |
| 27    | Diagnostics and log cadence | PF01, PF12, PF15      |

---

## Prepared-build verification

Checked on 10 November 2026 against the prepared, uncommitted `dev` source atop `8531932`, with `FIRMWARE_VERSION=1.0.2`. These are local validation builds; published release images must come from credential-free CI. Record the actual image source/hash used for each physical run, because another build can differ.

`python3 tools/check_host.py` passed, including installed-header configuration/history integration checks, portable energy/history/debug/boot/night/flow models, panel/Wi-Fi lifecycle scenarios and Improv/body-cap sanitizer suites. All three browser JavaScript regression suites and embedded dashboard syntax passed. `python3 tools/check_docs.py` and `git diff --check` passed. Boot previews were regenerated for `1.0.2`; the light boot image and final local handbook version/release notes/board instructions were visually or browser reviewed. No real device or HA endpoint was contacted.

Final builds passed for all five environments using the final embedded handbook and referenced previews. Each produced the app image, merged image and part map, and passed the flash-budget guard. The OTA slot is 1,835,008 bytes with a 65,536-byte minimum free policy.

| Environment      | Image bytes | Free bytes |
| ---------------- | ----------: | ---------: |
| cyd28            | 1,656,864   | 178,144    |
| cyd24            | 1,656,864   | 178,144    |
| e32r28t-st7789   | 1,656,848   | 178,160    |
| e32r28t-ili9341  | 1,656,944   | 178,064    |
| cyd40            | 1,654,720   | 180,288    |

Smallest margin: 178,064 bytes, on `e32r28t-ili9341`. These sizes include the 4.0″ native 480 × 320 layout code (10-11-2026), which every env compiles; its 320 × 240 output is unchanged. Static build sizes do not establish runtime heap health. Dedicated-touch boards retain the expected TFT_eSPI undefined `TOUCH_CS` warning.

All PF/LAB physical results remain Not run or pending; no hardware checklist was completed. Nothing was flashed, tagged, committed, pushed or published as part of release preparation. The previous-build results in `STATE_MACHINES.md` remain dated pre-version-bump evidence, not the image sizes above.
