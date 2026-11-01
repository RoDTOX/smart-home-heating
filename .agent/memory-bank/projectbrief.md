# Project Brief

Rewritten 01.11.2026 (process review QA-25). Mission and rules: [`AGENTS.md`](../../AGENTS.md) §1–§3; scope:
requirements §1 ([`docs/requirements_and_features.md`](../../docs/requirements_and_features.md)).

## What

Firmware for autonomous multi-zone underfloor heating on an ESP32 relay board: BLE room sensors and outdoor weather
in; 230 V thermal actuators, an optional circulation pump and a gas boiler ON/OFF link out.
**Install once, calibrate once, forget it.**

## Why

Commercial multi-zone controls (Tado, Netatmo, Danfoss Ally, Honeywell evohome) are expensive for 5–6 zones, and the
original wireless room thermostat of the reference home broke. The controller replays the boiler's radio command, uses
low-cost Xiaomi BLE sensors with open firmware and drives the manifold actuators through relays.

## Scope

**In scope** (requirements baseline v2): room control per zone (TPI, schedule, pre-heat), hydraulic safety
(interlock, overrun, burner guard, frost protection, zone without sensor), boiler link, sensors and plausibility,
diagnostics and event history, local API, console and one page, commissioning, phase-4 predictive control.

**Out of scope** (requirements §1, AGENTS.md §2): cloud services, SaaS, remote fleet, sales, Home Assistant,
kiosk / wall tablet, floor cooling, push notifications; changes inside the boiler.

## Success criteria

1. Heating continues when Wi-Fi, router or internet fail (NFR-1.1).
2. Every occupied room stays within the comfort band of NFR-4.1 in steady state.
3. The boiler never burns with all zone valves closed (FR-2, FR-3).
4. No parameter tweaking after commissioning (FR-68); faults are reported, not felt (NFR-5.1).
