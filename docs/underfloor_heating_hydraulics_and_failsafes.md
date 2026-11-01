# Underfloor Heating — Hydraulics, Components and Failsafes

> **For:** installers, service technicians and owners.
> **Hardware:** ESP32-Relay X4 V1.1 (reference install `192.168.1.20`).
> **Version:** 3.0, 01.11.2026 — English rewrite against requirements baseline v2
> ([`requirements_and_features.md`](requirements_and_features.md)) and [`architecture.md`](architecture.md)
> (process review QA-28). Replaces the Romanian version 2.0 of 09.10.2026.
>
> Controller behaviour is defined in the requirements; parameters are cited by name and their defaults and ranges are
> in requirements §6. This document explains the hydraulics and the installation.

---

## 1. Installations differ

Underfloor installations (and mixed underfloor + radiator ones) differ a lot between a flat and a house. A small flat
may have only a wall-hung boiler feeding the manifold directly; houses often add a **pump and mixing group (GPA)**,
**secondary circulation pumps**, **differential-pressure bypass valves** and **mechanical high-limit thermostats**.
The firmware supports these through configuration — actuator type, optional pump output, boiler link type, number of
zones (requirements §6) — not through code changes.

**Reference install:** no mixing valve and no manifold pump; boiler water goes straight into the screed; the bathroom
towel radiators are the always-open bypass (requirements §2).

---

## 2. Manifold anatomy and special components

Generic manifold with a mixing group (not the reference install):

```text
       FLOW from the boiler (e.g. 50-65 °C) ──┐
                                             │
                                             ▼
                         ┌─────────────────────────────────┐
                         │ Pump and mixing group (GPA)     │
                         │ - thermostatic mixing valve     │
                         │   (lowers the water to 35-40 °C)│
                         │                                 │
                         │ - secondary circulation pump    │ ◄── optional pump output
                         │   (230 V)                       │     (relay 4, GPIO26)
                         └────────────────┬────────────────┘
                                          │
                                          ▼
   MANIFOLD FLOW (water at about 38 °C) ──┼───────────────────────┐
   [ high-limit contact thermostat 55 °C ]┘                       │
   [ flow meters, one per circuit ]                               │
       │           │           │                                  │
    Zone 1      Zone 2      Zone 3                         differential
   (living)     (kids)     (master)                           bypass
       │           │           │                                  │
       ▼           ▼           ▼                                  ▼
   MANIFOLD RETURN ───────────────────────────────────────────────┘
   [ valves with 230 V thermal actuators (NC or NO) ]
   (relays 1, 2, 3: GPIO32, GPIO33, GPIO25)
                                          │
                                          ▼
       RETURN to the boiler (water cooled to 28-32 °C)
```

Without a mixing valve (reference install) the boiler's central-heating flow setpoint is set on the boiler to at most
45 °C; this is an installation precondition checked at commissioning (requirements §2, FR-68).

### 2.1 230 V thermal actuators: NC (normally closed) vs NO (normally open)

- **NC (most of the market):** without power, the internal spring keeps the valve closed. With 230 V from the relay
  the wax element heats up and opens the valve fully after a few minutes (the requirements assume 3–5 min, FR-47).
  The opening time is measured at commissioning (FR-68); the boiler is switched ON only after a valve has been
  energised for `interlockDelay` (FR-2).
- **NO (rare):** without power the valve is open; with 230 V it closes.
- **Firmware:** `actuatorType` NC / NO (FR-15.1), stored in NVS and respected by every valve command; set through the
  API or the CLI (`python tools/sh_esp_cli.py actuator nc`).

### 2.2 Secondary circulation pump / mixing group (GPA)

- **Why it exists:** underfloor circuits are hundreds of metres of PEX pipe with a high hydraulic resistance. The
  internal pump of a wall-hung boiler often cannot deliver enough flow through the mixing valve, so installers add a
  dedicated pump at the manifold.
