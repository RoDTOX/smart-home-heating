# Progress: Smart Heating System

## Status
Architecture defined; project plan, changelog, and persistent memory bank initialized and committed. Ready for hardware pinout configuration and firmware implementation.

## Works
- Project repository and git tracking initialized.
- High-level multi-zone architecture and failsafe hydronic timing rules established.
- Samsung Galaxy A6 BLE/MQTT gateway operational in adjacent repository (`smart-home-ble-gateway`).
- Open-source BTHome v2 firmware active on Xiaomi LYWSD03MMC sensors.

## Pending
- Hardware selection for ESP32 8-relay board.
- Definition of boiler radio trigger electrical interface.
- Firmware scaffolding (`platformio.ini`, `src/`, `include/`).
- BTHome v2 NimBLE decoder implementation.
- Multi-zone thermal hysteresis state machine.
- LittleFS responsive web app with interactive apartment floorplan SVG.
- MQTT sync to A6 Grafana dashboard.

## Known Issues
- None currently identified.
