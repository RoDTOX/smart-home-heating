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
| 7 | `ZoneController`: TPI, learned duty per outdoor band, FR-51 fault duty, window state; `FaultInjector` | accepted: owner visual check 05.11.2026, tests 100% PASS | 211a7ee | owner OK 05.11.2026 |
| 8 | `ModeManager` + `Schedule`: MANUAL timeout, automatic season, global OFF, boost limits, last level without time | built, flashed, tests PASS; owner test OK 04.11.2026 (accepted) | bcedf8a | — |
| 9 | `Commands` + authentication: one command path, FR-58 (also `/api/config/import`, FR-65.3), FR-59, CORS, English messages | built, flashed, tests PASS; owner test pending | in step 9 | — |
| 10 | `Weather` and `ControlTask` (AR-21) | accepted: Weather worker 04.11.2026, ControlTask 05.11.2026 (automated tests sufficient per owner) | 431a64f | owner OK 05.11.2026 |
| 11 | `History` in LittleFS, `StatusModel` static buffer | done: rings moved from RAM to LittleFS (5.28 KB RAM saved), StatusModel static buffer implemented (fragmentation eliminated, largestBlock 86 KB); tests SWE.5-TC37 100% PASS | 81ada91 | owner OK 05.11.2026 |
| 12 | `Maintenance`: OTA quiet gate and rollback, full self-test, valve exercise | done: QuietGate (FR-60, NFR-1.8), automatic rollback verification (FR-60), valve exercise weekly (FR-19), self-test diagnostics (/api/maint/selftest); tests SWE.5-TC45 100% PASS | in step 12 | owner OK 05.11.2026 |
| 13 | `FlowInput` + `FlowLimit` | deferred (owner 05.11.2026: without physical probes; documentation for hardware selection provided; boiler setpoint <= 45 C is protection) | — | owner deferred 05.11.2026 |
| 14 | Phase 4, one advisor per step: `PlantMonitor` (FR-56, FR-57), `HeatUpModel` (FR-67), `SmartWindow` (FR-40.2), `CycleAligner` (FR-63), `SolarThrottle` (FR-70), `HumidityComp` (FR-71) | done: all 6 advisors implemented and verified on board (`PlantMonitor` TC46 PASS, `HeatUpModel` TC47 PASS, `SolarThrottle` TC48 PASS, `CycleAligner` TC49 PASS, `HumidityComp` TC50 PASS, `SmartWindow` TC51 PASS) | cf8b1cd | owner OK 07.11.2026 |

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
| R-2 | Flash margin: the board has the old 1.25 MiB OTA slot (C-34): limit 1 306 624 B; earlier `1 299 649 B` notes mixed ELF Flash with `.bin` (`ba61543` `.bin` was `1 305 968 B`, 656 B margin). Schedule JSON dedup (`37ee751`) + Step I-4 (`SafetySupervisor`): `1 302 832 B` (`esp32dev`, margin `3 792 B`) / `1 302 848 B` (`esp32release`) on 12.11.2026. Working rule: keep `>= 1 024 B` margin per commit (`>= 512 B` floor after `Schedule`). A bigger slot needs a USB reflash (owner, later) | the image stops fitting the OTA slot (NFR-2.8) | keep ≥ 40 KB margin (AR-18); a change that goes below needs a removal first; CI fails above the limit | open |
| R-3 | Boiler-link receiver hardware open: the reference receiver has no timeout; its ESP32-class replacement and the link type (BLE / Wi-Fi / 433 MHz) are not chosen (D19) | if the controller dies while the boiler is ON, the boiler keeps burning into the towel radiators (FR-55 not met) | step 5 builds the controller side; DTC-207 stays active while the heartbeat is off | open — owner decision |
| R-4 | Flow-probe library approval: the FR-54 sensor type is open; DS18B20 would need `OneWire` and `DallasTemperature`, new libraries (AGENTS.md §3, D30) | step 13 cannot start | until the owner decides, the boiler flow setpoint ≤ 45 °C is the protection (requirements §2) | open — owner decision |
| R-5 | Recorded window traces needed for FR-40.2: `windowK`, `windowAhDrop` and the acceptance criteria need 2–3 weeks of traces with hand-marked window openings (D33) | `SmartWindow` (step 14) cannot be tuned or accepted; FR-40 stays the rule | log traces to the PC with a `tools/sh_*` script (not written yet) during the heating season, before step 14 | open |

