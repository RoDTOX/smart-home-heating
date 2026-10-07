# Progress: Smart Home Heating System

## Status
Hardware specifications completed. Boiler RF hardware wiring and pinout confirmed with multimeter. `BoilerRadioTrigger` C++ driver implemented and flashed onto ESP8266 test bench on COM6. Device is online on local Wi-Fi with web dashboard, REST API, Web OTA, and Serial CLI ready for live boiler testing.

## Completed Work (2026-10-07)
- **RF Hardware Cabling & Pinout:**
  - Multimeter verification on test pads: Negru (GND $\rightarrow$ G), Galben (3.3V $\rightarrow$ 3V), Verde (DATA $\rightarrow$ D1/GPIO5).
  - Pin 2 confirmed internal bridge to VCC (+3.3V).
  - Main PCB kept intact to preserve matched perimeter 433.92 MHz trace antenna.
- **Microcontroller & PlatformIO Setup:**
  - Configured PlatformIO environment for ESP8266 (ESP-12F / 8266MOD, 4M1M flash).
  - Centralized hardware definitions in `include/Config.h`.
- **C++ Boiler RF Driver (`BoilerRadioTrigger`):**
  - Generated PROGMEM tables (409 steps) matching sniffed signals for ON and OFF.
  - Implemented microsecond-accurate bit-banging with watchdog feeding to prevent resets.
- **Bench Test Application (`src/main.cpp`):**
  - Interactive Serial CLI at 115200 baud.
  - Web Server with modern dark-theme dashboard on port 80.
  - REST endpoints (`/api/boiler/on`, `/api/boiler/off`, `/api/boiler/status`).
  - Web OTA firmware update support on `/update`.
  - Flashed to ESP8266 on `COM6`. Connected to Wi-Fi at `192.168.1.18`.

## Completed Work (2026-10-06)
- **Repository Setup:** Git tracking active, SSH authentication verified (`git@github.com:RoDTOX/smart-home-heating.git`).
- **Data Recovery:** Located and moved logic analyzer captures (`.sr`, `.sal`, raw text) and working Arduino C++ sketch (`control_RF_Centrala.ino`) to `input/boiler_rf_trigger/`.
- **Hydraulic Analysis:** Extracted circuit layouts from `Schema circuite incalzire pardoseala.pptx` and `Odaia.html`. Mapped 7 loops to 3 zones.
- **Hardware Procurement:** Purchased ESP 4-Relay board and 2x SYN115/SYN480R 433MHz RF sets on eMAG.
- **Documentation & Memory Bank:** Initialized and updated `docs/plan_oct2026.md`, `docs/changelog_oct2026.md`, and all `.agent/memory-bank/` files.

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
- [ ] Native ESP32 BLE Scanner for Xiaomi sensors via NimBLE (Task 10).
- [ ] LittleFS SVG web interface generation from `input/Odaia.html` (Task 8).
- [ ] Hardening & Failsafes (Watchdog, sensor timeout fallback, Wi-Fi reconnection).
