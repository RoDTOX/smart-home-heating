# Progress: Smart Home Heating System

## Status
All core functional subsystems (heating hysteresis engine, boiler RF trigger, native BLE passive scanner, MQTT fallback, 2-way console, web OTA, diagnostic engine with DTCs, Tesla-style service mode, digital passport, NTP & weather sync, headless API-first architecture, latency fixes, and `esp_cli.py` utility) are fully implemented, compiled, uploaded, and verified live on ESP32 (`192.168.1.20`).

## Completed Work (2026-10-09)
- **Headless API-First Architecture & UI Latency Elimination:**
  - Decoupled backend REST endpoints from front-end volatility.
  - Eliminated UI lag: converted all buttons to 0ms optimistic DOM updates and asynchronous background POSTs (removed `location.reload()`).
  - Resolved BLE-MQTT mutual recursion and instituted non-blocking socket timeouts.
  - Smart location selection without GPS coordinate requirements (Geocoding search + 41 Romanian county capitals + IP/GPS auto-detection).
  - Zone index normalization (supports 1-based and 0-based indices).
  - Comprehensive CLI utility [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).

## Completed Work (2026-10-08)
- **Native BLE Scanning on ESP32 (`BleSensorScanner`):**
  - Integrated `NimBLE-Arduino` into `lib/` with flash footprint reduced to 1.08 MB.
  - Decodes BTHome v2 (UUID 0xFCD2) and PVVX/ATC (UUID 0x181A).
  - Priority channel on ESP32 with seamless MQTT fallback from Galaxy A6.
- **Diagnostic Engine & Tesla-Style Service Mode (`DiagnosticEngine`):**
  - Standardized Diagnostic Trouble Codes (DTC-101 through DTC-402).
  - Onboard Self-Test (`/api/diag/selftest`) with 100% health score verification.
  - Remote service mode toggle for telephone guided troubleshooting.
- **Digital Device Passport & NTP / Weather Service:**
  - `DevicePassport`: MAC-based serial, commissioning timestamp, client metadata, NVS storage.
  - `TimeAndWeatherService`: NTP sync (Europe/Bucharest DST), season detection, Open-Meteo REST API exterior temperature ingestion.
- **Commercial Feasibility & Strategy Documentation:**
  - PRD requirements updated (`requirements_and_features.md`).
  - Remote Fleet Management & Cloud architecture (`remote_fleet_and_service_mode.md`).
  - Market analysis, ROI, unit economics & competition (`market_analysis_and_roi.md`).
  - Sales plan, side-hustle strategy & 90-minute install procedure (`plan_sales.md`).
  - Hardware BOM costs & bill of materials updated (`bom_hardware_costs.md`).

## Next Milestones
- [x] Hardware wiring & Pinout confirmation for RF transmitter daughterboard.
- [x] ESP8266 C++ driver for boiler RF transmission (`BoilerRadioTrigger`).
- [x] Live boiler RF firing test (ON / OFF) via web UI or Serial (Confirmat de utilizator).
- [x] Galaxy-A6 Reverse Proxy (`/heating/`) & Tailscale portal card integration.
- [x] MQTT telemetry consumer for Xiaomi BLE thermometers via Samsung Galaxy A6 gateway (`MqttTelemetryClient`).
- [x] Multi-zone thermal hysteresis & actuator interlock engine (`ThermalZoneManager`).
- [x] Full architectural migration to native **ESP32** (ESP32-Relay X4 V1.1, DOIT ESP32-32E N4) on `192.168.1.20`.
- [x] Codebase English localization (UI, CLI, REST APIs, logs, comments).
- [x] Wireless Web OTA update verified live via Wi-Fi (`/update`).
- [x] 2-Way Virtual Serial Network Console via Wi-Fi (Telnet TCP :23 & Web `/console`).
- [x] Native ESP32 BLE Scanner for Xiaomi sensors via NimBLE.
- [x] Setup Wizard, NVS persistence, Device Passport, NTP & Weather service.
- [x] Diagnostic Engine (DTCs, Self-Test, Service Mode).
- [x] Headless API-First refactoring, UI latency elimination, and `esp_cli.py`.
- [ ] Cloud Fleet Outbound Tunnel prototype (Outbound TLS WebSocket/MQTTS).
- [ ] Field installation on physical manifold actuators when received.