- **Problem in the field:** without a controller the pump is often left powered 24/7. This causes:
  1. wasted electricity (45–65 W continuously, about 40 kWh per month);
  2. early mechanical wear of the rotor;
  3. **pumping against closed valves:** when all room actuators are closed the pump pushes against a dead end —
     noise in the manifold and overheating of the pump motor.
- **Firmware** (optional pump, FR-14.1, `pumpOutput`, disabled by default; unused in the reference install):
  - the pump starts only after a valve has been open for `interlockDelay` (FR-14.2);
  - it runs only while at least one valve is open, or during the overrun with the overrun valve open (FR-14.5);
  - it runs during the overrun (FR-14.3).
- **Overrun valve:** the last open zone valve stays open while the boiler is ON or its OFF is deferred, and for
  `overrun` after the OFF — with or without a pump (FR-3). The overrun takes the residual heat out of the boiler's
  primary heat exchanger after the flame goes off. If the burner never started (the request was cancelled during the
  interlock), there is no overrun (FR-3.2).

### 2.3 Mechanical high-limit contact thermostat (50–55 °C)

- **What it is:** a bimetal thermostat clamped on the manifold flow pipe, set to 50–55 °C.
- **Why it matters:** water above about 50 °C must never enter the floor: it cracks the screed, debonds tiles and
  damages laminate or engineered wood. If the thermostatic mixing valve sticks open, boiler water at 65–75 °C goes
  straight into the floor.
- **Recommended wiring:** its NC contact in series with the supply of the manifold pump and the boiler demand. When
  the flow exceeds the setting the contact opens mechanically and stops the pump, whatever the electronics do.
- **Controller side:** the optional flow / return probes add a software flow limit (FR-54, `flowSensors`, `flowMax`).
  The mechanical thermostat works without any electronics and does not depend on the controller.

### 2.4 Differential-pressure bypass valve

- A spring-loaded valve (0.2–0.3 bar) between the flow and return headers.
- If all actuators close while a pump is still running, the pressure opens it and the water circulates through the
  end of the manifold.
- **Reference install:** the two bathroom towel radiators have no actuators and are always open; they are the
  permanent bypass. An always-open bypass is an installation precondition (requirements §2).

---

## 3. Failures and failsafes

**Controller reactions** (fallback, DTC, fault-injection name) are defined once, in the fallback matrix of
[`architecture.md`](architecture.md) §8.2, with the DTC list in requirements §5. This document does not repeat them.

**What the hardware does on its own**, independent of the controller:

| Failure | Hardware behaviour | Related requirement |
|---|---|---|
| Power loss during a burn | The boiler's gas valve closes when its own power or demand goes; NC actuators close by their spring. The residual heat goes into the water of the boiler's primary exchanger; the boiler's own 3 bar safety valve is the last barrier. | NFR-1.4 (safe boot when power returns) |
| Controller software hang | The 90 s task watchdog resets the controller; at boot all valves are driven closed and one boiler OFF is sent. | NFR-1.5, NFR-1.4 |
| Controller loses power | NC actuators lose power and close. The 433 MHz receiver keeps its last state until it times out — only a fail-safe receiver does that; the reference receiver has no timeout. A wired link uses a dry normally-open contact: no power = OFF. | FR-55 |
| All valves closed while the boiler pump runs | The always-open bypass (towel radiators or differential-pressure valve) carries the flow. | requirements §2, FR-3 |
| Mixing valve stuck open | The mechanical high-limit thermostat stops the pump and the boiler demand. | FR-54 (optional probes) |
| Boiler lockout (EN 298) | The boiler's burner control blocks ignition and ignores the thermostat contact and the radio; a tier-A (ON/OFF) controller cannot see it directly. | FR-57 (phase 4, DTC-204) |
| Actuator stuck closed | The zone stays cold; the bypass protects the pump. | FR-57 (phase 4, DTC-202) |
| Actuator stuck open, relay contact welded, wrong NC/NO setting | The zone keeps heating with its valve commanded closed. | FR-56 (phase 4, DTC-203) |

### 3.1 Demand ends during the interlock

