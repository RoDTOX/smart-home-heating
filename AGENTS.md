# AGENTS.md

> Agent instructions for the `smart-heating` repository. Format: <https://agents.md>.

## 1. Mission

Firmware for autonomous multi-zone underfloor heating — **install once, calibrate once, forget it**.

- **Hardware:** ESP32-WROOM-32E on ESP32-Relay X4 V1.1 (`192.168.1.20`).
- **Inputs:** Xiaomi BLE room sensors (NimBLE passive scan), Open-Meteo outdoor temperature.
- **Outputs:** 230V NC thermal actuators (relays), optional GPA pump (relay 4, unused in reference install), gas boiler ON/OFF via 433.92 MHz OOK.
- **Interface:** headless REST/JSON API + CLI ([`tools/esp_cli.py`](tools/esp_cli.py)). UI is secondary.

Reference install: Immergas Victrix Tera 24, 7 circuits → 3 zones (Z1 living+kitchen, Z2 kids bedroom, Z3 master bedroom).
Zones, circuits and boiler interface must stay **configuration**, not hard-coded, so other installs work too.

## 2. Current focus: make it lean

The repo grew in every direction. Work happens in phases on a branch:

1. ✅ Remove dead docs/tools/integrations.
2. Firmware lean — drop features that do nothing, plain naming.
3. One simple UI page.
4. Predictive control (weather compensation, pre-heat, learned per-zone model).

**Do not add features outside the current phase.** No cloud, SaaS, fleet, sales, Home Assistant or kiosk work.

## 3. Engineering rules

- **Smallest change that works.** Reuse existing modules before writing new ones.
- **No new libraries** without user confirmation. Vendored: `ArduinoJson`, `NimBLE-Arduino`.
- **Never cut safety corners:**
  - actuator opening interlock (120–180 s) before boiler ON
  - pump overrun (180 s)
  - anti-short-cycling burner guard
  - frost protection (<12 °C)
  - sensor timeout isolation (`DTC-101`) and safe state when BLE / Wi-Fi drop
- **Resources:** NVS writes only on value change, no blocking calls that starve `loop()`,
  bounded buffers, no heap growth, firmware < 1.25 MB (OTA).

## 4. Verification before flashing

1. `pio run` builds clean (also runs on GitHub Actions for every push).
2. `python tools/test_system.py` passes against the board.
3. Read your own `git diff` for debug leftovers and the safety list above.

## 5. Workflow

- Explain the *why* briefly (analogy welcome), keep tasks atomic.
- Ask when intent is unclear (>10% ambiguity) instead of guessing.
- On task completion update [`docs/changelog.md`](docs/changelog.md) and `.agent/memory-bank/activeContext.md`.
- **Commit/push only on explicit user request.** Deletions need confirmation.

## 6. Language

- Code, identifiers, API, logs, docs: **English**.
- Conversation with the user: **Romanian**, concise.