## Improvement plan — testable core first (10.11.2026)

Owner decision D-62 (10.11.2026). Runs **before** Phase 5, on the board of today, and adds no feature: it makes the
code match [`architecture.md`](architecture.md) (P1, P5, P9, §3.2) and makes every later change cheap to verify.
Written so that a fast, low-cost agent (Antigravity, Gemini Flash) can execute it step by step. New hardware
(bigger board, boiler receiver with timeout, OpenTherm) stays in Phase 5.2 and is not part of this plan.

### Why (measured on `501d9ac`, 10.11.2026)

| Finding | Measured | Target of this plan |
|---|---|---|
| No test runs on the PC: every test needs the board, many need a real burn | 0 host tests, no `test/` directory | host suite runs in < 10 s with `pio test -e native`, also in CI |
| Logic reads the clock and Arduino itself, against P9 | `millis()` in 5 advisors; every header in `include/` includes `Arduino.h` | the pure modules (I-1, I-2, I-4) compile without `Arduino.h` |
| One file still holds zones, schedule, modes, season, frost and the safety arbitration | `src/ThermalZoneManager.cpp` 2054 lines; `SafetySupervisor` exists only "in place" inside it | `Schedule`, `ModeManager`, `SafetySupervisor` are modules of their own (§3.2); `ThermalZoneManager.cpp` < 700 lines |
| Tests that cannot fail on what they promise | 27 hollow tests ([`audit_traceability_step14.md`](audit_traceability_step14.md) §3) | 0 hollow WARN in `sh_process_check.py trace` |
| `String` in the control path, against R-A7 | `getScheduleJson`, `getSlotsString` in `ThermalZoneManager`; `PlantMonitor`; free heap dips to 91 KB (risk R-1) | no `String` in L2–L4; free heap ≥ 100 KB during the weather fetch |
| The page is one source file | `web/index.html` 3170 lines, `renderHistoryChart` about 750 lines | sources of ≤ 400 lines each, built into the same single `index.html.gz` |

### Rules for the agent that executes a step

1. **One step at a time, in the order below.** Before a step: a micro-plan of 3–5 lines and the owner's approval
   (AGENTS.md §5). After a step: stop and report; the owner tests on the board where the step says so.
2. **No behaviour change** unless the step says so. A step that moves code moves it unchanged; a fix found on the way
   becomes a `PB-n` row in [`problems.md`](problems.md), not part of the move.
3. **Tests follow the requirements.** A test asserts the value written in
   [`requirements_and_features.md`](requirements_and_features.md) §6, never a number copied from the code. A test or
   requirement that looks wrong goes to the owner; a test is never changed to make it pass.
4. **Safety list (AGENTS.md §3) is not for the fast model.** Steps marked **strong model** are done by the strongest
   model with `sh-safety-reviewer`. In any other step: if the commit hook asks for a `Safety-Reviewed:` line, or the
   diff changes a condition or a timer of interlock, overrun, burner guard, frost, sensor loss or safe boot — stop and
   hand the step over. Do not add the line yourself and do not use `SH_HOOK_SKIP`.
5. **No new library, no new framework, no `npm`.** Unity comes with PlatformIO (architecture P9). Anything else needs
   the owner (AGENTS.md §3).
6. Gate of every step: `pio run` clean, `pio test -e native` green (from I-1 on), `python tools/sh_process_check.py`
   without ERROR, firmware size within NFR-2.8. Commit on the work branch; no push without the owner's request.

