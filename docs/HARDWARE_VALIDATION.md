# Hardware validation

Acceptance record for real CYD hardware, current for `1.0.0`. Ticked items have dated evidence from the owner's board running against live Home Assistant data; unticked items still need a real CYD. A local build or the synthetic browser fixture never completes an item here.

---

## Evidence log

Owner's ESP32-2432S028R, SolarEdge Modbus Multi through Home Assistant, Putney NSW. The `0.9.1` builds are the `1.0.0` code before the version number changed. "Live UI correct" means the tiles, flow, history chart, trimmed picker and resized footer all showed as expected.

| Date       | Build | Observation                              |
|:-----------|:------|:-----------------------------------------|
| 04-11-2026 | 0.8.0 | Centre layout matches preview            |
| 04-11-2026 | 0.8.0 | `http://solar.local/` opens on an iPhone |
| 07-11-2026 | 0.9.1 | Weather TLS `-9984` was heap; now HTTP   |
| 07-11-2026 | 0.9.1 | Weather 200; `min_heap` 64 KB over 4 min |
| 07-11-2026 | 0.9.1 | No Wi-Fi: portal opened; later resets OK |
| 07-11-2026 | 0.9.1 | 24 h `/api/history`: 272 of 288 slots    |
| 07-11-2026 | 0.9.1 | History kept across USB app flash        |
| 08-11-2026 | 0.9.1 | Boot: 6/6 live, weather 200              |
| 08-11-2026 | 0.9.1 | `min_heap` 73.5 KB after boot            |
| 08-11-2026 | 0.9.1 | Status by IP 0.06 s; `solar.local` 5 s   |
| 08-11-2026 | 0.9.1 | Live UI correct, no console errors       |
| 08-11-2026 | 0.9.1 | Boot restored 270 history samples        |

The macOS figure is name lookup, not the device: an IPv6 mDNS query for `solar.local` goes unanswered and the Mac waits 5 s. Forcing IPv4, or using the IP address, resolves in milliseconds. Browser request deadlines (12 s status, 15 s others) sit above it.

---

## Build and desktop checks

- [x] PlatformIO `cyd28` build succeeds and fits the standard OTA app slot.
- [x] App image, merged image and `flash_parts.json` are generated.
- [x] Supplied AC balance and grid/battery sign conventions pass portable checks.
- [x] Solar charge, discharge, grid charging, zero/night, invalid inputs, stale inputs and skewed timestamps pass portable checks.
- [x] Power units, invalid states and percentage limits are checked.
- [x] ISO timestamp offsets, stale/future reports and `millis()` rollover are checked.
- [x] All 22 current SVG previews render: six scenes per layout, weather-only, four boot states and five history states; thirteen PNGs regenerate when the converter is installed.
- [x] Embedded JavaScript passes syntax checking.
- [x] Browser fixture shows sample measurements and saves settings through its mock endpoint.

The browser fixture uses synthetic data. Its success does not validate device authentication, NVS or the on-device HTTP server.

---

## Panel and provisioning

- [ ] Fresh USB flash starts with readable, correctly coloured 320 × 240 output.
- [ ] Panel colours match `docs/images/flow-preview.png` (navy cards, not green); the palette is RGB332-exact.
- [ ] Centre-flow layout matches `docs/images/centre-flow-preview.png`; comets move toward/away from the hub correctly for import/export and charge/supply, smoothly at 20 fps.
- [ ] Switching **Settings → Display → Layout** in the browser changes the panel immediately and persists across restart.
- [x] Boot screen shows Wi-Fi, IP, signal, HA and clock, and closes about 6.4 s after boot when Wi-Fi connects; readings are live by then (first poll at 2 s).
- [ ] Boot screen readable by eye; a tap skips it; with Wi-Fi unavailable it closes at 15 s onto the offline flow screen; a >2-second hold opens Wi-Fi/setup details.
- [ ] Setup page refreshes once per second without visible flashing.
- [ ] At night, the touch that wakes a dimmed panel does not open the setup page.
- [ ] Touch works under continuous TFT redraws; calibration targets map correctly.
- [ ] Saturated/no-response readings do not generate phantom taps.
- [ ] Saved Wi-Fi reconnects, and loss/recovery leaves the device responsive.
- [x] No saved Wi-Fi opens the password-protected setup portal after about 20 seconds (owner, 07-11-2026).
- [ ] Portal timeout and failed-password recovery retain usable setup access.
- [ ] Improv responds to device-info queries and provisions Wi-Fi through the installer.
- [ ] Device access code persists across ordinary restarts.

