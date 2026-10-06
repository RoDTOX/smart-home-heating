# Progress: Smart Home Heating System

## Status
Hardware specifications completed and parts ordered. Hydraulic zones mapped. Sniffed boiler RF signals and reference Arduino firmware recovered and archived in repository. Ready for ESP32 firmware implementation.

## Completed Work (2026-10-06)
- **Repository Setup:** Git tracking active, SSH authentication verified (`git@github.com:RoDTOX/smart-home-heating.git`).
- **Data Recovery:** Located and moved logic analyzer captures (`.sr`, `.sal`, raw text) and working Arduino C++ sketch (`control_RF_Centrala.ino`) to `input/boiler_rf_trigger/`.
- **Hydraulic Analysis:** Extracted circuit layouts from `Schema circuite incalzire pardoseala.pptx` and `Odaia.html`. Mapped 7 loops to 3 zones.
- **Hardware Procurement:** Purchased ESP32 4-Relay board and 2x SYN115/SYN480R 433MHz RF sets on eMAG.
- **Documentation & Memory Bank:** Initialized and updated `docs/plan_oct2026.md`, `docs/changelog_oct2026.md`, and all `.agent/memory-bank/` files.

## Next Milestones
- [ ] ESP32 RMT driver for boiler RF transmission (`BoilerRadioTrigger`).
- [ ] Multi-zone thermal hysteresis & actuator interlock engine (`ThermalZoneManager`).
- [ ] LittleFS SVG web interface generation from `input/Odaia.html`.
- [ ] BTHome v2 BLE scanner with NimBLE.
