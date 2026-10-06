# Decisions Log: Smart Heating System

<!-- Append-only Architecture Decision Record (ADR) log. -->

## 2026-10-06 — ESP32 Autonomous Edge Controller vs Smartphone Script
**Context**: The user has a Samsung Galaxy A6 already running Termux/PostgreSQL/MQTT, and wanted to decide where the core heating logic and web interface should live.
**Decision**: Place the core heating state machine, relay switching, failsafes, and embedded Web UI directly on the ESP32 (in flash/LittleFS), rather than running a Python daemon on the Galaxy A6.
**Consequences**: The heating system is 100% immune to smartphone reboots, Wi-Fi drops, Android battery manager actions, or database glitches. The A6 is retained as a complementary MQTT broker, telemetry bridge for distant sensors, and long-term Grafana visualizer.
**Alternatives considered**: Pure Python daemon on Samsung Galaxy A6 controlling relays via HTTP/MQTT commands to an ESP8266/ESP32. Rejected due to single point of failure (if phone/Android OS restarts or freezes in winter, heating stops).

## 2026-10-06 — Dual BLE + MQTT Telemetry Ingestion
**Context**: Xiaomi LYWSD03MMC sensors broadcast BTHome v2 beacons. ESP32 has native BLE, but the manifold may be shielded behind thick reinforced concrete walls.
**Decision**: Implement a hybrid telemetry feed. ESP32 passively scans BLE directly; if a room sensor is not heard for > 90 seconds, ESP32 falls back to the room temperature published by the Galaxy A6 over local MQTT.
**Consequences**: Maximizes autonomy while providing guaranteed apartment-wide coverage.
**Alternatives considered**: Relying solely on ESP32 BLE (risk of blind spots) or solely on MQTT from A6 (destroys offline autonomy).

## 2026-10-06 — Electrothermal Actuator Opening Interlock
**Context**: 230V NC electrothermal actuators rely on wax expansion and require 2 to 3 minutes to physically open the valve port.
**Decision**: Enforce a software interlock delay (120–180s) between powering an actuator relay and firing the boiler flame radio trigger.
**Consequences**: Prevents boiler circulation pump cavitation and overpressure against closed valves.

## 2026-10-06 — Hardware Board Selection & Zone Allocation
**Context**: The apartment has 7 underfloor heating loops and 2 bathrooms with wall-mounted towel radiators. We needed to choose the relay hardware and assign zones.
**Decision**: 
- Selected the ESP32 4-Relay Board (Songle 10A 250VAC) with on-board ESP32-WROOM.
- Grouped the 7 manifold loops into 3 thermal zones:
  - Relay 1: Living + Kitchen + Dining + Vestibule (loops 1, 2, 3 in parallel).
  - Relay 2: Child Bedroom (loops 6, 7 in parallel).
  - Relay 3: Master Bedroom (loops 4, 5 in parallel).
  - Relay 4: Spare / Auxiliary.
- Bathrooms remain un-actuated on manual bimetal valves to provide permanent hydraulic bypass protection for the boiler pump.
- Boiler trigger will NOT consume a mechanical relay; it is driven via GPIO to an external 433.92 MHz transmitter (SYN115).

## 2026-10-06 — Boiler RF Protocol & Transceiver Hardware
**Context**: Need to trigger the boiler wirelessly without opening or modifying the boiler receiver unit.
**Decision**:
- Replay the sniffed 410-transition pre-modulated signal from `control_RF_Centrala.ino` via ESP32 GPIO.
- Purchased 2x SYN115 (TX) + SYN480R (RX) sets. SYN115 uses quartz PLL; SYN480R is superheterodyne (-107 dBm).
- Quarter-wave 17.3 cm copper wire antennas attached to ANT pins.
- Weather compensation integrates Tronic 4-LD7396-2 on 433.92 MHz with Open-Meteo REST API as Wi-Fi fallback.

## 2026-10-06 — Git Push Protocol: Explicit User Confirmation Required
**Context**: Remote git operations affect upstream repositories.
**Decision**: NEVER execute `git push` automatically or as a side-effect. Always display `git status` / pending commits and wait for explicit confirmation from the user in chat before pushing.
**Consequences**: Eliminates unintended upstream pushes; ensures user reviews all changes first.

