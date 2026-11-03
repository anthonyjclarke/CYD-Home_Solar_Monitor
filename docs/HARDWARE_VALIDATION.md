# Hardware Validation

Firmware `0.6.0`. Build and portable model checks pass, and the firmware runs on the owner's panel with live Home Assistant data. Unchecked items below still need a check on a real CYD.

---

## Build and desktop checks

- [x] PlatformIO `cyd28` build succeeds and fits the standard OTA app slot.
- [x] App image, merged image and `flash_parts.json` are generated.
- [x] Supplied AC balance and grid/battery sign conventions pass portable checks.
- [x] Solar charge, discharge, grid charging, zero/night, invalid inputs, stale inputs and skewed timestamps pass portable checks.
- [x] Power units, invalid states and percentage limits are checked.
- [x] ISO timestamp offsets, stale/future reports and `millis()` rollover are checked.
- [x] Six preview scenes render from the shared TFT drawing code.
- [x] Embedded JavaScript passes syntax checking.
- [x] Browser fixture shows sample measurements and saves settings through its mock endpoint.

The browser fixture uses synthetic data. Its success does not validate device authentication, NVS or the on-device HTTP server.

---

## Panel and provisioning

- [ ] Fresh USB flash starts with readable, correctly coloured 320 × 240 output.
- [ ] Panel colours match `docs/images/flow-preview.png` (navy cards, not green); the palette is RGB332-exact.
- [ ] Centre-flow layout matches `docs/images/centre-flow-preview.png`; chevrons move toward/away from the hub correctly for import/export and charge/supply.
- [ ] Switching **Panel layout** in the browser changes the panel immediately and persists across restart.
- [x] Boot screen shows Wi-Fi, IP, signal, HA and clock, and closes about 6.4 s after boot when Wi-Fi connects; readings are live by then (first poll at 2 s).
- [ ] Boot screen readable by eye; a tap skips it; with Wi-Fi unavailable it closes at 15 s onto the Wi-Fi setup page.
- [ ] Setup page refreshes once per second without visible flashing.
- [ ] At night, the touch that wakes a dimmed panel does not open the setup page.
- [ ] Touch works under continuous TFT redraws; calibration targets map correctly.
- [ ] Saturated/no-response readings do not generate phantom taps.
- [ ] Saved Wi-Fi reconnects, and loss/recovery leaves the device responsive.
- [ ] No saved Wi-Fi opens the password-protected setup portal after about 20 seconds.
- [ ] Portal timeout and failed-password recovery retain usable setup access.
- [ ] Improv responds to device-info queries and provisions Wi-Fi through the installer.
- [ ] Device access code persists across ordinary restarts.

---

## Home Assistant data

- [ ] Configure the real token locally and confirm the base URL/port.
- [ ] Capture serial diagnostics at `115200` baud and confirm HTTP/value/solar reasons agree with live HA data.
- [ ] Verify log volume and serial Improv responses under live polling on the panel.
- [ ] Six `[READ]` lines per cycle show low `ms` after the first (connection reused); `report_skew` is small.
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
- [x] `[WEATHER]` shows HTTP 200 after boot (fetch ≈7.6 s, no watchdog reset). Still to check: repeats about every 30 minutes; icons match conditions.
- [ ] Weather survives an HA outage and clears when the location is cleared.
- [x] Free heap healthy across a weather TLS fetch (`min_heap` 53 KB, idle heap ~114–122 KB). Still to check over hours.
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
- [x] `solar.local` resolves on the Mac and the request-header guard returns 403 without the header. Still to check: `http://solar.local/` opens from an iPhone; browser changes work without a login and are refused without the `X-CYD-Request` header.
- [ ] Panel matches `docs/images/centre-flow-preview.png`: SOLAR/GRID/BATTERY/HOUSE labels readable, no overlaps.
- [ ] Web UI: flow-diagram arrows match real import/export and charge/supply; light/dark follow the browser; Save works from every settings section.
- [ ] Night dimming follows Sydney time; touch restores brightness for a minute.
- [ ] Wi-Fi reset preserves HA settings and returns to setup after restart.
- [ ] Browser app update succeeds and preserves settings/calibration.
- [ ] Invalid/merged upload is rejected and normal operation can resume.
- [ ] ArduinoOTA works when Wi-Fi connects after boot and uses the device code.
- [ ] Sustained operation has adequate free heap, responsive touch and no watchdog resets.
- [ ] If using HTTPS, valid CA verification works and an invalid CA is rejected.

---

## Browser installer release

- [ ] Create/configure the remote repository and Pages only when requested.
- [ ] Build a credential-free CI release on a `v*` tag on `main`.
- [ ] Fresh browser installation provisions Wi-Fi and reports the expected project identity.
- [ ] Reconnecting recognises the same firmware and offers an update preserving NVS.

