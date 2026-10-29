# Progress

## Works (verified live on `192.168.1.20` before the lean refactor)
- Multi-zone hysteresis control (3 zones), actuator interlock, pump overrun, anti-short-cycling, frost guard, sensor timeout (DTC-101).
- Boiler ON/OFF via 433.92 MHz OOK (`BoilerRadioTrigger`) with safety heartbeat refresh.
- Native passive BLE scan of Xiaomi sensors (BTHome v2 / PVVX), NimBLE heap leak fixed (DTC-402).
- REST API, Web OTA, Telnet console, NTP + Open-Meteo, schedule with presets, open-window detection.

## Pending
- Phase 2 firmware lean, phase 3 single UI page, phase 4 predictive control (see `docs/plan.md`).

## Known issues
- Hysteresis-only control overshoots/undershoots on slow underfloor screed (motivation for phase 4).
- Web UI (`web/index.html`, ~4.9k lines) too large and not what the user wants (phase 3).
