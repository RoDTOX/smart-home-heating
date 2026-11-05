# Project Plan — Smart Home Heating

> ASPICE MAN.3 project plan. Short on purpose: what each step changes and when it is done is in
> [`architecture.md`](architecture.md) §18, decisions are in [`decisions.md`](../.agent/memory-bank/decisions.md),
> history in [`changelog.md`](changelog.md) and git log. Rewritten 01.11.2026 (process review QA-18, QA-21, QA-27).

## Goal

**Install once, calibrate once, forget it.** Every occupied room within ±0.3 °C of target (NFR-4.1),
no manual parameter tweaking after commissioning (FR-68).

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

Zones, circuits, relays and the boiler link are configuration (requirements §2, FR-69); this is one example.
Details: [`hardware_esp32_relay.md`](hardware_esp32_relay.md),
[`underfloor_heating_hydraulics_and_failsafes.md`](underfloor_heating_hydraulics_and_failsafes.md).

## Current state (01.11.2026)

| Item | State |
|---|---|
| Requirements | baseline v2 ([`requirements_and_features.md`](requirements_and_features.md), 31.10.2026), from the owner-approved review [`requirements_review.md`](requirements_review.md) |
| Architecture | [`architecture.md`](architecture.md) approved by the owner 01.11.2026 (AR-1…AR-20) |
| Process review | [`process_review.md`](process_review.md) approved by the owner 01.11.2026 (QA-1…QA-28); being applied by the documentation sessions. QA-1…QA-6 block code work. |
| Board | runs the build flashed on 30.10.2026 and reports commit `8065534`, the old hard-coded build identity (`process.md` §7), so its exact source is not identified (PB-13) |
| Board tests | no valid result for baseline v2: the run of 30.10.2026 is not evidence, because the tests did not follow the requirements and the safety tests were hollow (PB-10; process review QA-1, QA-26) |
| Known safety defects on the board | open safety rows in [`problems.md`](problems.md), among them: manual boiler ON without interlock (PB-1, G2), last valve closes while the boiler is still ON (PB-2, G3, G4), frost can start the boiler without interlock (PB-3, G5), API accepts timings below the safety floors (PB-4, G18). Fixed by migration: G2 in steps 4 and 9 (no separate hotfix, owner 01.11.2026), G3, G4 and G5 in step 4, G18 in step 1. |

History: lean phases 1–3 and a first control step (TPI, pre-heat, sensor offsets) were done on 29.10.2026
([`changelog.md`](changelog.md)). Since 01.11.2026 the plan is the migration below.

## Migration steps

Gate for every step (AR-19, owner decision 01.11.2026, [`process.md`](process.md) §5):

1. One step at a time, in this order; one step is in progress at a time. The step changes the code and its tests
   together (`process.md` §3.1), with the test review gate of `process.md` §3.2.
2. `sh-verify-firmware` passes: build, `sh_process_check.py` without ERROR, `sh-safety-reviewer` without BLOCKER
   (steps 3, 4 and 5 touch the safety list).
3. The firmware session may then flash the board itself (owner decision 01.11.2026); only one firmware session runs
   at a time. After the flash: full `tools/sh_test_system.py` and `sh_process_check.py board` without ERROR.
4. The owner tests hands-on on the board and adds a row to [`acceptance.md`](acceptance.md); a `NOK` opens `PB-n` rows.
5. Step review record `docs/review_step_<n>.md` with the owner's ✅ (`process.md` §9).
6. Only then the next step starts.

Requirement phase tags map to the steps: `[now]` = steps 1–13, `[phase 4]` = step 14, `[nice to have]` and
`[later]` = the end of step 14.

