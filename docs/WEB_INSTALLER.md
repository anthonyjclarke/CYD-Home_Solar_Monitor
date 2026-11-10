# Web installer

Installer page: [CYD Home Solar Monitor](https://anthonyjclarke.github.io/CYD-Home_Solar_Monitor/).

The local `.github/workflows/firmware.yml` runs host checks and calls the external reusable workflow in `anthonyjclarke/cyd-web-installer` at `main`. That workflow owns release/Pages implementation; it is not copied into this repository. See [DEVELOPMENT.md](DEVELOPMENT.md) for local build outputs.

---

## Identity and image contract

The reference environment is `cyd28`, labelled “CYD 2.8″ – ESP32-2432S028R”. `1.0.2` also defines `cyd24`, `e32r28t-st7789`, `e32r28t-ili9341` and `cyd40`; labels/hints are in `platformio.ini`. Additional boards are build-verified; the 4.0″ layout and touch were checked on the owner's board, and the others await physical acceptance. Manifest name must match `PROJECT_NAME`, `CYD-Home-Solar-Monitor`. The GitHub repository is `CYD-Home_Solar_Monitor` (underscores). Changing either spelling casually can break links or installer update recognition.

Use separate image parts from `flash_parts.json`, including `boot_app0`, so a device previously running from `app1` boots the newly written app. Never put `firmware-merged.bin` in an update manifest. That clean USB image writes from `0x0` and overwrites NVS; browser/ArduinoOTA app updates take `firmware.bin`.

The release convention is a `v*` tag on `main` matching a suffix-free `FIRMWARE_VERSION`, for example `v1.0.2`. Pushing that tag runs the release and Pages publication. Publish credential-free CI artifacts rather than a local build. Preserve partition layout between releases.

---

## Recorded status

The repository records releases `v0.6.0`, `v0.7.0` and `v0.8.0` on 4 November 2026 with green CI and unchanged dual-OTA partitions. This historical record is not a fresh verification of hosted assets. The firmware was running on the owner's panel after a PlatformIO USB flash.

`0.9.0`, `0.9.1` and `1.0.0` were not published; the `v1.0.0` tag failed CI before its release step. `1.0.1` is the first release after `v0.8.0`. The browser install/update behavior described in the README is the intended contract and still needs the real-board cases below. Releases `v0.7.0` and `v0.8.0` proceeded with these cases outstanding at the owner's request.

---

## Tests owed

`1.0.2` was published on 10-11-2026 by owner decision; installer acceptance is still pending. Use the [post-flash update tests and release gate](TEST_PLAN_1.0.2.md) alongside the checks below.

Complete these before the next release and record date, board variant, prior app slot, firmware version and result. Keep a board MAC in private commissioning notes if needed; redact it from public reports.

- [ ] Fresh-install smoke test from the live page: install, Improv Configure Wi-Fi, reconnect, and confirm the exact project identity and selected release version.
- [ ] Update a provisioned board and confirm Wi-Fi, token, entity mappings, display settings and calibration remain.
- [ ] Update a board running from `app1` after an ArduinoOTA or browser update; confirm the new version boots and settings remain.

Run these only with authorization for the target device and any destructive erase. Link results to [HARDWARE_VALIDATION.md](HARDWARE_VALIDATION.md); compilation or synthetic fixture success is not installer evidence.
