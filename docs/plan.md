# Active Plan — Smart Home Heating

> Short on purpose. History lives in [`changelog.md`](changelog.md) and git log.

## Goal

**Install once, calibrate once, forget it.** Every occupied room within ±0.3 °C of target,
no manual parameter tweaking after commissioning.

## Hardware facts (reference install)

| Item | Value |
|---|---|
| Controller | ESP32-Relay X4 V1.1, ESP32-WROOM-32E, 4 MB flash, `192.168.1.20` |
| Relay 1 `GPIO32` | Z1 Living + Kitchen (circuits 1, 2, 3) |
| Relay 2 `GPIO33` | Z2 Kids bedroom (circuits 6, 7) |
| Relay 3 `GPIO25` | Z3 Master bedroom (circuits 4, 5) |
| Relay 4 `GPIO26` | Unused (no GPA pump installed) |
| `GPIO19` | 433.92 MHz OOK TX → boiler receiver (dry contact ON/OFF) |
| `GPIO23` | Status LED |
| Boiler | Immergas Victrix Tera 24 (condensing) — tier A (ON/OFF only) |
| Sensors | Xiaomi LYWSD03MMC, PVVX/BTHome v2 firmware, passive BLE scan |
| Hydraulic bypass | Bathroom towel radiators, always open |

Details: [`hardware_esp32_relay.md`](hardware_esp32_relay.md),
[`underfloor_heating_hydraulics_and_failsafes.md`](underfloor_heating_hydraulics_and_failsafes.md).

## Phases

- [x] **1. Repo cleanup** — removed sales/SaaS/fleet docs, Home Assistant, nginx/portal tooling, unused web pages.
- [x] **2. Firmware lean** — removed Service Mode / forced start, PWA, web console, DevicePassport (→ location),
      PubSubClient; bio-schedule + presets → day/night/away levels with one weekday/weekend slot list.
- [x] **3. UI** — one page (222 lines): family view + `#admin`. Setup page removed.
- [x] **4. Control (first step)** — TPI valve control with learned base duty, weather-shifted pre-heat,
      sensor calibration offsets.
- [ ] **Verify on the board** — build locally with `secrets.h`, `tools/ota_upload.py`, `tools/test_system.py`,
      watch TPI logs / room temps for a few days.
- [ ] **Next** — split `main.cpp` (~1.7k lines), persist learned duty in NVS, sensor-loss fallback duty
      instead of full close (needs a decision: frost risk vs. runaway), UI on LittleFS with version check.

## Design direction (for phases 2–4)

- Zones, circuits per zone and relay pins are **configuration**.
- Heat source has a capability tier: **A** = ON/OFF (relay/RF), **B** = OpenTherm (adds flow setpoint).
  Same control core for both.
- Condensing boiler on tier A: keep flow temperature low on the boiler, system controls *when / how long*.

## Decisions (29.10.2026)

- Phase order: lean first (2), then UI (3), then control (4) — each step testable on the board.
- UI: own page served by the ESP32 (no Home Assistant). Family view + hidden admin view.
- Alerts: shown in the page only (no push channel for now).
- Control target: TPI with ~1 h cycle, min ON time ≥ 10–15 min (wax actuator lag 3–5 min),
  weather-shifted early start, per-sensor calibration offsets, timed boost overrides (16–22.5 °C, 0.5 °C steps).
