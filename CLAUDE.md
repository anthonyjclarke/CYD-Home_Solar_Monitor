# CYD Home Solar Monitor – Claude context

Read [AGENTS.md](AGENTS.md) for the repository-wide architecture, hardware constraints, data invariants, security rules, build commands and contribution expectations. It is the shared maintainer reference for this project; apply it to Claude work too.

Current source identity is in `include/config.h` (`1.0.1`). User setup is in `README.md` and `docs/index.html`; route/storage details and validation commands are in [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). Outstanding physical-board and installer checks remain in `docs/HARDWARE_VALIDATION.md` and `docs/WEB_INSTALLER.md`.

---

## Project-specific design decisions

The browser palette follows the owner's SpendAnalysis design: paper/cream surfaces, amber controls, green/red for meaning. Panel palettes are constrained by the RGB332 sprite. Keep SOLAR/GRID/BATTERY/HOUSE labels and built-in fonts 1/2/4; the font-6 redesign was rejected for readability. The web UI intentionally has no login on the trusted LAN; preserve the mutation header guard and document its limits accurately.

The current implementation reads Home Assistant REST states, never the inverter. Do not replace its DC solar formula, heartbeat freshness, dedicated touch bus or partial 20 fps rendering with assumptions from historical planning documents or unrelated CYD projects.

Weather uses plain HTTP on purpose: an mbedTLS handshake does not fit in heap beside the 76.8 KB frame sprite and fails as a misleading X509 `-9984`. Every build must leave 64 KiB of OTA slot free (`tools/flash_budget.py`); the embedded handbook is the main cost. On macOS `solar.local` takes about 5 s to resolve, so browser request deadlines must stay well above that.
