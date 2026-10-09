# Active Context: Smart Home Heating System

## Current State (Updated 2026-10-09)
- **Target Hardware Deployed & Operational:**
  - Board: **ESP32-Relay X4 V1.1** featuring module **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
  - IP Address: **`192.168.1.20`** (on `SBC_WiFi`).
  - Free Heap: **>117 KB** (zero fragmentation, stable).
  - Web Server & Web OTA: Active on port 80 (`http://192.168.1.20/` and `/update`).
- **Clean Architecture & 100% English Codebase (Zero Bloatware):**
  - Zero `#ifdef ESP8266` legacy branches. Native ESP32 libraries used throughout.
  - Eliminated legacy MQTT / Galaxy-A6 code completely (`MqttTelemetryClient` deleted, `PubSubClient` unlinked, `-D MQTT_MAX_PACKET_SIZE` removed).
  - Production architecture relies 100% on native passive BLE scanning via `NimBLE-Arduino` directly on ESP32 (BTHome v2 & PVVX/ATC).
  - Zero unrequested console spam: Telnet TCP:23 and Serial output remain completely quiet during loop; sensor readings are displayed strictly upon explicit request (`STATUS`, `BLE`, `SENSORS` or REST).
- **Headless API-First Transition & Automated Testing:**
  - Fast, deterministic REST/JSON endpoints (<150 ms response time).
  - Dedicated CLI tool [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).
  - Automated System Verification Test Suite ([`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py) / `python tools/esp_cli.py test`): 12 end-to-end integration test cases verifying ASPICE SWE.5 with 100.0% pass rate.
  - Live execution report: [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).
- **Relay Mapping & Actuator Control:**
  - Zone 1 (Living & Kitchen): Relay 1 on `GPIO32` (active HIGH).
  - Zone 2 (Kids Room): Relay 2 on `GPIO33` (active HIGH).
  - Zone 3 (Master Bedroom): Relay 3 on `GPIO25` (active HIGH).
  - Relay 4 (Secondary Circulation Pump GPA): on `GPIO26`.
  - Status LED: `GPIO23` (active HIGH).
  - RF Transmitter DATA: `GPIO19` (OOK 433.92 MHz).
- **Onboard Diagnostics & Service Mode:**
  - `DiagnosticEngine` tracking standardized DTCs (`DTC-101` through `DTC-402`).
  - 100ms onboard self-test suite (`/api/diag/selftest`) reporting percentage health score.
  - Service Mode toggle (`/api/diag/service`) for telephone remote troubleshooting.
- **Digital Device Passport & Provisioning:**
  - Unique hardware serial generated from MAC (`SH-14721A0D5020`).
  - Commissioning timestamp, client name, smart location, installer notes saved persistently to ESP32 NVS.
  - Sincronizare NTP timp (Europe/Bucharest) și vreme prin Open-Meteo REST API (`TimeAndWeatherService`).
- **Hydronic Safety Hardening & Circulation Pump Control:**
  - Automated secondary circulation pump controller on Relay 4 (`GPIO26`) with interlock safety and 180s pump overrun (with keeper zone).
  - Configurable actuator polarity: NC (Normally Closed) vs NO (Normally Open) via NVS (`act_nc`).
  - Emergency Frost Protection Guard (<12°C) forcing heating demand even in manual/off mode.
  - Sensor timeout failsafe (`DTC-101`) shutting down zones without signal for >30min, with instant automatic recovery upon reception.
  - Automated 20s anti-seize exercise cycle for summer maintenance against rotor calcification.
  - ESP-IDF Hardware Task Watchdog Timer (TWDT 30s) monitoring `loop()`.
- **Anti-Spam Failsafe, Rate-Limiting & DTC-201 (ASPICE V-Model SWE.1 - SWE.5):**
  - Fully implemented and hardware-verified anti-short-cycling guard (`FR-17`, `NFR-1.6`) on boiler RF trigger:
    * `FR-17.1`: Idempotency filter answering in 0 ms (`idempotent: true`), suppressing redundant RF bursts.
    * `FR-17.2`: RF transmitter cooldown (min 3s).
    * `FR-17.3`: Anti-short-cycling burner guard enforcing min 60s run time after ON and min 60s pause time after OFF.
    * `FR-17.4`: Standard HTTP 429 Too Many Requests response with remaining `waitSec` countdown.
    * `FR-17.5`: Excessive spam mitigation raising `DTC-201` when >=3 rapid commands sent within 10s.
    * `FR-17.6`: Pre-validation with zero CPU locking and protected TWDT.
    * Emergency override via `--force` flag on CLI and REST API.

## Next Immediate Steps
1. Wait for physical actuators delivery and wire up to distributor manifold valves.
2. Cloud fleet connectivity prototype: Outbound TLS reverse tunnel (MQTT over TLS / WebSocket).
3. Await explicit user instruction for git commit/push (`commit si push`).
