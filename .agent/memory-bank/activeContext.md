# Active Context: Smart Home Heating System

## Current State (Updated 2026-10-06 Evening)
- **Hardware Architecture Fully Specified & Ordered:**
  - 1x ESP32 4-Relay Board (5V/230V, Songle 10A, GPIO headers).
  - 2x 433.92 MHz RF sets (SYN115 PLL transmitter + SYN480R superheterodyne receiver).
  - 3 thermal zones defined from 7 underfloor heating circuits (Living+Kitchen: Rel 1; Child Room: Rel 2; Master Bed: Rel 3; Rel 4: Spare).
  - Bathrooms are free-flow towel radiators (permanent hydraulic bypass, zero pump cavitation risk).
- **Boiler RF Reverse-Engineered Data Recovered:**
  - All captures, pulse duration tables, and reference Arduino code saved under `input/boiler_rf_trigger/`.
  - Signal has 410 transitions, ~65 ms preamble, OOK/ASK modulation at 433.92 MHz.
- **Outdoor Climate Sensing:**
  - Primary: Tronic 4-LD7396-2 weather station (433.92 MHz OOK).
  - Fallback: Open-Meteo REST API via Wi-Fi (tested: 13.0Â°C vs 13.3Â°C).

## Next Immediate Steps (Tomorrow's Session)
1. Implement `BoilerRadioTrigger` C++ driver for ESP32 (using RMT peripheral or hardware timer for non-blocking 410-pulse transmission).
2. Create `ThermalZoneManager` engine (hysteresis, 2-3 min actuator opening delay interlock, pump overrun, anti-short-cycling).
3. Extract interactive SVG floorplan paths for LittleFS web interface from `input/Odaia.html`.
4. Implement BTHome v2 passive BLE scanner service with `NimBLE-Arduino`.
