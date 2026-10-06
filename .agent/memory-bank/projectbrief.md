# Project Brief: Smart Heating System (DIY Multi-Zone ESP32 Controller)

## What is this project?
An autonomous, low-cost, multi-zone smart heating and hydronic climate control system powered by an ESP32 microcontroller, controlling 230V NC electrothermal manifold actuators and a reverse-engineered wireless gas boiler trigger, using BLE broadcasts from hacked Xiaomi temperature sensors.

## Why does it exist?
Commercial multi-zone smart heating solutions (e.g., Tado, Netatmo, Danfoss Ally, Honeywell Evohome) are prohibitively expensive for 5–6 zones. Following the hardware failure of an existing remote wireless room thermostat, this system reverse-engineers the boiler's radio trigger signal, uses cheap ($3–$5) Xiaomi BLE thermometers with open-source firmware, and drives electrothermal radiator/underfloor valves via an ESP32 relay board.

## Scope
**In scope**:
- ESP32 firmware running an autonomous thermal state machine with hysteresis.
- Native ESP32 BLE scanning for BTHome v2 sensor packets (Xiaomi LYWSD03MMC).
- Samsung Galaxy A6 MQTT bridge fallback for remote rooms.
- Electrothermal actuator opening delay interlock (120–180s) before firing the boiler.
- Pump overrun & heat dissipation protection (180s open circuit post-flame).
- Reverse-engineered boiler radio trigger module interface.
- Embedded LittleFS web dashboard with an interactive SVG apartment floorplan and mobile PWA support.
- Rolling local 24h temperature telemetry on ESP32 + MQTT sync to PostgreSQL/Grafana on Galaxy A6.

**Out of scope**:
- Cloud/SaaS subscriptions (100% local operation).
- Hardware replacement of the boiler's internal gas electronics.

## Success criteria
1. 100% autonomous operation: system continues heating safely even if Wi-Fi, internet, or phone drops.
2. Individual room temperature control within ±0.3°C of setpoint.
3. Zero boiler pump strain: flame never ignites while all manifold valves are closed.
4. Rich, responsive web UI accessible from any phone/PC on the local network.
