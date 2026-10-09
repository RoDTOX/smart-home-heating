# Progress: Smart Home Heating System

## Status
All core functional subsystems (heating hysteresis engine, boiler RF trigger, native BLE passive scanner, 2-way console, web OTA, diagnostic engine with DTCs, service mode, digital passport, NTP & weather sync, headless API-first architecture, hydronic safety controls, anti-spam burner interlocks, and automated PC system test suite) are fully implemented, compiled, uploaded, and verified live on ESP32 (`192.168.1.20`) with 100% ASPICE SWE.5 pass rate.

## Completed Work (2026-10-09)
- **Eliminare Bloatware (Galaxy-A6 & MQTT):**
  - Eliminat complet modulul legacy MQTT (`MqttTelemetryClient.cpp`, `MqttTelemetryClient.h`).
  - Dezactivat `-D MQTT_MAX_PACKET_SIZE=512` și eliminat dependența de `PubSubClient`.
  - Firmware-ul funcționează 100% autonom pe radio Bluetooth BLE nativ (`NimBLE-Arduino`).
- **Confort Acustic & Curățare Consolă:**
  - Eliminat orice spam automat de pachete BLE din consolă.
  - Datele senzorilor sunt afișate strict la cerere explicită (`STATUS`, `BLE`, `SENSORS` sau interogare REST).
- **Suită Automată de Testare Sistem ASPICE SWE.5:**
  - Implementat [`tools/test_system.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/test_system.py) și integrat în CLI (`python tools/esp_cli.py test`).
  - 12 cazuri de testare end-to-end acoperind toate cerințele din PRD (`FR-1`, `FR-4`, `FR-10`, `FR-11`, `FR-13`, `FR-14`, `FR-15`, `FR-17`, `NFR-1.1`).
  - Validat live pe hardware cu rată de succes de **100.0% (12/12 PASS)**.
  - Generat raport oficial de testare în [`docs/system_test_report.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/system_test_report.md).
- **Headless API-First Architecture & UI Latency Elimination:**
  - Decoupled backend REST endpoints from front-end volatility.
  - Eliminated UI lag: converted all buttons to 0ms optimistic DOM updates and asynchronous background POSTs (removed `location.reload()`).
  - Smart location selection without GPS coordinate requirements (Geocoding search + 41 Romanian county capitals + IP/GPS auto-detection).
  - Zone index normalization (supports 1-based and 0-based indices).
  - Comprehensive CLI utility [`tools/esp_cli.py`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/tools/esp_cli.py).
- **Failsafes de Siguranță, Hardening Firmware & Suport Hidraulic (Task 15):**
  - Control pompă secundară GPA via Releu 4 (`GPIO26`) cu interlock și 180s overrun.
  - Actuatoare NC / NO configurabile persistent în NVS.
  - Protecție forțată la îngheț (<12°C).
  - Izolare avarie senzor (`DTC-101`) >30min.
  - Watchdog hardware Task Watchdog Timer (TWDT 30s).
- **Failsafe Anti-Spam & Interblocare de Siguranță Arzător Cazan (Task 16):**
  - Filtru idempotență (FR-17.1), Cooldown RF (FR-17.2), Gardă anti-short-cycling 60s (FR-17.3), răspuns HTTP 429 cu waitSec (FR-17.4), detecție abuz spam DTC-201 (FR-17.5), suprascriere `--force`.

## Next Milestones
- [x] Hardware wiring & Pinout confirmation for RF transmitter daughterboard.
- [x] ESP8266 C++ driver for boiler RF transmission (`BoilerRadioTrigger`).
- [x] Live boiler RF firing test (ON / OFF) via web UI or Serial (Confirmat de utilizator).
- [x] Multi-zone thermal hysteresis & actuator interlock engine (`ThermalZoneManager`).
- [x] Full architectural migration to native **ESP32** (ESP32-Relay X4 V1.1, DOIT ESP32-32E N4) on `192.168.1.20`.
- [x] Codebase English localization (UI, CLI, REST APIs, logs, comments).
- [x] Wireless Web OTA update verified live via Wi-Fi (`/update`).
- [x] 2-Way Virtual Serial Network Console via Wi-Fi (Telnet TCP :23 & Web `/console`).
- [x] Native ESP32 BLE Scanner for Xiaomi sensors via NimBLE.
- [x] Setup Wizard, NVS persistence, Device Passport, NTP & Weather service.
- [x] Diagnostic Engine (DTCs, Self-Test, Service Mode).
- [x] Headless API-First refactoring, UI latency elimination, and `esp_cli.py`.
- [x] Failsafes de Siguranță, Hardening Firmware & Suport Hidraulic Universal pentru Pardoseală.
- [x] Failsafe Anti-Spam & Interblocare de Siguranță Arzător Cazan (ASPICE SWE.1 - SWE.4).
- [x] Eliminare Bloatware MQTT & Dependențe Galaxy-A6 (Arhitectură 100% BLE nativă).
- [x] Suită Automată de Testare pe PC & Verificare ASPICE SWE.5 (100% Pass Rate).
- [ ] Cloud Fleet Outbound Tunnel prototype (Outbound TLS WebSocket/MQTTS).
- [ ] Field installation on physical manifold actuators when received.