---

## Home Assistant data

- [ ] Configure the real token locally and confirm the base URL/port.
- [ ] Capture serial diagnostics at `115200` baud and confirm HTTP/value/solar reasons agree with live HA data.
- [ ] Verify log volume and serial Improv responses under live polling on the panel.
- [ ] With Verbose selected, six `[READ]` lines per cycle show low `ms` after the first (connection reused); `report_skew` is small.
- [ ] A 401/404 on one entity does not break the following reads in the same cycle.
- [ ] Inverter AC/DC, home, grid, battery and state-of-energy values match HA within acquisition timing.
- [ ] Calculated solar tracks the documented DC/battery formula and SolarEdge at matched timestamps.
- [ ] A missing, stale or skewed DC/battery input disables solar without a false zero.
- [ ] Grid import/export directions agree during observed import and export.
- [ ] Battery charging/discharging directions agree with observed battery status.
- [ ] Meter data covers the property's three phases as intended by the existing helper.
- [ ] Unavailable entities, wrong units and missing IDs stop the affected arrows without fabricating zeroes.
- [ ] Revoked token produces authentication failure without leaking token/body data.
- [ ] HA shutdown/restart recovers without rebooting the CYD.
- [x] Idle full battery (unchanged 0 W / 100%) stays live; serial shows `reason=fresh-unchanged` and solar is calculated.
- [ ] A reachable HA with stale upstream entities is flagged using report time (whole integration stalled: `heartbeat_age` exceeds the stale threshold).
- [ ] Unknown source reporting time is labelled rather than assumed current.
- [ ] Source timestamp skew and real upstream reporting cadence are understood.
- [ ] Place search fills latitude, longitude, timezone and POSIX rule; saving changes the clock immediately.
- [x] `[WEATHER]` shows HTTP 200 after boot over plain HTTP, about 0.8–1 s (07-11 and 08-11-2026). Still to check: repeats about every 30 minutes; icons match conditions.
- [ ] Weather survives an HA outage and clears when the location is cleared.
- [x] Web page stays responsive while the panel animates at 20 fps: `/api/status` by IP in 0.06–0.09 s (08-11-2026).
- [x] Free heap healthy after boot and weather: `min_heap` 73.5 KB, heap about 96 KB (08-11-2026). Still to check over hours.
- [ ] Battery gauge fill and colour match state of energy (green ≥ 50%, amber ≥ 20%, red below).
- [ ] Daily solar total matches the HA Energy dashboard and resets at local midnight.
- [ ] Forecast value matches the HA forecast entity; `[TOTAL]` lines appear about once a minute.
- [ ] Blank, invalid or unavailable total entities hide the value (clock shown) without affecting live flows.
- [ ] Home Assistant's existing Modbus monitoring continues normally.

---

## Persistence and maintenance

- [ ] Entity mapping, token, brightness, battery label, demo and timing settings persist across power cycles.
- [ ] Updating a 0.1.0 device preserves its token, URL and first five entity mappings and supplies the new DC default.
- [ ] Saving migrated settings retains all six mappings after a subsequent restart.
- [ ] Blank token preserves the saved value; explicit clear removes it.
- [x] `solar.local` resolves on the Mac and the request-header guard returns 403 without the header. `http://solar.local/` opens from an iPhone (owner, 04-11-2026). Still to check: browser changes work without a login and are refused without the `X-CYD-Request` header.
- [x] Panel matches `docs/images/centre-flow-preview.png`: SOLAR/GRID/BATTERY/HOUSE labels readable, no overlaps, even pipe gaps (owner, 04-11-2026).
- [ ] Web UI: flow-diagram comets match real import/export and charge/supply (still, by design, with Reduce Motion on); light/dark follow the browser; Save works from every settings section.
- [ ] Night dimming follows Sydney time; touch restores brightness for a minute.
- [ ] Wi-Fi reset preserves HA settings and returns to setup after restart.
- [ ] Browser app update succeeds and preserves settings/calibration.
- [ ] Invalid/merged upload is rejected and normal operation can resume.
- [ ] ArduinoOTA works when Wi-Fi connects after boot and uses the device code.
- [ ] Sustained operation has adequate free heap, responsive touch and no watchdog resets.
- [ ] If using HTTPS, valid CA verification works and an invalid CA is rejected.

