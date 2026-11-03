# Hardware Validation

Firmware `0.2.2-dev`. Build and portable model checks have passed; all unchecked items below require a real CYD and/or locally entered Home Assistant credentials. No physical board was flashed during development.

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
- [ ] A reachable HA with stale upstream entities is flagged using report time.
- [ ] Unknown source reporting time is labelled rather than assumed current.
- [ ] Source timestamp skew and real upstream reporting cadence are understood.
- [ ] Home Assistant's existing Modbus monitoring continues normally.

---

## Persistence and maintenance

- [ ] Entity mapping, token, brightness, battery label, demo and timing settings persist across power cycles.
- [ ] Updating a 0.1.0 device preserves its token, URL and first five entity mappings and supplies the new DC default.
- [ ] Saving migrated settings retains all six mappings after a subsequent restart.
- [ ] Blank token preserves the saved value; explicit clear removes it.
- [ ] Browser configuration and firmware writes require the device code and request header.
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

