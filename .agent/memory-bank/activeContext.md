# Active Context: Smart Home Heating System

## Current State (Updated 2026-10-09)
- **Target Hardware Deployed & Operational:**
  - Board: **ESP32-Relay X4 V1.1** featuring module **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
  - IP Address: **`192.168.1.20`** (on `SBC_WiFi`).
  - Free Heap: **>230 KB** (plenty of headroom).
  - Web Server & Web OTA: Active on port 80 (`http://192.168.1.20/` and `/update`).
- **Clean Architecture & 100% English Codebase:**
  - Zero `#ifdef ESP8266` legacy branches. Native ESP32 libraries used throughout.
  - All UI text, Serial CLI menus, REST APIs, comments, and debug logs are strictly in English.
  - Romanian used for user communication and documentation.
- **Headless API-First Transition & CLI-Driven Testing:**
  - UI visual development is frozen; priority shifted to robust, low-latency REST/JSON endpoints.
  - Comprehensive command-line utility [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py) covering `status`, `set`, `selftest`, `diag`, `ble`, `passport`, `time`.
  - Latency completely eliminated: 0ms optimistic DOM feedback on web UI, broken recursive BLE/MQTT loop, non-blocking 1s socket timeouts.
- **Relay Mapping & Actuator Control:**
  - Zone 1 (Living & Kitchen): Relay 1 on `GPIO32` (active HIGH).
  - Zone 2 (Kids Room): Relay 2 on `GPIO33` (active HIGH).
  - Zone 3 (Master Bedroom): Relay 3 on `GPIO25` (active HIGH).
  - Relay 4 (Spare / Pump): on `GPIO26`.
  - Status LED: `GPIO23` (active HIGH).
  - RF Transmitter DATA: `GPIO19` (OOK 433.92 MHz).
- **Hybrid Sensor Ingestion (Direct BLE + MQTT Fallback):**
  - Native passive BLE scanning via `NimBLE-Arduino` directly on ESP32 (BTHome v2 & PVVX/ATC).
  - MQTT client connected to Galaxy A6 Mosquitto broker (`192.168.1.28:1883`) as secondary redundant channel.
- **Onboard Diagnostics & Tesla-Style Service Mode:**
  - `DiagnosticEngine` tracking standardized DTCs (`DTC-101` through `DTC-402`).
  - 500ms onboard self-test suite (`/api/diag/selftest`) reporting percentage health score.
  - Service Mode toggle (`/api/diag/service`) for telephone remote troubleshooting.
- **Digital Device Passport & Provisioning:**
  - Unique hardware serial generated from MAC (`SH-20500D1A7214`).
  - Commissioning timestamp, client name, smart location, installer notes saved persistently to ESP32 NVS.
  - Sincronizare NTP timp (Europe/Bucharest) și vreme prin Open-Meteo REST API (`TimeAndWeatherService`).

## Next Immediate Steps
1. Cloud fleet connectivity prototype: Outbound TLS reverse tunnel (MQTT over TLS / WebSocket).
2. Hardware delivery & field install: wait for physical actuators and wire up to distributor valves.
