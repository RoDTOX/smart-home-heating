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
- [ ] **2. Firmware lean** — inventory every feature/endpoint, drop what does nothing, plain naming
      (e.g. "bio-schedule/anchors" → "schedule"), split `main.cpp`. Build + tests must pass.
- [ ] **3. UI** — one simple page: status per zone, comfort targets, schedule, alarms.
- [ ] **4. Predictive control** — weather-compensated demand, pre-heat before schedule changes,
      valve PWM instead of pure hysteresis, per-zone learned model (gain, floor lag, heat loss)
      seeded from installer hints, manual override.

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
