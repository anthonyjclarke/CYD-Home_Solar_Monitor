# 24-hour history – implementation and validation

Included in source release `0.9.0` on 5 November 2026. Desktop fixtures use synthetic data only. No device was flashed and no live Home Assistant or inverter was contacted.

---

## Sampling and retention

A single RAM store supplies both the CYD and `GET /api/history?hours=24`. It retains 288 UTC five-minute slots, including the current partial slot. Once per second, the main task checks for an unfilled current slot and captures the first snapshot with at least one usable field. A filled slot is immutable. These are instantaneous snapshots, not averages over five minutes; a field missing at capture remains missing for that slot even if it recovers later. Fast transients can be missed.

The 13 fields retain the existing six-source order followed by derived solar, today's solar kWh, tomorrow's solar forecast, current temperature, current WMO weather code, tomorrow's maximum temperature and tomorrow's WMO code. No new upstream requests are made. Home load comes from its existing helper; solar uses the existing coherent DC/battery derivation. Grid remains positive for import and battery positive for discharge. Optional weather and totals retain their existing freshness limits.

Records include UTC slot start, actual device capture time, a per-field validity mask, an upstream-report-time availability mask and 13 floats. Stale, offline, invalid and non-finite fields are absent. Unknown upstream age follows existing transport freshness rules and is explicitly labelled. Demo readings and samples before a sane synchronized clock are excluded. Actual history already collected remains viewable during demo mode.

Missed slots are emitted as explicit null rows; neither display bridges them. No data is backfilled from Home Assistant, and time while powered off stays empty. The oldest partial interval drops as the current UTC slot advances. Graphs span a rolling 24 hours, with up to 288 points; they never reset at local midnight. Backward clock corrections discard future capture times, while forward corrections leave gaps and expire old slots. UTC storage avoids daylight-saving ordering problems; panel labels use the configured timezone and browser labels use the browser timezone.

A CRC32 identity covers HA URL, all source and total mappings, and coordinates. Changing any of those clears the entire history immediately. Labels, tokens, certificates, poll/stale settings, serial debug level, timezone and display preferences retain it. This intentionally avoids silently combining different installations or locations; CRC32 is an identity/checksum mechanism, not cryptographic authentication.

---

## Storage and recovery

The existing 384 KiB `spiffs` data partition is mounted with Arduino LittleFS. Partition offsets and sizes, both 1,835,008-byte OTA slots, NVS settings schemas and calibration namespaces remain unchanged. No filesystem image upload is needed. If mounting fails, initialization formats only after every byte of the partition has been verified as erased. Nonblank mount failure keeps flash untouched and collects in RAM with a visible storage warning.

The RAM records occupy 18,432 bytes, plus small service state. Each bank file is 18,460 bytes: a 24-byte header, 288 explicitly encoded 64-byte records and a trailing CRC32. The header includes magic, format version, capacity, field count, identity and generation. All integers and float bits use explicit little-endian encoding without compiler padding. Restore rejects wrong size/version/shape, invalid masks/timestamps, non-finite floats, trailing bytes and CRC failure.

`/history0.bin` and `/history1.bin` alternate. A checkpoint writes the inactive bank, flushes/closes it, then reopens and verifies its full content before promoting it. The other bank remains intact. Boot validates both and loads the newest valid generation, falling back to the older valid bank when a write was interrupted. A mapping mismatch starts empty instead of loading an older matching mapping. Invalid banks are reported and can be replaced by new checkpoints; a mount failure never triggers a destructive recovery format.

Checkpoints occur every 15 minutes while dirty, measured with rollover-safe monotonic time. Failed attempts are rate limited too. A full day produces at most 96 scheduled checkpoints, approximately 1.77 MB of logical file writes before filesystem metadata and wear levelling overhead. This batches writes rather than persisting every poll; flash lifetime and write pauses require real-board measurement. Controlled WebUI reboots, Wi-Fi reset, Improv restart and app OTA start request an additional checkpoint.

When storage is healthy, unexpected power loss can lose roughly the last 15 minutes of new samples; failed saves can lose more. Filesystem repair, flash wear and power-failure behavior cannot be proven by host codec tests. App-only updates preserve the data partition. A filesystem upload or full flash erase removes history. A merged clean USB image overwrites NVS but does not guarantee erasure of history. Local telemetry is not encrypted, consistent with existing project storage.

---

## Display and API

The CYD renders into its existing 320 × 240 RGB332 sprite. History has five primary tabs, latest sampled value/time, coverage count, sampled minimum/average/maximum and a signed graph. Prev/Next reaches all other fields. Raw WMO codes are categorical points without an average or interpolated line. Both light and dark themes use exact RGB332 palette levels. Missing data, no clock, storage failure and demo exclusion have explicit states. Offline flow screens retain the History button.

History replaces the flow screen Settings button. A hold longer than two seconds opens Settings once, with progress feedback and no tap on release. A separate hold after releasing in Settings retains calibration. A Settings tap returns to flows; the first night-dimmed contact is consumed entirely. Full-screen and motion refresh limits are preserved.