A wax actuator needs minutes to open. If the room reaches its target, or the target is lowered, while the valve is
still opening, the valve is de-energised within one control tick and no boiler ON is sent for that zone (FR-37). The
boiler is never started for a circuit that has no flow.

### 3.2 Open window

Airing a room in winter drops the air temperature fast. A plain thermostat would open the circuit and burn gas to heat
the outside air; because the screed reacts with hours of lag, the heat pumped in during the airing overheats the room
after the window is closed. The controller therefore closes the zone's valve when the room temperature drops by at
least the global `windowThreshold` within 5 min, for 30 min at most, or until the temperature rebounds, or until it is
cleared by hand with `POST /api/zones/window/clear` (FR-40). Frost protection overrides the pause. A smarter detection
using the temperature slope, the outdoor temperature and absolute humidity is planned for phase 4 (FR-40.2).

### 3.3 Valve exercise

Valves that stay closed for a long time can seize. A valve closed for `exerciseIdleDays` is opened for 5 min with the
boiler OFF, and the optional pump runs with it (FR-19). Today's firmware still runs the old fixed weekly short exercise
until migration step 12 (`architecture.md` §17, G24).

---

## 4. Electrical wiring

The **ESP32-Relay X4 V1.1** is powered from 230 V AC (or 5 V DC over micro-USB). Its four relays have normally-open
(NO) and common (COM) contacts rated up to 10 A at 250 V AC. Mains wiring is work for a qualified electrician.

### Recommended connections

```text
                  230 V AC PHASE (from the home's distribution board)
                                     │
                                     ├───────────────────────────────────────────┐
                                     │ (fuse 2-4 A)                              │
                    ┌────────────────┴────────────────┐                          │
                    │      ESP32-Relay X4 V1.1        │                          │
                    │  AC supply terminal: L and N    │                          │
                    └────────────────┬────────────────┘                          │
                                     │                                           │
     ┌───────────────────────┬───────┴───────────────┬───────────────────────────┤
     ▼ COM relay 1           ▼ COM relay 2           ▼ COM relay 3               ▼ COM relay 4
[ relay 1 - GPIO32 ]    [ relay 2 - GPIO33 ]    [ relay 3 - GPIO25 ]        [ relay 4 - GPIO26 ]
     ▼ NO                    ▼ NO                    ▼ NO                        ▼ NO
phase to actuator(s)    phase to actuator(s)    phase to actuator(s)        phase to the manifold
zone 1 (living +        zone 2 (kids bedroom)   zone 3 (master bedroom)     pump (optional) or a
kitchen)                                                                    4th zone
     │                       │                       │                           │
     ▼                       ▼                       ▼                           ▼
  actuators               actuators               actuators                    pump
     │                       │                       │                           │
     └───────────────────────┴───────────────────────┴───────────────────────────┘
                                     │
                                     ▼
                  230 V AC NEUTRAL (common to all loads)
```

- With NC actuators an energised relay opens the valve; with NO actuators set `actuatorType` = NO (FR-15.1).
- Relay 4 is either a zone or the pump (FR-69, `pumpOutput`).
- **Boiler link:** the reference install uses the 433.92 MHz transmitter on GPIO19 to the boiler's receiver. A wired
  link uses a dry normally-open relay contact on the boiler's thermostat input, so loss of power means OFF (FR-55,
  `boilerLinkType`).

---

## 5. CLI commands for diagnosis and commissioning

For quick checks during installation without a browser:

```bash
# Full status (zones, pump, actuators, sensors)
python tools/sh_esp_cli.py status

# Show or set the actuator type (NC = normally closed, NO = normally open)
python tools/sh_esp_cli.py actuator
python tools/sh_esp_cli.py actuator nc

# Show or set the circulation pump output
python tools/sh_esp_cli.py pump
python tools/sh_esp_cli.py pump enable

# Start the valve exercise by hand (see §3.3)
python tools/sh_esp_cli.py exercise

# Self-test report and active DTC codes
python tools/sh_esp_cli.py selftest
python tools/sh_esp_cli.py diag
```

The full commissioning checklist is FR-68.
