# Web installer

Installer page: https://anthonyjclarke.github.io/CYD-Home_Solar_Monitor/

It is built by `.github/workflows/firmware.yml`, which calls the shared workflow in `anthonyjclarke/cyd-web-installer`. The board env is `cyd28`, labelled "CYD 2.8″ – ESP32-2432S028R". The manifest `name` is `CYD-Home-Solar-Monitor` (`PROJECT_NAME`).

---

## Status

Releases: `v0.6.0` (first), `v0.7.0` and `v0.8.0`, all 04-11-2026. CI builds are green and the partitions are standard dual-OTA, unchanged between them.

The firmware runs on the owner's panel, flashed over USB from PlatformIO, not from the installer. The installer smoke test (RUNBOOK 5a) has not been run yet: a fresh install from the live page, Improv **Configure Wi-Fi**, then **Connect** again showing "Connected to CYD-Home-Solar-Monitor 0.8.0". `v0.7.0` and `v0.8.0` were released with these tests still owed, at the owner's request.

---

## Tests owed

These have not been run yet. Run them on the next real work on this project, or before the next release, and tick each one off with the date and board MAC.

- [ ] 5a – smoke test from the live page (fresh install, Improv Wi-Fi, reconnect shows name and version)
- [ ] Case 2 – Update on a provisioned board (settings kept)
- [ ] Case 3 – Update from `app1` after an ArduinoOTA or browser update