---

## Browser installer release

- [x] Create/configure the remote repository and Pages only when requested.
- [x] Build a credential-free CI release on a `v*` tag on `main` (v0.6.0, v0.7.0).
- [x] Every build fails if `firmware.bin` leaves less than 64 KiB in the OTA slot (`tools/flash_budget.py`).
- [ ] Fresh browser installation provisions Wi-Fi and reports the expected project identity.
- [ ] Reconnecting recognises the same firmware and offers an update preserving NVS.

---

## 24-hour history

The design and desktop checks are described in [HISTORY.md](HISTORY.md). A full day of real data and retention across a USB app flash are in the evidence log; the checks below are still open.

- [ ] First boot initializes only a completely blank data partition; nonblank mount failure leaves a RAM-only warning and retains flash contents.
- [ ] Collect more than 24 hours of actual power, SOC, configured totals and weather; compare timestamps, units and signs on the CYD and `/api/history?hours=24`.
- [ ] Reboot with NTP temporarily unavailable: restored samples stay hidden until clock sync, then show at their original times.
- [ ] Restart through WebUI, Wi-Fi reset, Improv and both OTA paths; latest requested checkpoint is restored with configuration and calibration intact.
- [ ] Cut power during alternating checkpoint writes repeatedly; recover the latest complete bank and show gaps for lost/unpowered intervals.
- [ ] Disconnect Wi-Fi, stall upstream reporting and make individual entities unavailable; no stale values are stored as fresh, no gaps become zero and no line crosses a missing slot.
- [ ] Confirm demo mode never enters actual history; mapping/location changes clear history while token, labels, timezone and layout changes retain it.
- [ ] Test clock corrections, midnight and daylight-saving changes; preserve UTC ordering, signed values and a rolling day.
- [ ] Verify History/Home/metric tabs/Prev/Next hit targets in both themes at 320 × 240. Hold >2 seconds opens Settings once without a release tap; a separate hold there calibrates; night wake consumes the contact.
- [ ] Measure free heap, longest checkpoint pause and HTTP response time with TLS/weather, animated flows, full history and multiple browser clients; check watchdog/Wi-Fi responsiveness and OTA from either app slot.

---

## Serial debug settings

- [ ] At default Info, verify routine polling/health reports occur about once per minute; connection/state changes and occasional total/weather events remain visible.
- [ ] Save each debug level in Settings → Display; confirm Off suppresses application logs and Verbose restores every-poll detail without disrupting Improv or live data/history.
- [ ] Restart after selecting Off and after selecting Verbose; verify the saved level is restored. Check USB bootloader/library output separately from application logging.

---

## Shared serial/CYD startup diagnostics

- [ ] Capture a cold boot at 115200 baud with Info selected: verify service stages precede TFT initialization, the seven serial/TFT status rows agree, and HA readiness reflects actual readings rather than configuration alone.
- [ ] Check restored, expired, new-source and corrupt/unavailable history cases; confirm clock-pending counts change appropriately after NTP sync.
- [ ] With Wi-Fi/HA/NTP unavailable, confirm one final pending-services summary after 60 seconds, no per-frame boot log repetition and normal reconnection/Improv operation afterward. Skipping the panel boot screen must not cancel serial checks.
- [ ] Test saved Off, Warnings and Info levels, sprite allocation failure and both panel themes. Confirm no token/password/device code appears in boot logs; bootloader/library messages are separate.

---

## Scheduled dim/off/on

- [ ] Upgrade an existing dim-only hour schedule; verify unchanged behaviour, saved minute-precision times and all three night actions across restart.
- [ ] Verify dim, off and on transitions at minute boundaries across midnight and timezone/DST changes; missing clock must keep the display awake.
- [ ] While the backlight is zero, confirm polling, 24-hour history, WebUI, serial and both OTA paths continue. Morning on and a touch must restore a complete frame.
- [ ] A touch or hold on a dimmed/off screen must only wake it for one minute; a separate contact must operate History, Settings or calibration. Check the one-minute awake grace after restart.
