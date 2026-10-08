# System Patterns: Smart Heating System

## Architecture Overview

```text
[ Xiaomi LYWSD03MMC Thermometers ]
   │ (BTHome v2 BLE Advertising Beacons - 10s-20s interval)
   ├──────────────────────────────┐
   ▼                              ▼ (weak RSSI fallback)
[ ESP32 Heating Controller ]   [ Samsung Galaxy A6 ("Cinderella") ]
   ├── NimBLE-Arduino Scanner     ├── BTSnoop Tailer / btsnoop_scanner.py
   ├── ThermalZoneManager Engine  ├── Mosquitto MQTT Broker (:1883)
   │     ├── Hysteresis loop      ├── PostgreSQL DB & Grafana (:3000)
   │     ├── Actuator Delay       └── Telemetry Forwarder (WiFi/MQTT)
   │     └── Boiler Interlock             │
   ├── DiagnosticEngine (DTCs)            │
   ├── DevicePassport (NVS)               │
   ├── TimeAndWeatherService (NTP)        │
   ├── Web Server & REST API ◄────────────┘
   ├── Relay Driver (230V NC Actuators)
   └── Boiler Radio Trigger Driver
         ▼
      [ Gas Boiler Flame On/Off ]
```

## Components
- **`BleSensorScanner` (ESP32):** Passive BLE beacon receiver parsing BTHome v2 (UUID 0xFCD2) and PVVX/ATC (UUID 0x181A) frames.
- **`MqttTelemetryClient` (ESP32):** Secondary redundant telemetry channel subscribing to room topics from Galaxy A6; publishes boiler and relay state for Grafana logging.
- **`ThermalZoneManager` (ESP32):** State machine evaluating temperature vs setpoint per zone; commands actuator relays and manages boiler heat demand. Persists config to NVS.
- **`BoilerRadioTrigger` (ESP32):** Hardware abstraction driving the reverse-engineered wireless 433.92 MHz boiler trigger.
- **`DiagnosticEngine` (ESP32):** Real-time monitoring of subsystems, standard DTC management, onboard self-test routine, and remote Tesla-style Service Mode.
- **`DevicePassport` (ESP32):** Manages device serial number, first commissioning timestamp, client profile, and technician maintenance log in NVS.
- **`TimeAndWeatherService` (ESP32):** Wi-Fi NTP synchronization with Europe/Bucharest timezone and periodic exterior temperature retrieval via Open-Meteo REST API.
- **`NetworkConsole` (ESP32):** Dual Telnet (TCP 23) and Web console (`/console`) with synchronized bidirectional command handling.
- **`esp_cli.py` (Host/PC):** Command-line client for zero-overhead headless management and rapid validation.

## Key Patterns
- **Headless API-First Pattern:** Microcontroller serves as an ultra-fast, robust REST backend (<100ms response). UI logic is decoupled from core safety firmware.
- **Autonomous Edge Pattern:** Control decisions are made strictly on ESP32. Loss of Wi-Fi, MQTT, or Cloud never stops heating.
- **Thermal Actuator Interlock Pattern:** NC wax actuators require 2–3 minutes to open. Boiler flame ignition is delayed until at least one actuator is physically open.
- **Pump Overrun & Bypass Pattern:** Prevents boiler heat exchanger damage by keeping at least one zone valve open for 180s post flame shutdown.
- **Anti-Short-Cycling Pattern:** Enforces minimum 3-minute burn time and minimum 5-minute cooldown before reigniting flame.
- **Hybrid Sensor Fallback Pattern:** Zone listens to direct BLE; if no packet received within 90s, uses MQTT payload from Galaxy A6. If no telemetry received for > 30 minutes, zone trips sensor fault failsafe (`DTC-101`).
- **NVS Persistence Pattern:** All configuration, calibration, sensor mappings, and passport metadata survive power cycles and reboots via ESP32 Preferences.