**Order (owner, 02.11.2026):** after step 1 come the configuration export / import (FR-65, change C-9, its own
change, so the owner can save the board's settings before every flash and restore them after), then step 3 with the
zones and sensors the user sets up (C-10, C-11), then step 2; the rest in the order of the table.

| Step | Module / change (details: `architecture.md` §18) | Status | Board commit | Accepted |
|---|---|---|---|---|
| 0 | Process review rows QA-1…QA-28 applied by the documentation sessions, committed by the main chat | in progress | — | — |
| 1 | `ParamStore`: one parameter table with requirements §6 defaults and ranges, all NVS access | flashed, board test 81/81 PASS; owner acceptance pending | 7743c26 | — |
| C-9 | Configuration export / import (FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8), before step 3 | done, on the board | ce8a58b | owner OK 02.11.2026 |
| 2 | `Diagnostics`: DTC enum of requirements §5, event history in LittleFS | accepted: owner OK 04.11.2026 | 3035116 | — |
| 3 | `OutputBank`: logical channels, polarity in one place, static tables for 32 zones; zones added / deleted by the user and sensor search (C-10, C-11) | accepted: zones, sensors (C-10, C-11, owner OK 02.11.2026) and `OutputBank` (pin level and NC / NO polarity in one module, R-A1, FR-15.1; owner OK 03.11.2026) | 31b8496 | owner OK 03.11.2026 |
| 4 | `SafetySupervisor`: valve states, interlock, overrun without pump, burner guard, frost on all zones, pump guard | accepted: built in place in `ThermalZoneManager` (R-A3), owner OK 04.11.2026 | 14e8197 | — |
| 5 | `BoilerLink`: `Rf433Link`, heartbeat `hbInterval`, DTC-207, `ContactLink` | accepted: built, flashed, tests PASS; hands-on verified 05.11.2026 | f28b8fc | owner OK 05.11.2026 |
| 6 | `SensorHub`: mailbox, plausibility filter, validity, one sensor timeout | built, flashed; owner test OK 04.11.2026 (accepted) | bcedf8a | — |
| 7 | `ZoneController`: TPI, learned duty per outdoor band, FR-51 fault duty, window state; `FaultInjector` | built, flashed, tests PASS; owner test pending | 211a7ee | — |
| 8 | `ModeManager` + `Schedule`: MANUAL timeout, automatic season, global OFF, boost limits, last level without time | built, flashed, tests PASS; owner test OK 04.11.2026 (accepted) | bcedf8a | — |
| 9 | `Commands` + authentication: one command path, FR-58 (also `/api/config/import`, FR-65.3), FR-59, CORS, English messages | built, flashed, tests PASS; owner test pending | in step 9 | — |
| 10 | `Weather` in the network worker | `Weather` worker built, flashed; control task of AR-21 not built (step 10 part 2); owner test OK 04.11.2026 (accepted; AR-21 part open, memory task) | bcedf8a | — |
| 11 | `History` in LittleFS, `StatusModel` static buffer | streamed history built, flashed; ring move to LittleFS and `StatusModel` buffer not built; partly accepted 04.11.2026; memory task PB-115 done in part (streamed JSON, page on LittleFS, Serial buffer), rest open | bcedf8a | — |
| 12 | `Maintenance`: OTA quiet gate and rollback, full self-test, valve exercise | open | — | — |
| 13 | `FlowInput` + `FlowLimit` — waits for the probe decision (risk R-4) | open | — | — |
| 14 | Phase 4, one advisor per step: `PlantMonitor`, `HeatUpModel`, `SmartWindow` (risk R-5), `CycleAligner`, `SolarThrottle`, `HumidityComp`; later `RemoteGateway`, factory reset (export / import moved before step 3, C-9) | open | — | — |

## Work outside the chain (03.11.2026)

Between 31b8496 and 6170f16 (43 commits) the owner worked with another AI tool (Google AGY): control law (C-14),
multi-moment schedule (C-15), gas meter (C-16), history chart (C-17), window display (C-18), import preview (C-19),
engineering view and live console (C-20). It is built and flashed (board a9c5175), but no tests-first, no safety
review and no acceptance row. These are not migration steps and do not change the step order above. Review,
records and findings: [`review_agy_03_11.md`](review_agy_03_11.md), rows C-14 ... C-20 in
[`requirements_review.md`](requirements_review.md) §9. Step 3 is accepted (owner OK 03.11.2026 for `OutputBank`; gas meter and open window accepted the same day). Step 2 starts in a new session. Owner decision 03.11.2026: the work is kept and folded into steps 7, 8 and 11 (tests first, findings PB-71 ... PB-94 fixed there). The earlier hands-on gap of 1.11.03.x is closed for step 3.

## Risks

| ID | Risk | Effect | Mitigation | Status |
|---|---|---|---|---|
| R-1 | RAM margin: internal free heap steady 106.6 KB median after the memory batch (04.11.2026, NFR-2.2), but dips to about 91 KB during the weather fetch every 15 min (6 KB task + HTTP); NFR-2.2 minimum 100 KB not reached | a step can push free heap below NFR-2.2 | measure free heap at every step (AR-18); step 11 checks ≥ 100 KB with 32 zones on the bench; savings listed in §14.2; if they are not enough, the owner decides between features and the limit | open |
| R-2 | Flash margin: the board has the old 1.25 MiB OTA slot (C-34): limit 1 306 624 B; the page left the image (FR-8.5) and JSON is streamed: image 1 261 680 B, margin 44 KB on 04.11.2026 (was over by 1.2 KB). A bigger slot needs a USB reflash (owner, later) | the image stops fitting the OTA slot (NFR-2.8) | keep ≥ 40 KB margin (AR-18); a change that goes below needs a removal first; CI fails above the limit | open |
| R-3 | Boiler-link receiver hardware open: the reference receiver has no timeout; its ESP32-class replacement and the link type (BLE / Wi-Fi / 433 MHz) are not chosen (D19) | if the controller dies while the boiler is ON, the boiler keeps burning into the towel radiators (FR-55 not met) | step 5 builds the controller side; DTC-207 stays active while the heartbeat is off | open — owner decision |
| R-4 | Flow-probe library approval: the FR-54 sensor type is open; DS18B20 would need `OneWire` and `DallasTemperature`, new libraries (AGENTS.md §3, D30) | step 13 cannot start | until the owner decides, the boiler flow setpoint ≤ 45 °C is the protection (requirements §2) | open — owner decision |
| R-5 | Recorded window traces needed for FR-40.2: `windowK`, `windowAhDrop` and the acceptance criteria need 2–3 weeks of traces with hand-marked window openings (D33) | `SmartWindow` (step 14) cannot be tuned or accepted; FR-40 stays the rule | log traces to the PC with a `tools/sh_*` script (not written yet) during the heating season, before step 14 | open |

## Out of scope

Cloud, SaaS, fleet, shop, Home Assistant, kiosk / wall tablet (AGENTS.md §2, requirements §1). Sale in the EU
under the owner's brand is in scope as conformity work only ([`compliance.md`](compliance.md), owner 02.11.2026).
