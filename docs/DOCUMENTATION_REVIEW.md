# Documentation review – 5 November 2026

Historical record of the initial documentation-only scan before the later history, diagnostics and night-mode implementation. Counts, screenshots and build sizes below describe that earlier snapshot. Current source-release validation is in [RELEASE_0.9.0.md](RELEASE_0.9.0.md).

Reviewed repository `CYD-Home-Solar-Monitor`, branch `dev`, base commit `dc1b676` (`0.9.0-dev`). The initial working directory was an empty chat workspace. The app project inventory, recent CYD project context, Git history and actual firmware/WebUI contents established the repository at `/Users/anthonyjclarke/PlatformIO/Projects/CYD-Home-Solar-Monitor`. The working tree was clean when review began.

---

## Coverage

Inventoried all 89 tracked files. Reviewed application C++ source/headers, board and PlatformIO configuration, partition map, workflow, ignore rules, the host test, all development/build tools, vendored Improv source/metadata/licenses, Markdown guides, handbook HTML, dashboard HTML/CSS/JavaScript and project structure. Image/artwork files were inventoried and checked through references; generated panel previews were regenerated. Existing dashboard screenshots remain illustrative historical assets.

Ignored local/editor metadata and build directories were inventoried for tooling context, not included in the patch. Installed third-party dependency internals and the external reusable CI workflow were not independently audited. Historical external-product claims/URLs were not revalidated online. This was a local documentation review, not a penetration test or new device commissioning.

---

## Documentation findings and corrections

The README and handbook labelled themselves `0.8.0` while source had moved to `0.9.0-dev`. They now distinguish development source from the latest release recorded in the changelog, and identify screenshots/boot preview labels as samples.

The roadmap still presented resolved RGB332, connection reuse, config-copy, wake-touch and repaint issues as current bugs. It now describes their implemented fixes, actual feature coverage and remaining work without speculative release targets. The original product brief/build plan are clearly historical, and the plan's nonexistent renderer path and obsolete deferred-feature claims are corrected.

The previous Claude security rule incorrectly said the CA certificate was never returned by the API. The implementation intentionally returns editable public `ca`; only the HA token is write-only. Updated guidance explains HTTP token submission, NVS storage without project-configured encryption, unauthenticated LAN mutations, the request header's limited role, and optional Open-Meteo/NTP traffic. It no longer claims the browser communicates exclusively with the panel.

Build guidance previously led with upload and omitted several test limits. It now starts with host checks and a build, documents 15 preview SVGs and optional PNG conversion, generated assets, app-slot limits, script syntax checks and synthetic fixture limitations. API routes, settings validation, migration, source timing and snapshot contracts now have a maintainer reference.

The handbook claimed URLs could not have a path, although validation allows a base path. It now follows the reader's suffix behavior. Polling is described as a wait after a completed cycle, weather failures are distinguished from its age ceiling, and chart history is identified as browser-tab memory using the browser timezone. A merged USB write is described as overwriting NVS, rather than guaranteeing a full-chip erase.

Hardware and installer observations remain separate from today's desktop checks. Existing checked observations are preserved; no unchecked physical case is marked complete. The shared agent guide replaces duplicated Claude rules and provides architecture, conventions, testing, security/privacy and contribution/release guidance.

---

## Follow-up code findings

These are static inspection findings, not firmware changes or claims of newly reproduced device failures. Details are retained in [REVIEW_AND_ROADMAP.md](REVIEW_AND_ROADMAP.md).

- The vendored Improv parser accepts advertised lengths without consistently bounding the 128-byte receive buffer or command payload offsets. Harden malformed USB-serial input and add regression coverage before provisioning changes.
- Some optional dashboard tile branches have no reset fallback when configuration/data disappears; previously displayed values can remain until reload. Status-fetch failure greys readings but does not explicitly stop comets or insert a chart gap.
- The centre renderer can replace an unknown-source-age status with its normal live/spare-power message; API/serial flags retain the distinction. The handbook now states this limitation.
- Unknown-length HTTP responses are fully buffered before the post-read size rejection, so configured response limits are not strict memory allocation ceilings.
- With a configured token and no successful source reads at startup, the initial weather fetch can wait behind the `haPublished` gate.

The unauthenticated HTTP interface is an existing trusted-LAN design decision, not newly added access control. Flash headroom and the outstanding installer/long-run checks remain practical release constraints.

---

## Validation performed

Validation used a copy of the tracked repository with the final documentation changes and the installed pinned PlatformIO/dependency/toolchain cache. Build/test outputs stayed in the review workspace. No real credentials, serial device, live HA/inverter, remote publish or Git commit were used.

`python3 tools/check_host.py` passed all existing model assertions with C++17 warnings as errors. It compiled the shared renderer and regenerated all 15 SVG previews and five PNGs; generated images matched the tracked files byte for byte.

`pio run -e cyd28` passed with the updated handbook embedded. Reported static RAM was 55,568 / 327,680 bytes (17.0%); app flash was 1,693,901 / 1,835,008 bytes (92.3%), leaving 141,107 bytes. `firmware.bin`, `firmware-merged.bin` and `flash_parts.json` were generated. The expected TFT_eSPI warning about undefined `TOUCH_CS` remains deliberate for the dedicated touch bus.

The dashboard's one inline script passed `node --check`; the handbook contains no scripts. All four Python tool sources compiled. Local Markdown/HTML links, image targets and HTML anchors were checked, and edited Markdown tables/indentation were normalized to the supplied formatting rules.

The loopback preview fixture passed endpoint checks for HTML, seven readings including derived solar, synthetic configuration/save, missing-header refusal, footer asset delivery and the documented missing `/guide/` route. Its missing-header status is 400, whereas the real firmware guard returns 403; the fixture is not an exact device server. The temporary server was stopped after testing.

No new visual browser interaction, real-board HTTP/NVS/TLS/OTA, serial fuzzing, provisioning, installer or endurance test was performed. The existing physical checklist and three installer cases remain open where unchecked.

---

## Files changed

New guides:

- `AGENTS.md`
- `docs/DEVELOPMENT.md`
- `docs/DOCUMENTATION_REVIEW.md`

Updated documentation:

- `README.md`
- `CLAUDE.md`
- `CHANGELOG.md`
- `docs/index.html`
- `docs/BUILD_PLAN.md`
- `docs/RAYHUB_REVIEW_AND_BUILD_BRIEF.md`
- `docs/REVIEW_AND_ROADMAP.md`
- `docs/HARDWARE_VALIDATION.md`
- `docs/WEB_INSTALLER.md`

Application source, dependencies, workflow, partition map, tests and firmware version are unchanged. The handbook text is compiled into firmware, so the rebuilt app bytes change even though application logic does not. Existing assets need no cache-version bump because their bytes are unchanged.
