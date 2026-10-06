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
   ├── Web Server (LittleFS PWA)          │
   ├── Relay Driver (230V NC Actuators) ◄─┘
   └── Boiler Radio Trigger Driver
         ▼
      [ Gas Boiler Flame On/Off ]
```

## Components
- **`BleSensorScanner` (ESP32):** Passive BLE beacon receiver parsing BTHome v2 frames (0x02 temperature, 0x03 humidity, 0x01 battery).
- **`MqttBridgeClient` (ESP32):** Subscribes to room fallback topics from Galaxy A6; publishes boiler and relay state for Grafana logging.
- **`ThermalZoneManager` (ESP32):** State machine evaluating temperature vs setpoint per zone; commands actuator relays and manages boiler heat demand.
- **`BoilerRadioTrigger` (ESP32):** Hardware abstraction driving the reverse-engineered wireless boiler trigger.
- **`WebServerManager` (ESP32):** Serves static assets from LittleFS (HTML/CSS/JS/SVG) and REST API `/api/status`, `/api/zones`, `/api/config`.

## Key Patterns
- **Autonomous Edge Pattern:** Control decisions are made strictly on ESP32. Loss of Wi-Fi or MQTT never stops heating.
- **Thermal Actuator Interlock Pattern:** NC wax actuators require 2–3 minutes to open. Boiler flame ignition is delayed until at least one actuator is physically open.
- **Pump Overrun & Bypass Pattern:** Prevents boiler heat exchanger damage by keeping at least one zone valve open for 180s post flame shutdown.
- **Anti-Short-Cycling Pattern:** Enforces minimum 3-minute burn time and minimum 5-minute cooldown before reigniting flame.
- **Hybrid Sensor Fallback Pattern:** Zone listens to direct BLE; if no packet received within 90s, uses MQTT payload from Galaxy A6. If no telemetry received for > 30 minutes, zone trips sensor fault failsafe.