The read-only history endpoint adds schema version, interval/duration, UTC start/end, clock/storage status, `persistedThrough`, demo exclusion, device timezone name, field names/units and 288 chronological rows when the clock is known. Each row has `slot`, actual capture time `t` or null, `sourceTimeKnown` and a 13-element `v` array with per-field nulls. Without a clock, it returns no rows and keeps restored records hidden. `persistedThrough` is the newest capture in the last verified checkpoint, not the time of the flash write. Only the 24-hour window is supported; another `hours` value returns 400.

The response streams in roughly 1 KiB chunks instead of allocating a complete JSON document and response body. All history operations run on the main Arduino task, so rendering and HTTP see one consistent store without adding a second large buffer or worker locks. The HA worker, existing API routes, configuration validation, mutation header, OTA app upload and normal controls retain their existing contracts.

The WebUI fetches history every minute and after a settings save, independently of its three-second live status refresh. Default power flows share a signed kW scale; the selector shows every other metric on its own unit scale. Browser reloads fetch retained device records. Dots have value/time tooltips, gaps break paths and unknown upstream age is called out. Individual numeric metrics show sampled statistics; codes do not.

---

## Changed files

The implementation adds `include/history_model.h`, `include/history_service.h`, `src/history_service.cpp`, `include/history_layout.h`, `src/ui/history_layout.cpp` and `include/touch_gesture.h`. Runtime integration changes are in `src/main.cpp`, `src/app/app.cpp`, `src/ui/flow_layout.cpp`, `src/web/monitor_web.cpp`, `src/web/web_page.h`, `src/ota_service.cpp` and `src/network/improv_setup.cpp`. Only explanatory comments change in `partitions_custom.csv`.

Regression and fixture work adds `tests/test_history.cpp`, `tests/test_web_history.js`, `tests/history_response_harness.cpp`, `tests/support/Arduino.h` and `tools/check_history_response.py`, and adjusts `tools/check_host.py`, `tools/render_preview.cpp` and `tools/preview_server.py`. Shared renderer previews under `docs/images/` now show History buttons; a synthetic WebUI screenshot and five additional history SVG/PNG fixtures cover solar, battery, empty, dark and weather views.

User and developer guidance is updated in `README.md`, `CHANGELOG.md`, `AGENTS.md`, `docs/index.html`, `docs/DEVELOPMENT.md`, `docs/REVIEW_AND_ROADMAP.md` and `docs/HARDWARE_VALIDATION.md`. Existing documentation edits present before this work are preserved. Generated `src/web/handbook_data.h` remains ignored and is not a source change.

---

## Validation and remaining hardware checks

`python3 tools/check_host.py` compiles with warnings as errors, runs existing energy checks and the new history suite, then renders both layouts, boot and history fixtures. History tests cover signed values, partial failure, coherence, stale/future/unknown reports, absent clock, demo exclusion, receive-time rollover, statistics, gaps, more than 24 hours across midnight, backward clock changes, round-trip recovery, corrupt header/payload/checksum, trailing data, failed writes and every possible truncation point of a checkpoint. Gesture tests cover short noise, taps, the strict two-second boundary, one hold per contact, release suppression, night wake and timing rollover.

After PlatformIO installs its libraries, `python3 tools/check_history_response.py` compiles the actual `getHistory()` handler with the pinned ArduinoJson Arduino String writer and host String/HTTP stand-ins, then parses the reassembled response. It covers 288 full, all-missing and mixed rows across packet boundaries, quoted metadata, no clock and accepted/rejected `hours` values. `tools/check_host.py` runs this integration check when those library headers are available. This catches firmware serialization behavior that a synthetic browser fixture cannot: ArduinoJson replaces a String destination, so array commas must be appended outside the serialized row string.

`node tests/test_web_history.js` exercises the actual embedded chart functions with a synthetic API: shared endpoint usage, fetch throttling, missing-data path breaks, signed axes, unit selection, statistics and clock/storage/demo/fetch-failure states. Extracted dashboard JavaScript is syntax checked. Both themes and empty/signed/weather CYD layouts are visually inspected with generated fixtures. The release sweep checked the synthetic WebUI in light and dark themes at desktop width for metric selection, gaps, settings navigation and reload persistence; mobile acceptance remains pending as recorded in the release verification report. The README and handbook screenshots were retaken from this fixture on 5 November 2026.

`pio run -e cyd28` builds the app, merged image and installer part map against the pinned toolchain. Final source-release build sizes, including night scheduling, startup diagnostics and refreshed handbook assets, are recorded in [RELEASE_0.9.0.md](RELEASE_0.9.0.md). The app must fit the unchanged 1,835,008-byte OTA slot. Build success measures static use, not peak runtime heap with the display sprite and TLS active.

All real-device history tests remain unchecked in `docs/HARDWARE_VALIDATION.md`, including first-use mount/format, actual HA sampling, outage and mid-write power cuts, NTP recovery, 24-hour soak, touch targets, watchdog/heap/latency measurements, and retention/configuration across both OTA paths and app slots. The source release is committed locally; publication, hardware flashing and live configuration changes are separate work.
