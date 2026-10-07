# Active Context: Smart Home Heating System

## Current State (Updated 2026-10-07)
- **Target Hardware Deployed & Operational:**
  - Board: **ESP32-Relay X4 V1.1** featuring module **DOIT WiFi MODEL ESP32-32E N4** (ESP32-D0WD-V3, 240MHz Dual-Core, 4MB Flash, MAC: `20:50:0d:1a:72:14`).
  - IP Address: **`192.168.1.20`** (on `SBC_WiFi`).
  - Free Heap: **235 KB** (huge headroom vs ~28KB on ESP8266).
  - Web Server & Web OTA: Active on port 80 (`http://192.168.1.20/` and `/update`).
- **Clean Architecture & 100% English Codebase:**
  - Zero `#ifdef ESP8266` legacy branches. Native ESP32 libraries used throughout.
  - All UI text, Serial CLI menus, REST APIs, comments, and debug logs are strictly in English.
  - Localization for Romanian will be handled via an external JSON bundle.
- **Relay Mapping & Actuator Control:**
  - Zone 1 (Living & Kitchen): Relay 1 on `GPIO32` (active HIGH).
  - Zone 2 (Kids Room): Relay 2 on `GPIO33` (active HIGH).
  - Zone 3 (Master Bedroom): Relay 3 on `GPIO25` (active HIGH).
  - Relay 4 (Spare / Pump): on `GPIO26`.
  - Status LED: `GPIO23` (active HIGH).
  - RF Transmitter DATA: `GPIO19` (OOK 433.92 MHz).
- **MQTT Telemetry Ingestion:**
  - Connected to Galaxy A6 Mosquitto broker (`192.168.1.28:1883`).
  - Receiving real-time BLE temperature/humidity data from Xiaomi LYWSD03MMC sensors:
    - Kids Room: 23.3°C, 55%
    - Living Room: 23.0°C, 55%
    - Bathroom: 22.1°C, 59%
- **2-Way Wi-Fi Network Console Deployed & Live:**
  - Telnet server on port 23 (`192.168.1.20:23`) with raw terminal support and PlatformIO monitor (`monitor_port = tcp://192.168.1.20:23`).
  - Interactive Web Console at `http://192.168.1.20/console` (and reverse proxied at `http://galaxy-a6/heating/console`):
    - Dark theme terminal with 8KB RAM ring buffer.
    - Command input bar with history navigation (Up/Down arrows) and quick action buttons (`STATUS`, `ON`, `OFF`, `AUTO`, `MANUAL`, `HELP`).
    - Bidirectional command execution: commands are parsed and executed on-chip, with synchronized output to Web, Telnet, and USB Serial.
- **Wireless Updates (Web OTA):**
  - Fully tested and working over Wi-Fi (`scratch/ota_upload.py` flashed 863KB binary in 6.43s).
  - Physical cables no longer needed for firmware development, log monitoring, or command debugging.

## Next Immediate Steps
1. Task 10: Native BLE Scanner on ESP32 (using NimBLE) to read Xiaomi beacons directly without relying on phone gateway.
2. Task 8: Interactive SVG apartment floorplan dashboard (`Odaia.html`).
3. Task 9: RF weather station reception (Tronic 433MHz) & Open-Meteo fallback.