### Steps

| Step | Change | Files | Done when | Who |
|---|---|---|---|---|
| I-1 | **Host test bench.** Add `[env:native]` (platform `native`, Unity) and `test/`. Make the TPI part of `ZoneController` compile on the PC: no `Arduino.h` in its header, `ParamStore` access only in `begin()` behind a small interface. First tests: outdoor band limits and the duty rules of FR-47, values from requirements §6. Add `pio test -e native` to `.github/workflows/build.yml`. Needs a host C++ compiler on the owner's PC (say so if it is missing). **DONE (11.11.2026):** `[env:native]` + `test_zone_controller` (8/8 PASS), `esp32dev` +180 B (`1299437 B`), `TC43` 4/4 PASS on board, `sh_process_check` 0 errors (CI run pending push). | `platformio.ini`, `test/test_zone_controller/`, `include/ZoneController.h`, `src/ZoneController.cpp`, `.github/workflows/build.yml` | `pio test -e native` passes locally and in CI; `pio run` image size unchanged within 1 KB; board tests of `t_zone_controller.py` still pass | fast model; `ZoneController` holds the FR-51 fault duty — rule 4 applies |
| I-2 | **Time is passed in.** The five advisors (`HeatUpModel`, `SolarThrottle`, `CycleAligner`, `SmartWindow`, `HumidityComp`) and `PlantMonitor` get `now` as an argument instead of calling `millis()`; no `Arduino.h` in their headers. One host test file per module, values from FR-67, FR-70, FR-63, FR-40.2, FR-71, FR-56, FR-57. **DONE (11.11.2026):** `0` `millis()` in all 6 modules, `0` `Arduino.h` in headers, `pio test -e native` 19/19 PASS across 7 suites, `esp32dev` +212 B (`1299649 B`), `sh_process_check` 0 errors, `TC46`–`TC51` 25/25 PASS on board. | the six `src/` + `include/` pairs, their callers, `test/test_<module>/` | `grep -n "millis()" ` finds nothing in the six modules; host tests pass; board tests `t_heatup_model.py` … `t_plant_monitor.py` pass | fast model |
| I-3 | **Simulated house** (test code only, never in the firmware image). A small thermal model: room air, floor screed with hours of lag, outdoor temperature, valve open → heat in; a fake clock; a loop that calls the control logic once per simulated second. Scenario helpers: cold start, steady winter day, sensor silent, open window. **DONE (11.11.2026):** `test/sim/SimulatedHouse.h` + `test/test_sim/test_sim.cpp` (4/4 PASS in 1.66 s; 7-day winter run stays within `±0.3 C` NFR-4.1 after day 1). | `test/sim/` | a simulated 7-day winter run finishes in < 5 s on the PC and keeps each room within the NFR-4.1 band after the first day | fast model |
| I-4 | **`SafetySupervisor` as its own module.** Write host safety invariant tests on the PC (FR-2, FR-1.3, FR-3, FR-3.2, FR-16.1, FR-51, FR-37; `FR-4` host test deferred as `PB-127`, `NFR-1.4` covered on board only), checked on **every** simulated second of I-3. Move `arbitrateValves`, `trackBoilerState`, `evaluateFrost`, `boilerOnPermitted` and the valve state out of `ThermalZoneManager` into `SafetySupervisor`, unchanged, pure (time and inputs passed in). **CODE + HOST TESTS DONE (12.11.2026), pending board test:** `SafetySupervisor` pure (`0` `Arduino.h`, `0` `millis()`); `test/test_safety/` 5/5 PASS; `esp32dev` `1 302 832 B` / `esp32release` `1 302 848 B`. Pending `sh_test_system.py` + owner acceptance on board. | `include/SafetySupervisor.h`, `src/SafetySupervisor.cpp`, `src/ThermalZoneManager.cpp`, `test/test_safety/` | host invariants pass on `SafetySupervisor` (FR-2, FR-1.3, FR-3, FR-3.2, FR-16.1, FR-37, FR-51; `FR-4` deferred `PB-127`, `NFR-1.4` board-only); `SafetySupervisor` pure (`0` `Arduino.h`, `0` `millis()`); full `sh_test_system.py` on the board; owner hands-on test and acceptance row | **strong model** + `sh-safety-reviewer`; tests by `sh-test-writer` |
| I-5 | **Split the rest of `ThermalZoneManager`** into the modules of architecture §3.2, one move per commit: `Schedule` (levels, slots, moments, pre-heat lead, `evaluateSchedule`), then `ModeManager` (mode, manual timeout, away, season). Host tests for each from FR-29, FR-30, FR-31, FR-39, FR-48, FR-61, FR-12.2, FR-52, FR-64. | `include/Schedule.h`, `src/Schedule.cpp`, `include/ModeManager.h`, `src/ModeManager.cpp`, `src/ThermalZoneManager.cpp` | `ThermalZoneManager.cpp` < 700 lines; host and board tests pass; owner checks schedule and modes on the page | fast model for the move (the file is on the safety list: the commit needs the strong model's safety review, rule 4) |
| I-6 | **No hollow test left.** For each of the 27 tests of the audit: assert the value the requirement states. Where requirement, test and firmware disagree, do not choose — report the three values to the owner as a `PB-n` row. An edited test line cites its `PB-n` in the commit message (hook). | `tools/sh_tests/t_*.py`, `docs/problems.md` | `python tools/sh_process_check.py trace` prints no "hollow test" WARN; `sh-test-reviewer` without FINDING | fast model; safety tests (TC07.x, TC24.1, TC36.x) by `sh-test-writer` |
| I-7 | **No `String` in the control path** (R-A7). Schedule status goes out through `StatusModel` into the static buffer; `getScheduleJson` and `getSlotsString` disappear; `PlantMonitor` texts use fixed buffers. Record free heap (steady and during the weather fetch) before and after in [`changelog.md`](changelog.md). | `src/ThermalZoneManager.cpp` or `src/Schedule.cpp`, `src/StatusModel.cpp`, `src/PlantMonitor.cpp`, `src/WebApi.cpp` | `/api/schedule` answer unchanged byte for byte for the same state (compare before / after); R-A7 WARN gone for L2–L4; heap minimum reported | fast model |
| I-8 | **The checker keeps it that way.** In `tools/sh_process_check.py arch`: R-A7 and a new rule "no `millis()` and no `Arduino.h` in the pure modules" become ERROR (the module list lives in one place in the tool). The default run fails when `test/` has no test for a pure module. **PART 1 DONE (12.11.2026), R-A7 pending I-7:** added `R-A9` (`PURE_MODULES` single list in `tools/sh_process_check.py` + `docs/architecture.md` §3.3 + `docs/process.md`): errors on `millis()`, `Arduino.h` in headers or unguarded `.cpp`, or missing `test/` suite; verified injected `millis()` triggers exit 1 FAIL and clean tree passes (0 errors). `R-A7` stays WARN until `I-7` clears L2–L4 `String`. | `tools/sh_process_check.py`, `docs/architecture.md` §3.3 (rule text), `docs/process.md` | a deliberately added `millis()` in a pure module makes the checker fail; removed again, it passes | fast model |
| I-9 | **Page from parts.** Split `web/index.html` into `web/src/` (one file per card: rooms, schedule, history chart, diagnostics, gas, setup, console; one stylesheet; one file with all user-visible texts per language, NFR-3.1). `tools/sh_build_web.py` joins them into the same single `index.html.gz`. Split `renderHistoryChart` into data preparation and drawing. No framework. **CODE + BUILD DONE (12.11.2026), pending board visual check:** split into 20 files under `web/src/`; `renderHistoryChart` split into `prepareHistoryChartData` + `drawHistoryChart`; `sh_build_web.py` (`44 101 B` gzip, `-411 B` vs `44 512 B` baseline) & `sh_dev_server.py` updated; legacy `web/index.html` removed per owner confirmation. Pending `t_webassets.py`, `t_ui_history.py`, and visual check on board. | `web/src/*`, `tools/sh_build_web.py`, `tools/sh_dev_server.py` | built page within 2 KB of today's gzip size; `t_webassets.py` and `t_ui_history.py` pass; owner looks at every card on the board | fast model |
| I-10 | **One parameter table feeds everything** (architecture P4, optional, owner decides after I-9). Generate the parameter section of [`api_reference.md`](api_reference.md) and the setup form metadata of the page from the table of `ParamStore`, so ranges and defaults are written once. | `tools/sh_gen_params.py` (new), `src/ParamStore.cpp`, `docs/api_reference.md`, `web/src/` | a changed range in the table changes the document and the form after one build, with no hand edit | fast model |

Order and reasons: I-1 … I-3 build the bench that protects everything after them; I-4 is the only step that moves
safety code and must not start before the invariants run on the old code; I-5 … I-7 shrink the big file under that
protection; I-8 stops the drift from coming back; I-9 and I-10 are independent of the firmware steps and may run in
between when the board is busy.

Relation to Phase 5.1: I-6 delivers the test half of item 1, I-7 and I-8 deliver the heap and static-analysis part of
items 2 and 3. The security part of item 2 (release profile, PB-104, compliance rows) is not covered here.

Status: every step `open`. The agent that finishes a step sets its row to `done` with the commit and the owner's
acceptance, as in the migration table above.

## Phase 5 — Commercial Product Preparation, Ecosystem & Extended Hardware

Approved by owner (08.11.2026). Strategic expansion beyond the initial 14 migration steps towards commercial deployment, modern user experience, and industrial interoperability.

### 5.1 Quality, Security & Embedded Optimization
1. **Traceability & Coverage Audit**: Bidirectional SWE.1 ↔ SWE.5 audit across all 149 requirements, identifying and closing gaps.
2. **Code & Security Review**: Static analysis, memory leak detection, heap profiling, and elimination of unauthenticated development flags (CRA / RED compliance, PB-104).
3. **Embedded Footprint Optimization**: Maximize free internal RAM (>100 KB steady), move residual static buffers and web assets to LittleFS, verify OTA headroom.
4. **Gas Telemetry UI**: Highlight calibrated consumption factor (`gasFlowMeasured`) prominently in Card 3.3.

### 5.2 Extended Hardware & Communications
1. **Dedicated Boiler Transceiver**: Fail-safe hardware transceiver with bidirectional ACK/heartbeat and autonomous timeout (resolving R-3, FR-55).
2. **OpenTherm Interface**: Hardware galvanic isolation bridge for direct flame modulation and flow temperature control (Tier B/C integration).
3. **Long-Range Sensor Technology**: Evaluation and integration of Sub-1GHz / Zigbee / LoRaWAN sensors for multi-story residential penetration.
4. **Industrial Open RF Actuation**: Support for open-protocol RF switches, smart plugs, and actuators (433/868 MHz, Tuya RF, Sonoff, Shelly) for secondary pumps/valves.

### 5.3 Modern User Interface (Lovable UI)
1. **Unified Dual Interface**: High-performance local device UI + hosted cloud/server web dashboard.
2. **Client Onboarding & Provisioning**: Guided startup wizard covering account registration, device pairing, WiFi provisioning, and initial zone calibration.

### 5.4 Ecosystem Integrations
1. **Home Assistant Integration**: Native MQTT Discovery and REST HVAC entity integration for smart home interoperability.

## Out of scope (historical)

Third-party cloud subscriptions, paid fleet monetization, kiosk hardware. Home Assistant and Cloud Onboarding brought into scope under Phase 5 (owner decision 08.11.2026).
