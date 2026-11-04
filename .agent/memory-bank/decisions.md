# Decisions Log: Smart Heating System

<!-- Append-only Architecture Decision Record (ADR) log. -->

Entries of 2026-10-06 ... 2026-10-22 (the system before the lean clean-up) are in
[`docs/archive/decisions_until_2026-10-22.md`](../../docs/archive/decisions_until_2026-10-22.md); which of them still
hold: entry "2026-11-01 — Superseded decisions".

## 2026-11-01 — Process review approved; agent workflow for the clean-up and migration
**Context**: The main chat reviewed process, requirements and architecture (`docs/process_review.md`, QA-1…QA-28).
The owner wants parallel Claude sessions to implement the review, then the firmware migration with tests and reviews.
**Decision**:
- All QA rows approved with the recommended option: QA-12 an OTA image is valid when at least one mapped sensor is
  received within 60 s; QA-16 a maximum safety reaction latency of 2 s is added as a requirement (except OTA with the
  boiler OFF); QA-23 CI reports checker errors linked to an open `PB-n` as known and fails only on new errors.
- Firmware migration keeps AR-19: one step of `architecture.md` §18 at a time; the owner tests hands-on on the board
  before the next step starts.
- The firmware session may flash the board itself after `sh-verify-firmware` passes (build, process check, safety
  review without BLOCKER). Only one firmware session runs at a time.
- Documentation sessions run in parallel, each in its own files (AGENTS.md §5 zones); they do not commit; the main
  chat verifies, writes the shared log entries and commits.
**Decided by**: owner (chat, 01.11.2026).
**Affects**: `docs/process_review.md`, `docs/process.md`, `AGENTS.md`, `docs/architecture.md` §18, `.claude/skills/sh-verify-firmware/SKILL.md`.
**Consequences**: The review rows have an owner; code work starts after the documentation wave; flashing is no longer
reserved to the owner's confirmation per flash, but stays behind the verification gate.

## 2026-10-31 — Requirements review: safety values and hydraulic rules (D1, D2, D3, D6, D7, D14, D15)
**Context**: The requirements review (`docs/requirements_review.md`, SYS.2 / SWE.1, dated 31.10.2026) found several
values for one safety behaviour (interlock, burner guard, overrun, frost) and manual paths that bypass the guards.
Recorded on 2026-11-01 from the review's §3 table (process review QA-5).
**Decision**:
- D1: actuator interlock `interlockDelay` default 180 s, floor 120 s (option B).
- D2: burner minimum run / minimum pause 180 s / 180 s for every ON/OFF source (option A, the code values);
  NFR-1.6 (60 s + 60 s) retired.
- D3: overrun keeps the last zone valve open for 180 s always, also without the circulation pump (option A).
- D6: frost protection also on disabled zones.
- D7: `frostTemp` range 8–16 °C.
- D14: manual boiler ON requires an open valve unless `openLoopBypass` is set.
- D15: relay test only in MANUAL mode.
**Decided by**: owner — each row marked "✅ owner" in `requirements_review.md` §3, no date per row (review dated
31.10.2026, approval recorded in `activeContext.md` on 01.11.2026). The ranges were confirmed by the quiz of
01.11.2026 (entry "Requirements §6 ranges win over the AGENTS.md safety list" below).
**Affects**: FR-1.3, FR-2, FR-3, FR-3.2, FR-4, FR-16.1, FR-43; retired FR-38, NFR-1.3, NFR-1.6; requirements §6
`interlockDelay`, `overrun`, `burnerMinRun`, `burnerMinPause`, `frostTemp`, `openLoopBypass`.
**Consequences**: The values live only in requirements §6. Code gaps G2, G3, G5, G21 (`architecture.md` §17) are
closed by migration steps 3, 4 and 9. The D2 recommendation also said to check the boiler's own anti-cycling
parameter; no record says this was done.

## 2026-10-31 — Requirements review: sensor timeout and zone without sensor (D4, D5, D26, D35)
**Context**: One behaviour had four timeout values (control 600 s, DTC-101 after 1200 s, config API 60–3600 s,
setter 60–86400 s), and `docs/plan.md` left open whether a zone without sensor closes (frost risk) or keeps heating
(runaway risk). Recorded on 2026-11-01 from `requirements_review.md` §3 and §4.5.
**Decision**:
- D4 / D26: one `sensorTimeout` for control and DTC-101, default 20 min (1200 s), range 60–3600 s; the separate
  20 min DTC path is dropped.
- D5: first left open ("FR-51 stays TBD"), then resolved by blueprint item B-17: fixed fault duty 20 %.
- D35 (replaces the fixed 20 % of D5): a zone in sensor FAULT runs its learned duty for the current outdoor
  temperature, bounded by NFR-5.3; `faultDuty` 20 % only when nothing is learned. Frost protection, interlock and
  burner guards stay active.
**Decided by**: owner — `requirements_review.md` §3 (D4, D5) and §4.5 (D26, D35) marked "✅ owner", no date per row
(review dated 31.10.2026).
**Affects**: FR-10.2, FR-16.2, FR-18, FR-22, FR-51; retired NFR-1.2; `sensorTimeout`, `faultDuty`;
DTC-101 `ERR_SENSOR_TIMEOUT`.
**Consequences**: Sensor loss no longer closes the zone, so the AGENTS.md item "sensor timeout isolation" contradicts
FR-51 (process review QA-2). Code gaps G7, G8 are closed by migration steps 1, 6 and 7. Extended on 2026-11-01 by O-2
(global OFF, disabled zone) and O-9 (duty per outdoor band).

## 2026-10-31 — Requirements review: control, comfort and configuration (D8, D9, D10, D11, D12, D13, D16, D17, D18, D27, D28, D34)
**Context**: Rows of `requirements_review.md` §3 and §4.5 where the v1 specification, the UI, the API and the code
disagreed, or where the owner's blueprint changed the proposal. Recorded on 2026-11-01.
**Decision**:
- D8: free heap ≥ 100 KB in normal operation; DTC-402 below 60 KB.
- D9: one global `windowThreshold` (default 1.2 °C, 0.8–2.0 °C, 5 min window) for all zones; out-of-range values
  rejected; per-zone values (board: 0.3 °C on Z2 / Z3) dropped.
- D10: boost only upward, current level + 0.5…2.0 °C in 0.5 °C steps (day 23 °C → at most 25 °C).
- D28: a boost ends at the first of 3 h (`boostDuration`) and the next schedule slot.
- D11: the circulation pump stays an optional output, disabled by default.
- D12 (valve exercise ≥ interlock, pump 20 s) is superseded by D27: a valve closed for 14 days opens for 5 min,
  idle time tracked per valve.
- D13: no sensor MACs or zone names in the firmware; they are set at commissioning.
- D16: one compact console status requirement (FR-23); FR-24 merged into it, FR-25 and FR-27 retired.
- D17: code, API and logs in English; the family view language is configurable Romanian / English.
- D18: weather location is the new FR-9.2; FR-13 (device passport) retired.
- D34: heating season automatic from the outdoor daily mean (`heatLimit` 15 °C, ON after 2 days, OFF after 5),
  with a room safety net and manual override; fixed dates removed.
- B-04: factory levels day / night / away 23 / 21 / 18 °C.
**Decided by**: owner — rows marked "✅ owner" in `requirements_review.md` §3 and §4.5 (D9, D10, D13, D17, D34 with
the owner's own wording), B-04 "Owner: factory defaults"; no date per row (review dated 31.10.2026).
**Affects**: FR-1.2, FR-8.1, FR-9.2, FR-11.1, FR-12.2, FR-14.1, FR-19, FR-23, FR-29, FR-39, FR-40, FR-64, NFR-2.2,
NFR-3.1; retired FR-13, FR-14.4, FR-24, FR-25, FR-27; requirements §6.
**Consequences**: Board values outside the new ranges (window threshold 0.3 °C on Z2 / Z3) are corrected once
`ParamStore` (migration step 1) enforces §6. Code gaps G6, G12, G14, G15, G19, G24 are closed by migration steps 1,
3, 7, 8 and 12.

## 2026-10-31 — Requirements review: domain gaps G-01…G-22 (D19, D20, D21, D22, D23, D30, D31, D32, D33)
**Context**: `requirements_review.md` §4 derived what the product needs from the mission and the physics (screed fed
directly by a condensing boiler, one-way 433 MHz link, unauthenticated BLE sensors, LAN device), not from the code.
Recorded on 2026-11-01.
**Decision**:
- All gap rows G-01…G-22 approved. Changed by the owner: G-01 (optional flow / return sensors at the manifold; boiler
  flow setpoint ≤ 45 °C), G-02 (the receiver without timeout is replaced by an ESP32-class receiver), G-07 (universal
  default credentials acceptable for the first flash, with a DTC while active), G-09 (reject isolated spikes, no fixed
  rate limit, so an open window is not a fault), G-10 (full self-test with boiler start, on request). Owner inputs:
  G-11 last known level without valid time, G-12 50 events, G-20 1–32 zones.
- D19: boiler-link receiver replaced by an ESP32-class one (BLE / Wi-Fi / 433 MHz to be chosen); heartbeat switchable
  in configuration.
- D20: plausibility filter now; BTHome encryption optional later.
- D21: API authentication now, for safety-relevant writes.
- D22: alarms page-only, documented as a limitation (no push channel).
- D23: zone grouping for the boiler minimum load in phase 4.
- D30: flow / return sensors optional, enabled in configuration; sensor type open (DS18B20 would need the new
  libraries `OneWire` and `DallasTemperature`, AGENTS.md §3).
- D31: the boiler step of the self-test runs only on request; the automatic post-OTA checks without boiler decide
  the rollback.
- D32: smart window detection in phase 4; FR-40 stays meanwhile; recorded traces are needed first.
- D33: traces come from logging zone temperature, humidity and outdoor temperature every 30 s to the PC with a
  `tools/sh_*` script (no flash wear); the owner marks real window openings by hand for 2–3 weeks.
**Decided by**: owner — rows marked "✅ owner" or "✅ owner (changed)" in `requirements_review.md` §4.1–§4.4, no date
per row (review dated 31.10.2026).
**Affects**: FR-40.2, FR-52, FR-53, FR-54, FR-55, FR-56, FR-57, FR-58, FR-59, FR-60, FR-61, FR-62, FR-63, FR-64,
FR-65, FR-66, FR-67, FR-68, FR-69, NFR-4.1; requirements §2 installation preconditions.
**Consequences**: Receiver type (D19) and flow-probe type (D30) are open hardware decisions; the traces of D33 are
needed before the smart window advisor (migration step 14). These are risks in `docs/plan.md`.

## 2026-10-31 — Requirements review: owner's blueprint items B-01…B-21 (D24, D25, D29, D36)
**Context**: `requirements_review.md` §4.5 compared each item of the owner's blueprint
(`underfloor_heating_blueprint_2.pdf`, not in the repository — process review QA-15) with the requirements.
Recorded on 2026-11-01.
**Decision**:
- B-01…B-21 approved as assessed; B-06 (dew-point guard) rejected: heating only, no floor cooling.
- D24: humidity compensation (B-05) nice to have, phase 4 or later.
- D25: solar gain throttling (B-07) in phase 4, forecast-based, no new sensor.
- D29: extra BLE receivers (B-09) nice to have, later.
- D36: fault injection commands on the board (test only), otherwise the fallbacks cannot be tested.
- D26, D27, D28, D34, D35 of the same table are recorded in the entries above.
**Decided by**: owner — §4.5 rows marked "✅ owner", B-06 "reject ✅ owner"; no date per row.
**Affects**: FR-14.5 (B-12), FR-47 (B-02 acceptance example), FR-68 (B-01, B-08, B-10, B-11, B-13), FR-70, FR-71,
FR-72, NFR-5.5; requirements §1 out of scope (floor cooling).
**Consequences**: `FaultInjector` (`architecture.md` §11, AR-7) starts in migration step 7; the advisors for FR-70 and
FR-71 come in step 14.

## 2026-10-31 — Every feature degrades gracefully (graceful-degradation requirements, fallback matrix)
**Context**: The owner asked for algorithms that are smart and reliable, focused on comfort and safety
(`requirements_review.md` §4.6). Recorded on 2026-11-01.
**Decision**: Every feature has a fallback that stays inside the comfort and safety zone; the occupants do not notice
a fault, it is reported only through the log and a DTC (alarms page-only, D22). Priority safety > comfort > economy;
every requirement states inputs, fallback and report; learned values are bounded; heating interruptions shorter than
60 min are acceptable; every fallback can be triggered by fault injection.
**Decided by**: owner, 31.10.2026 (`requirements_review.md` §4.6, together with the fallback matrix).
**Affects**: NFR-5.1, NFR-5.2, NFR-5.3, NFR-5.4, NFR-5.5; the fallback clause of every functional requirement;
`architecture.md` §8.
**Consequences**: The architecture gives every input a validity `Ok` / `Held` / `Lost` (AR-4) and one fallback matrix
(`architecture.md` §8.2), each row with its fault-injection name.

## 2026-11-01 — Architecture observations on the requirements (O-1…O-11)
**Context**: Writing the architecture found eleven gaps in baseline v2 (`architecture.md` §16); the owner decided the
open ones by questionnaire and the requirements chat applied them (commit 925ea91).
**Decision**:
- O-1: the overrun valve stays open while the boiler is ON or `PENDING_OFF`, then for `overrun`; the boiler never
  burns with all zone valves closed (same as AR-11). Safety.
- O-2: in global OFF and for a disabled zone, a zone in sensor FAULT runs its fault duty while outdoor is below
  `faultFrostOutdoor` = 5 °C (the owner chose 5 °C over the proposed 0 °C) or unknown. Safety.
- O-3: the firmware modules are those of `architecture.md`.
- O-4: configuration items missing in requirements §6 are added with the reference-install defaults.
- O-5: the flow-limit OFF is forced; it does not wait for the burner minimum run.
- O-6: the 12 h season safety net only in season mode AUTO.
- O-7: NFR-1.4 says "enters the boot warmup" (zones with readings may heat during the warmup, FR-22).
- O-8: events are kept in RAM until reboot when the event store cannot be written.
- O-9: learned base duty per zone and outdoor band of 5 °C, 7 bands (same as AR-12).
- O-10: outputs of a failed expansion count as closed, DTC-208 `ERR_OUTPUT_EXPANSION_FAULT`.
- O-11: MANUAL mode and fault injection require authentication.
**Decided by**: owner, 01.11.2026 (`requirements_review.md` §6, `architecture.md` §16).
**Affects**: FR-3, FR-12.2, FR-16.1, FR-22, FR-37, FR-47, FR-51, FR-54, FR-58, FR-62, FR-64, FR-69, NFR-1.4,
NFR-3.2, NFR-5.5; requirements §5 (DTC-208) and §6.
**Consequences**: Baseline v2 and the architecture agree (requirements commit 925ea91, architecture aligned
01.11.2026 01:45). Code gap G4 (valve closes while the burner finishes its minimum run) is closed by migration step 4.

## 2026-11-01 — Architecture: structure and safety layer (AR-1…AR-10)
**Context**: `docs/architecture.md` (SWE.2) for baseline v2. Today `ThermalZoneManager` holds nine responsibilities,
`applyRelay` is called from 25 places, and manual sources reach the boiler without the interlock (§17 G1, G2).
**Decision**:
- AR-1: layered design with requests and commands; `SafetySupervisor` is the only writer of outputs.
- AR-2: the 23 modules of §3.2 replace the module list of NFR-3.2.
- AR-3: one control loop plus two background tasks (NimBLE, network worker); they share data only through two
  mailboxes.
- AR-4: every input has a validity `Ok` / `Held` / `Lost` and an age.
- AR-5: `ParamStore` is the only NVS user: one parameter table, integers in NVS, write on change, one-time key
  migration.
- AR-6: DTCs as an enum of requirements §5 with zone masks; event history 50 × 16 B in LittleFS, at most 6 records
  per code and hour.
- AR-7: fault injection only on inputs, toward the safe side, RAM only, ends with a reboot, needs authentication.
- AR-8: `OutputBank` with logical channels and backends (GPIO now, expansion later); polarity only there; static
  tables for 32 zones.
- AR-9: `BoilerLink` interface (`Rf433Link`, `ContactLink`, `ReceiverLink`); the burner guard is outside the link.
- AR-10: zone control state and valve state are separate state machines (fixes the frost and window lock-ups C1, C2).
**Decided by**: owner, 01.11.2026 (`architecture.md` §19).
**Affects**: `docs/architecture.md` §2–§12; FR-15.1, FR-16.1, FR-40, FR-42, FR-55, FR-62, FR-69, NFR-2.9, NFR-3.2,
NFR-5.2, NFR-5.5; all of `src/` and `include/`.
**Consequences**: The code gaps G1…G32 (`architecture.md` §17) are closed through the migration steps of §18 (AR-19).

## 2026-11-01 — Architecture: behaviour, resources and migration (AR-11…AR-20)
**Context**: Same document: decisions that fix behaviour left open by the requirements, the resource budget and the
order of work.
**Decision**:
- AR-11: the overrun valve stays open while the boiler is ON or `PENDING_OFF`, then for `overrun` (O-1).
- AR-12: learned TPI duty per zone and outdoor band of 5 °C, 7 bands (O-9).
- AR-13: season from Open-Meteo's previous-day mean; season mode `OFF` is the manual global OFF; safety net in AUTO
  only (O-6).
- AR-14: advisor contract of §13: optional, bounded, outside the safety path, ≤ 2 ms, ≤ 2 KB.
- AR-15: telemetry history in LittleFS files, streamed by the API; RAM keeps one record.
- AR-16: `Commands` is the only entry for user actions, with uniform result codes and authentication by parameter
  flag.
- AR-17: OTA verification with the core rollback hook; OTA only when `QuietGate` reports the boiler OFF (no new
  library).
- AR-18: resource rules of §14: flash margin ≥ 40 KB, RAM measured at every step, ≤ 30 ms per flash write.
- AR-19: migration order of §18, one step at a time with board test and owner acceptance.
- AR-20: the dependency rules R-A1…R-A7 become a grep check in `sh_process_check.py` (process chat).
**Decided by**: owner, 01.11.2026 (`architecture.md` §19).
**Affects**: FR-3, FR-12.2, FR-34, FR-47, FR-51, FR-60, FR-64, FR-67, FR-70, FR-71, NFR-2.2, NFR-2.6, NFR-2.8,
NFR-5.3; `architecture.md` §13, §14, §18; `tools/sh_process_check.py`; `docs/plan.md`.
**Consequences**: `docs/plan.md` tracks the 14 steps and the resource risks. The R-A grep check is an open process
gap (`process.md` §7). The entry "Process review approved; agent workflow for the clean-up and migration" above adds
that the firmware session may flash after `sh-verify-firmware` passes.

## 2026-11-01 — Requirements §6 ranges win over the AGENTS.md safety list (quiz)
**Context**: `AGENTS.md` §3 says interlock 120–180 s and frost < 12 °C; requirements §6 gives other ranges, and the
checker had copies of both.
**Decision**: The ranges of requirements §6 are the reference: `interlockDelay` 120–600 s, `frostTemp` 8–16 °C,
`burnerMinRun` / `burnerMinPause` 120–600 s, `sensorTimeout` 1–60 min.
**Decided by**: owner, quiz of 01.11.2026 (recorded in `activeContext.md` by the /simplify chat).
**Affects**: FR-2, FR-4, FR-16.1, FR-16.2, FR-18; requirements §6; `AGENTS.md` §3 (process review QA-2);
`.claude/agents/sh-safety-reviewer.md` (QA-3); `tools/sh_process_check.py` (reads the safety ranges from §6).
**Consequences**: The AGENTS.md safety list and the safety reviewer are rewritten to cite requirement IDs and
parameter names instead of numbers (QA-2, QA-3, approved 01.11.2026).

## 2026-11-01 — Work order: requirements and architecture first, then code; tests follow the code
**Context**: The v1 requirements, the tests and the code disagreed; baseline v2 and the architecture were rebuilt
before any code change.
**Decision**:
- Requirements and architecture are aligned and cover each other first, then the code is changed.
- Tests follow the code (not TDD): each migration step changes the code and its tests together; there is no separate
  test step first.
- G2 (`/api/boiler/on` and console `ON` start the burner in AUTO without the interlock) is fixed in migration steps 4
  and 9, with no separate hotfix.
**Decided by**: owner, 01.11.2026 (`activeContext.md`, requirements chat 01:29; `architecture.md` §18 "Order (owner,
01.11.2026)"; G2: changelog entry "Architecture: SWE.2 for baseline v2 approved", owner decisions).
**Affects**: `docs/process.md` §3 (its last line said tests first, then code — process review QA-6),
`architecture.md` §18, `tools/sh_test_system.py`.
**Consequences**: Process review QA-6 (approved 01.11.2026) adds a test review gate before a step is flashed
(`process.md` §3.1, §3.2), because
tests written after the code can follow the code instead of the requirements. G2 is the open safety problem PB-1
(`docs/problems.md`, QA-1), which blocks a release until steps 4 and 9 are accepted.

## 2026-11-01 — Superseded decisions
**Context**: The entries of this log from 2026-10-06 to 2026-10-22 describe a system that no longer exists, and
agents read this log as context (process review QA-5). This entry names every older decision that no longer holds and
what replaces it. The old entries stay unchanged (append-only).
**Decision**:

| Old entry | No longer holds | Replaced by |
|---|---|---|
| 2026-10-06 ESP32 Autonomous Edge Controller vs Smartphone Script | Galaxy A6 kept as MQTT broker, telemetry bridge and Grafana host | the controller works alone (NFR-1.1; requirements §1). Control on the ESP32 still holds. |
| 2026-10-06 Dual BLE + MQTT Telemetry Ingestion | MQTT fallback from the Galaxy A6 after 90 s | passive BLE scan only (FR-6.3, MQTT part retired); weak coverage → optional extra BLE receivers on the LAN (FR-72, D29) and DTC-105; sensor loss → `sensorTimeout` and FR-51 (D4, D35) |
| 2026-10-06 Electrothermal Actuator Opening Interlock | 120–180 s | `interlockDelay` in requirements §6, FR-2 (D1, quiz 2026-11-01) |
| 2026-10-06 Boiler RF Protocol & Transceiver Hardware | Tronic 4-LD7396-2 outdoor sensor over 433.92 MHz as weather source, SYN480R receiver for it | outdoor temperature from Open-Meteo only (FR-9; FR-12.3 retired); the boiler receiver is replaced by a fail-safe ESP32-class receiver (FR-55, D19). The frame replay still holds (FR-5.1). |
| 2026-10-07 Unified Gateway Integration via Galaxy-A6 (Nexus) & Cross-Repo Tracking | Nginx portal on the Galaxy A6, Tailscale access, cross-repo tracking in `docs/plan.md` | one page served by the controller (FR-8.1); Tailscale is the user's network, not product behaviour (FR-8.2 retired); portal tooling removed in lean phase 1 (changelog 29.10.2026) |
| 2026-10-07 Dedicated Wall Kiosk Display (Galaxy Tab A9) & Separation of Concerns | kiosk client running a PWA | kiosk / wall-tablet UI out of scope (requirements §1, AGENTS.md §2); PWA removed in lean phase 2; one page with family view and `#admin` (FR-8.1) |
| 2026-10-07 Future Server Strategy: Lenovo ThinkPad L590 Evaluation | a home server as part of this project | no server: all decisions on the controller (NFR-1.1); cloud and remote fleet out of scope (requirements §1) |
| 2026-10-15 Boiler Anti-Short-Cycling Reversal Guard & Cooldown Countdown | 60 s minimum burn, 60 s pause | `burnerMinRun` / `burnerMinPause` in requirements §6, FR-4 (D2; NFR-1.6 retired). The deferral as `PENDING_OFF` still holds (FR-32); the wait time is `waitSec` (FR-17.4). |
| 2026-10-16 Underflow Protection & Non-Volatile Runtime Tracking | billing (gas cost) counters | burner time counters only (FR-35). The underflow guard still holds. |
| 2026-10-16 ASPICE SWE.5 Non-Destructive State Isolation & Preset Preservation | presets, circadian schedule, "100 % test pass rate" | levels day / night / away with one weekday and one weekend slot list (FR-29, FR-30, FR-31; changelog 29.10.2026); a pass rate is not evidence while the tests do not follow the requirements (process review QA-1) |
| 2026-10-16 High-Density Responsive Web Interface | widescreen dashboard, embedded live console tab, dev-server proxy as part of the product | one page (FR-8.1); web console page retired (FR-7.2), console via Telnet and HTTP (FR-7.1, FR-45); FR-25, FR-26, FR-33 retired; the dev server is tooling only (NFR-2.7 retired) |
| 2026-10-18 Dedicated Home Server (ThinkPad L590) & Cloudflare Named Tunnel on mysmarthome.cc | home server, `mysmarthome.cc`, Cloudflare tunnel, Home Assistant | not part of this project: cloud, remote fleet and Home Assistant are out of scope (requirements §1, FR-36 retired); local API and page only |
| 2026-10-18 Test Integrity Invariant & Daily Zero-Gap Requirements Audit Protocol | the rules living in `systemPatterns.md`; "guarantees 100 % bidirectional traceability" | both rules are now in `docs/process.md` §3.2 (process review QA-25); `sh_process_check.py trace` reports gaps, it guarantees nothing. Not weakening a test still holds. |
| 2026-10-19 Multi-Tenant IoT SaaS Architecture & Edge Decoupling | SaaS portal, `mqtt.mysmarthome.cc`, outbound MQTT, `DevicePassport` | out of scope (AGENTS.md §2, requirements §1); SaaS documents removed in lean phase 1, `DevicePassport` removed in lean phase 2 (FR-13 retired, location → FR-9.2); cloud (FR-10.1) and fleet OTA (FR-10.5) retired |
| 2026-10-22 Architectural Consensus (/grill-me) | hybrid server with Home Assistant; optional Home Assistant over MQTT; master / slave controllers over ESP-NOW; multi-property accounts; B2C / B2B sales; PWA and wall kiosk; PocketBase backend | Home Assistant, kiosk, SaaS, fleet and sales out of scope (AGENTS.md §2, requirements §1); one controller with 1–32 zones and output expansion (FR-69, AR-8) plus optional extra BLE receivers on the LAN (FR-72); boiler through `BoilerLink` (FR-55, AR-9); no server backend; PWA removed in lean phase 2 |

Still valid: 2026-10-06 Hardware Board Selection & Zone Allocation (now the reference install, requirements §2;
zones are configuration, FR-11.3, FR-69); 2026-10-06 Git Push Protocol (AGENTS.md §5, `process.md` §5: commit and
push only on the owner's explicit request); 2026-10-07 Native ESP32 Migration (the `MqttTelemetryClient` it names no
longer exists).

Also replaced, from older memory-bank files and `docs/plan.md` (no entry of their own):
- ON/OFF hysteresis loop → TPI (FR-47; FR-1.1 retired).
- 30 min sensor timeout and valve closed on sensor loss → `sensorTimeout` and FR-51 (D4, D35).
- Service Mode and forced burner START → removed in lean phase 2 (FR-10.3 retired); only OFF may be forced (FR-17.3).
- Web console `/console` and setup page `/setup` → retired (FR-7.2, FR-11.2); discovery via API (FR-11.3).
- Weekly 20 s anti-seize → valve exercise after `exerciseIdleDays` (FR-19, D27).
- Bio-schedule, anchors and presets → levels and slot lists (FR-29, FR-30, FR-31).
- Frost threshold fixed at 12 °C → `frostTemp` (FR-16.1, D7).
- Task watchdog 8 s (hydraulics document) → NFR-1.5.
- `docs/plan.md` "Decisions (29.10.2026)": boost 16–22.5 °C → FR-1.2 (D10); phase order lean → UI → control is done
  and continues as the migration of `architecture.md` §18; own page without Home Assistant, page-only alerts and the
  TPI target hold as FR-8.1, FR-62 (D22) and FR-47.
- `docs/plan.md` "sensor-loss fallback needs a decision" → decided (D35).
- `docs/plan.md` design direction "heat source tier B = OpenTherm" → not in baseline v2; only tier A (ON/OFF) is
  required (requirements §2).

**Decided by**: the replacements were decided where the "Replaced by" column points: owner rows in the requirements
review and the architecture, the scope of AGENTS.md §2, and the removals of lean phases 1 and 2 (changelog
29.10.2026, which does not name who decided). This entry only records them (process review QA-5, approved by the
owner 01.11.2026).
**Affects**: the entries named above; `.agent/memory-bank/systemPatterns.md`, `techContext.md`, `productContext.md`,
`projectbrief.md`, `progress.md`, `docs/plan.md`, `docs/underfloor_heating_hydraulics_and_failsafes.md` (rewritten
the same day, process review QA-25, QA-27, QA-28).
**Consequences**: An agent that finds one of these decisions in an older file follows the replacement. New entries go
at the end of this log.

## 2026-11-01 — Requirements review: OTA without mapped sensors, radio cooldown vs reaction time, blueprint source (D37, D38, D39)
**Context**: Applying the process review produced three open questions in `docs/requirements_review.md` §8.1.
**Decision**:
- D38 = A: FR-60 skips the sensor check of a new image while no zone has a mapped sensor; loop/watchdog and Wi-Fi
  checks stay.
- D39 = A: the NFR-1.8 bound (2 s) applies to the safety decision and to every relay; a radio frame may additionally
  wait for the FR-17.2 cooldown (worst case 2 s + 3 s), which protects the receiver.
- D37 = B: the owner's blueprint `underfloor_heating_blueprint_2.pdf` stays outside the repository; its location and
  version are recorded in `requirements_review.md` §4.5. Location and version not given yet (open).
**Decided by**: owner (chat, 01.11.2026).
**Affects**: FR-60, NFR-1.8, FR-17.2, `requirements_review.md` §4.5 / §8.1, `architecture.md` §5, §7.7.
**Consequences**: OTA works on an uncommissioned board; the radio OFF frame keeps its cooldown; the B-n rows can
only be re-checked once the owner gives the blueprint's location.

## 2026-11-01 — Control in its own task, boiler timing, observations O-12…O-15, requirements frozen
**Context**: The architecture alignment to baseline v2.1 proposed AR-21 (bounded web server inside `loop()` to meet
NFR-1.8) and AR-22, and raised O-12…O-15 on the requirements. The owner answered by questionnaire.
**Decision**:
- AR-21 (as proposed) rejected. The web page must stay responsive; control may react 1–3 s later in the background.
  The control tick runs in its own FreeRTOS task; `WebServer` and console stay in `loop()`. Amends AR-3 and P6.
  RAM cost ~4–5 KB accepted; if the margin is short, the log ring goes from 8 to 4 KB.
- AR-22 approved: boiler state, run / pause timers and the overrun count from the frame actually sent.
- O-12: status reports the longest control-tick delay and longest loop pass (NFR-1.8 testable on the board).
- O-13: pump stop at the last valve (FR-14.5) and the forced manual OFF (FR-17.3) are in the NFR-1.8 list.
- O-14 = A: the commissioning result (FR-68) is a document filled at installation, not stored by the controller.
- O-15: the self-check may end a burner run early on a violated safety rule (FR-4), safety > wear.
- The requirements review is closed (owner sign-off); open action only: blueprint location and version (D37).
**Decided by**: owner (chat, 01.11.2026).
**Affects**: AR-3, AR-21, AR-22, P6, NFR-1.8, FR-4, FR-14.5, FR-17.3, FR-23, FR-68, `architecture.md` §5, §14.2, §16, §18.
**Consequences**: firmware migration may start at step 1 (`ParamStore`); the task split is placed in a later step by
the architecture.

## 2026-11-01 — Watchdog on both tasks, NFR-1.8 trigger and OTA exception (O-16)
**Context**: With the control tick in its own task (AR-21), NFR-1.5 watched only `loop()` and NFR-1.8 had no defined
trigger for a manual request; flash writes during OTA stall both cores.
**Decision**: Recommended option applied — the task watchdog covers `loop()` and the control task (either hang resets
into the safe boot); a manual request's reaction time counts from its acceptance by the controller, verified with the
longest control-tick delay (FR-23); the OTA exception of NFR-1.8 stays (boiler OFF during OTA, FR-60).
**Decided by**: owner (chat, 01.11.2026: "no preference", so the recommended option).
**Affects**: NFR-1.5, NFR-1.8, FR-17.3, FR-23, AR-21, `architecture.md` §5, §16.
**Consequences**: requirements baseline v2.3 with a post-closure review row.

## 2026-11-01 — Safety reaction bound simplified to 10 s (NFR-1.8)
**Context**: NFR-1.8 (2 s, from process review QA-16) produced follow-up exceptions (radio cooldown D39, OTA, task
watchdog). The plant reacts in minutes, and since AR-21 the web page can no longer hold up control.
**Decision**: Every safety reaction changes the outputs within 10 s of its trigger; the only exception is a firmware
update, which runs only with the boiler OFF. The radio-cooldown clause of D39 is superseded. Watchdog on both tasks,
"next control tick" in FR-37 / FR-17.3 and the measured longest control-tick delay (FR-23) stay.
**Decided by**: owner (chat, 01.11.2026, option B).
**Affects**: NFR-1.8, D39, AR-21, `architecture.md` §1, §5.
**Consequences**: one simple, testable bound; no special handling of radio frames for timing.

## 2026-11-02 — Tests first, in a clean room; one reviewer per work product
**Context**: The owner wants tests that check the requirements, not the code: tests written after the code could
copy it, and a failing test was sometimes changed instead of the firmware (TC16.5, 30.10.2026). Only safety had a
reviewer; requirements, tests and code had none of their own.
**Decision**:
- Each migration step writes its tests first (`sh-test-writer`), from the requirements, the architecture and the
  API reference only; the agent cannot read the code and can edit only the test script (hook
  `.claude/hooks/sh_clean_room.py`). The new tests must fail on the firmware before the step (red run); then the
  code is written until they pass; the tests stay frozen.
- Reviewers: `sh-req-reviewer` (requirements), `sh-test-reviewer` (tests, clean room), `sh-code-reviewer` (code
  against requirements and architecture) next to `sh-safety-reviewer`; links by `sh_process_check.py trace`.
  Skills `sh-review` and `sh-write-tests`.
**Decided by**: owner (chat, 02.11.2026: "Trei recenzori", "Da, întâi testele").
**Affects**: supersedes "tests follow the code (not TDD)" of 2026-11-01; `docs/process.md` §1, §3.1, §3.2, §9;
AGENTS.md §4; `sh-verify-firmware`; `architecture.md` §18 "no separate test step first" (to align).
**Consequences**: a step starts with its tests and a red run on the board; a test that cannot pass is a `PB-n` or
a requirements review row, never a test edit by the code session.

## 2026-11-02 — The owner's chain; commits inside it without asking
**Context**: The owner wrote requirements, code and tests with AI, reviewed the requirements, but has no time to
review code and tests or to run the process command by command ("mi-e peste mână"). The process tools existed but
nothing ran them in order. Parallel chats per zone had already been dropped (one main chat).
**Decision**:
- What the owner tells or asks in the main chat starts skill `sh-change`: log → requirement → tests first → code →
  gates → commit → board → owner test. The owner is asked only to decide a requirement change and to test on the
  board.
- Inside the chain the chat commits on the work branch once the gates pass; push, PR and `main` stay on request.
- The "one chat, one zone" rule is removed.
- Not now: merging agents, shortening the logs, a short checker output (offered, not chosen).
**Decided by**: owner (chat, 02.11.2026: "1. Lanțul automat", "Commit singur").
**Affects**: AGENTS.md §5, `docs/process.md` §1, §5, `.claude/skills/sh-change`, `sh-verify-firmware`,
`sh-write-tests`.
**Consequences**: the owner's acceptance rows refer to clean commits (no `-dirty` builds); the chain never
pushes.

## 2026-11-02 — Short logs and a short checker report
**Context**: Every chat read the changelog (1743 lines), this log and a 126-line checker report; most of it was about
the system before the lean clean-up. The owner asked for less token use (02.11.2026).
**Decision**:
- Old log entries move unchanged to `docs/archive/` (they are not deleted); the logs stay append-only.
- `activeContext.md` holds only "Now" and "Next"; a line that no longer holds is replaced.
- The five memory-bank pointer files (`productContext`, `progress`, `projectbrief`, `systemPatterns`, `techContext`)
  are deleted.
- The checker's default run prints a WARN count per section; a named check, `--full` and CI list every line.
- Not chosen: merging agents (the safety reviewer stays on its own).
**Decided by**: owner (chat, 02.11.2026: "Jurnale scurte", "Verificare pe scurt").
**Affects**: AGENTS.md §5, `docs/process.md` intro and §3, `tools/sh_process_check.py`, `sh-verify-firmware`,
`docs/archive/`.
**Consequences**: a WARN is no longer seen in the default run; who needs the lines (requirements review, test and
code reviewers) runs the named check, which they already did.

## 2026-11-02 — Independent step assessor; the device is prepared for sale in the EU
**Context**: The owner's chain does the work and reports on it itself; the owner has no time to check the
evidence of each step. The owner wants to sell the device in the EU: hardware made to order by a factory, the
software by the owner, no third-party certification.
**Decision**:
- `sh-assessor` (read-only) checks the evidence of every migration step at its end, before the owner's board
  test: requirements decided, frozen tests committed before the code and red on the old firmware, reviews acted
  on, gates on one clean commit, records, acceptance. It never judges code or test content.
- Inside the chain the frozen tests are committed alone, before the code, so "tests first" is visible in git.
- Scope: the device is prepared for sale in the EU under the owner's brand; the owner is the manufacturer in law.
  "No sales" in AGENTS.md §2 becomes "no shop"; cloud, SaaS, fleet, Home Assistant and kiosk stay out.
- `docs/compliance.md` lists what EU law asks of the software (CRA, RED cybersecurity and radio, EN 60730 Annex H),
  with status and technical file; `sh-compliance-reviewer` (read-only) checks it at the end of every step and
  when a requirement about access, credentials, updates, user data or the radio changes. An obligation becomes a
  requirement only through the chain, by the owner's decision.
- Not chosen now: aligning the process table names to ASPICE 4.0 (offered, not answered).
**Decided by**: owner (chat, 02.11.2026: "de acord cu evaluatorul la final de pas", "Eu, sub marca mea",
"Pregătit pentru UE").
**Affects**: AGENTS.md §2, §5; `docs/process.md` §1, §3.1, §9; `docs/plan.md` out of scope; `docs/compliance.md`;
`.claude/agents/sh-assessor.md`, `sh-compliance-reviewer.md`; skills `sh-change`, `sh-write-tests`, `sh-review`.
**Consequences**: conformity is self-declared, so the technical file (`docs/compliance.md` §4) is the evidence;
a consultant or test lab confirms the product category, the software class and the standards before the first
sale. The first compliance review will find requirement gaps (e.g. universal default credentials, FR-59) for the
owner to decide.

## 2026-11-02 — Configuration export / import now (C-9); zones and sensors set by the user (C-10, C-11); order
**Context**: The owner wants to keep using the board while the software changes, and to set up zones and sensors
themselves like a new user would. The board lists the first 8 sensors it hears and never drops one (PB-55); zones
come in fixed slots. FR-65 (export / import) was `[later]`, after step 14.
**Decision**:
- C-9: export / import (FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8) is built now, as its own
  change before any migration step. Import applies what is valid and reports the rest: unknown names are skipped,
  missing ones keep their value, out-of-range values are skipped; zones are checked together and applied or skipped
  whole; not JSON or an unknown format → HTTP 400, nothing changed. Credentials are never exported or imported.
  Import joins the FR-58 list (authentication from step 9); whether the export needs authentication stays with D49.
- C-10: a new controller starts with no zone; the user adds and deletes zones, at most one per free relay output;
  a zone with an energised valve cannot be deleted.
- C-11: a zone's sensor is set by typing its MAC or from a search the user starts (until Stop, at most 10 min);
  outside a search only the sensors assigned to zones are kept (fixes PB-55).
- Order: the FR-65 change first, then migration step 3 (`OutputBank`, with C-10 and C-11), then step 2.
**Decided by**: owner (chat, 02.11.2026, through the coordinator session: decisions 1-4 of the owner's request).
**Affects**: FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8, FR-58, FR-8.1, FR-11.3, FR-69, requirements §6 `number of zones`;
`docs/requirements_review.md` §9 C-9 (C-10, C-11 when step 3 starts), `docs/plan.md`, `docs/architecture.md` §3.2,
§9.3, §18, `docs/api_reference.md`, `docs/compliance.md` CMP-5.
**Consequences**: the board can be exported before each flash and restored after; an import is a full
reconfiguration in one request, so it waits for FR-58 (step 9) like every other write (PB-44); the unbounded HTTP
body of every POST is logged as PB-56.

## 2026-11-02 — Model and effort per kind of work
**Context**: The owner runs Opus at a high effort everywhere and the token use is too high, also in the main chat.
**Decision**: the strongest model only where a mistake can burn the boiler or the floor. Main chat Sonnet medium;
code on the safety list, safety decisions and cross-module design Opus high; other code, tests and documents
Sonnet medium; lookups and records Haiku low. Agents carry `model` / `effort` in their frontmatter; tests of the
safety list and the full EU review run with Opus high. Agents read by section, never whole documents.
**Decided by**: owner (chat, 02.11.2026: "să faci cumva ca modelul OPUS sau SONNET sau HAIKU să fie folosite optim").
**Affects**: AGENTS.md §5; `.claude/agents/*`; skills `sh-write-tests`, `sh-review`.
**Consequences**: the model of the main chat is the owner's setting in the app; the table says what to pick.

## 2026-11-02 — Fewer agent runs per step (tokens)
**Context**: A step started up to seven agents, each rereading the requirements, the architecture and the 4000-line
test file; the owner saw the token use (screenshot of the export / import session).
**Decision**: `sh-req-reviewer` only on changed requirement IDs; `sh-safety-reviewer` only when the change touches
the AGENTS.md §3 safety list; the test reviewer once per set of tests (hash of `tools/sh_test_system.py` in the step
record, no second run on the same hash); `sh-code-reviewer` only on the files changed since the last review; the
test writer gets the area of the test file to read. Not done yet: splitting `tools/sh_test_system.py` by area (it is
being edited by the firmware session; later).
**Decided by**: owner (chat, 02.11.2026: "ok, fa ce trebuie sa faci").
**Affects**: skills `sh-change`, `sh-write-tests`, `sh-verify-firmware`.
**Consequences**: less assurance per run on changes outside the safety list; the checker, the tests and the owner's
board test stay as they were.

## 2026-11-02 — Light workflow by default, full on request and at release
**Context**: The full chain (up to eight agents per change) took the owner to 45 % of the weekly limit on a Monday;
at the start of the project most changes do not need it (owner: "nu vad necesar pentru orice scărpinat").
**Decision**: two modes of `sh-change`. Light (default): the chat does every step, tests first written by itself,
no review agents; the one exception kept by the assistant: a change that touches the AGENTS.md §3 safety list gets
one `sh-safety-reviewer` run. Full: all agents, `sh-assessor`, `sh-compliance-reviewer` - when the owner asks and
before a release. Replaces the "Fewer agent runs per step" rules for the default path.
**Decided by**: owner (chat, 02.11.2026).
**Affects**: AGENTS.md §4, §5; `docs/process.md` §3.1; skills `sh-change`, `sh-verify-firmware`.
**Consequences**: in light mode no clean-room hook protects the tests, so "tests follow the requirement" rests on
the rule and on `trace`; the checker, the system tests on the board, the board check and the owner's test stay.

## 2026-11-02 — The order "tests before code" is kept by a hook, not by an agent
**Context**: In light mode no reviewer guards "tests are not written after the code, to pass". The owner: it must hold,
and a bad test or requirement must come to them for clarification.
**Decision**: `tools/sh_chain.py` keeps a local marker; the hook `.claude/hooks/sh_chain_guard.py` (project
`.claude/settings.json`) closes `src/`, `include/`, `web/` until the tests are frozen and closes the test file after
the freeze; `sh_process_check.py process` errors when a frozen test file changed (shell edits). No marker, no limits.
Unfreezing needs the owner's decision (`--because PB-n | D-n | C-n | O-n`). The semantic test review stays in full mode
(Sonnet): a hook cannot judge whether a test checks the whole requirement.
**Decided by**: owner (chat, 02.11.2026).
**Affects**: skill `sh-change`, AGENTS.md §5, `docs/process.md` §3.1, `tools/sh_process_check.py`.
**Consequences**: the chat can still read the code before writing the tests; the guard fixes the order and the
immutability, not the independence of the author.

## 2026-11-02 — One Save for all zones, sensors checked on the result (C-12)
**Context**: with one Save button per zone, a sensor cannot move to another zone while the old zone still holds it
(map-zone returns "Sensor already used by zone n"); the owner assigned sensors wrongly and could not correct them.
**Decision**: the engineering view saves all zones with one button; the zones are checked together on the
configuration that results, as the import already does for relays (FR-65.5); the import checks sensors the same way.
Single-zone `map-zone` (API, console) keeps its strict check.
**Decided by**: owner (chat, 02.11.2026).
**Affects**: FR-11.3, FR-65.5, FR-65.6, `api_reference.md`, `web/index.html`, `src/ConfigTransfer.cpp`.
**Consequences**: a bad zone is skipped alone, the others are saved; a partial save is reported on the page.

## 2026-11-02 — Zone target out of the settings file, setpoints are the levels (C-13)
**Context**: PB-62: the zone `target` is rewritten by the schedule at every control pass, so it is no setting. The owner:
what must survive a test or a reflash is the setpoint of every zone at every threshold of the day.
**Decision**: the file drops `zones[].target`; the day, night and away setpoints stay in `schedule.levels` and an import
restores them (a test proves it). An old file with `target` skips it as unknown (FR-65.4).
**Decided by**: owner (chat, 02.11.2026).
**Affects**: FR-65.1, `api_reference.md`, `ConfigTransfer.cpp`, tests TC29.2, TC29.9.
**Consequences**: none for the heating; the file is smaller.

## 2026-11-03 — Underfloor thermal deficit full duty, weather compensation & UI ergonomics (C-14)
**Context**: PB-66 (misleading "Temperatură atinsă" status in UI when below target), PB-67 (corner room underheating due to 60 min cycle 33% duty shutting off after 20 min, dissipating screed momentum), plus UI inputs wiped by polling refresh on blur, and confusing separate sensor search/stop buttons.
**Decision**:
1. For FR-47 (Option 1): When thermal deficit `(target - room) >= 0.4 °C`, duty is 100% (continuous heat, no TPI inter-pulse off) to heat screed. When deficit `< 0.4 °C`, modulated TPI engages with outdoor temperature compensation (+1.5% base duty per °C below 15 °C outdoor, up to 30%) and learned base duty.
2. UI Sensor Search: Single 2-in-1 toggle button (Caută senzori / Oprește căutarea).
3. UI Input Protection & Undo: Form editing marks inputs dirty, preventing periodic refresh poll overwriting; add Undo (Anulează) button.
4. Schedule: Visual slot editing with add/remove moments, placing Away (Plecat) last.
5. UI Zone State: In `IDLE`, if `currentTemp < target - 0.2`, show "Pauză TPI", reserving "Temperatură atinsă" for when target is actually reached.
**Decided by**: owner (chat, 03.11.2026).
**Affects**: FR-47, FR-8.1, FR-8.4, FR-31, `web/index.html`, `src/ThermalZoneManager.cpp`, `include/ThermalZoneManager.h`.
**Consequences**: corner rooms heat reliably without premature cutoff; user form edits cannot be wiped by polling.

## 2026-11-03 — Web asset decoupled LittleFS partition & dual versioning proposal
**Context**: Re-flashing the full 1.25 MB firmware binary for minor UI/CSS/HTML tweaks takes ~20 s, reboots the ESP32, and interrupts hydronic timers and runtime state.
**Proposal**:
1. Decouple web dashboard assets from the binary into the existing LittleFS partition (or a dedicated asset partition).
2. WebServer serves /web/index.html.gz from LittleFS if present, falling back to embedded ROM dashboard_html_gz.h if missing or corrupted (anti-brick).
3. Fast asset upload endpoint (POST /api/web/update or OTA asset channel) uploading in < 500 ms without reboot.
4. Versioning scheme review: Decouple firmware version (FR-21, 1.MM.DD.xx) from web UI version (ui-1.MM.DD.yy or 1.MM.DD.xx-ui.yy) so SemVer/API parsers are not broken by a 5-tuple format.
**Proposed by**: owner (chat, 03.11.2026).
**Target Phase**: Migration step 12 (OTA, asset management, FR-60).
**Consequences**: Zero-downtime UI updates without resetting boiler state, heating loops, or interlock timers.

## 2026-11-03 — Keep the work built outside the chain (31b8496..6170f16)
**Context**: 43 commits were built and flashed with another AI tool without tests first, safety review or acceptance (PB-83, PB-84, PB-85).
**Decision**: keep it. It is folded into migration steps 7 (`ZoneController`: duty rule, window detection), 8 (`Schedule`: moments) and 11 (`History`, gas meter) with the tests written first for each step; the PB-71 ... PB-94 findings are fixed inside those steps. No revert.
**Decided by**: owner (chat, 03.11.2026).
**Affects**: `docs/plan.md` steps 7, 8, 11; PB-71 ... PB-94.
**Consequences**: the code stays on the board; its tests and reviews come with the steps.

## 2026-11-03 — Owner decisions on the process review of 31b8496..6170f16
**Context**: `docs/review_agy_03_11.md` §4, relayed by the main chat.
**Decision**:
1. Console: login in the page and for Telnet; the log never shows passwords (PB-43, PB-82). Only `/api/console/send` stays, `/api/console/cmd` is removed and the page uses `/send`. A security-event log (failed logins, console commands, with time, visible in the engineering view) is wanted: a new requirement, checked with `sh-compliance-reviewer`.
2. The out-of-chain work (43 commits) is kept and its features are assigned to their migration steps (PB-83). The tests for it are written after, from the requirements (changes the line "tests first" of the entry above for this work only).
3. Window baseline is 5 min as FR-40 says: the code is fixed (PB-72).
4. The 1.25 limit of NFR-2.8 is in MiB (the partition size): the unit is written into NFR-2.8.
5. The old 8-slot schedule model is removed: one model with 4 moments; old export files are converted on first read (PB-92, PB-73, PB-75).
6. Wording of FR-34, FR-40, FR-35 counters and FR-31 is written from what the board does now, in the requirement language; the texts are shown to the owner before the tests (PB-90).
**Decided by**: owner (chat, 03.11.2026), relayed by the main chat.
**Affects**: FR-58, FR-45, FR-7.1, NFR-2.8, FR-29, FR-31, FR-34, FR-35, FR-40; PB-72, PB-73, PB-75, PB-82, PB-83, PB-86, PB-90, PB-92.
**Consequences**: order of work: register the five unregistered tests and run them (PB-86); cheap fixes (window 5 min, meter default, owner's day / night levels); then the console login in its own round.

## 2026-11-03 — Gas meter readings are data, not a calibration (C-25)
**Context**: every POST to `/api/boiler/gas-meter` wrote a log entry in an 8-entry ring and moved the base; the owner's readings were mixed with test entries and could be overwritten (PB-80, PB-99).
**Decision**: a reading typed by the user is stored with its time in a LittleFS file that survives a flash, reset and import; never overwritten or dropped; only a typed reading writes an entry; the page says "Citire contor"; no default reading. The physical reading 2265.304 m3 was taken at 15:39 on 03.11.2026 (owner) and seeds the log. Cap (1000 entries, monthly compaction after 13 months) and the optional reading time are proposals awaiting the owner's OK.
**Decided by**: owner (chat, 03.11.2026), relayed by the main chat; text of FR-35.1 shown to the owner before the tests.
**Affects**: FR-35.1, FR-65.1, `WebApi.cpp`, `docs/gas_meter_readings_03_11.md`.
**Consequences**: round of C-25 follows after the owner's OK of the text; the board's meter value is not touched until the owner types the current reading.

## 2026-11-03 — "Correct a reading" (C-28) rejected, flow from the history (C-29)
**Context**: the owner first said the first reading was taken at 15:30, then confirmed 15:39; the board entry (15:39) is right. The owner will not type past readings.
**Decision**: C-28 (correct a stored reading) is rejected: no requirement, no code, no tests. C-29: the measured flow takes the boiler ON time from the telemetry history when a reading has no stored runtime (24 h history while both readings are under 24 h old, else 5 day history with an interval of at least 4 h ON time; the history must be complete for the interval).
**Decided by**: owner (chat, 03.11.2026), relayed by the main chat.
**Affects**: FR-35.1, `TelemetryHistoryManager`, `docs/gas_meter_readings_03_11.md`.
**Consequences**: the readings 2265.304 (15:39) and 2265.996 (21:59) are enough to measure the flow; no manual correction function.

## 2026-11-03 — Event history read path (step 2, FR-62)
**Context**: FR-62 kept 50 events across reboots but named no way to read them; step 2 cannot be tested without one.
**Decision**: `GET /api/diag/events` (record fields in FR-62), types limited to `dtc-raised`, `dtc-cleared`, `restart` in step 2. Made by the firmware session as the smallest addition; **pending the owner's confirmation** at the hands-on test of step 2.
**Decided by**: firmware session (Claude), owner to confirm.
**Affects**: FR-62, `docs/api_reference.md`, `Diagnostics`.
**Consequences**: no value or safety behaviour changes; the owner may change the path or fields.

## 2026-11-04 — Unfreeze of step 2 tests (D51)

**Context**: the frozen-tests hash did not match after a `git stash` in the step 2 session; the diff against the frozen tests is only added lines (TC32.1…TC32.3).
**Decision**: the owner approved the unfreeze in chat on 04.11.2026; no existing test changes.
**Decided by**: owner.
**Affects**: `tools/sh_tests/t_health.py`, `tools/sh_test_system.py`, step 2.
**Consequences**: the session freezes the tests again at the new hash after committing.

## 2026-11-04 — Event history read path confirmed
**Context**: decision of 2026-11-03 (step 2, FR-62) was pending the owner.
**Decision**: `GET /api/diag/events` accepted as built.
**Decided by**: owner (chat, 04.11.2026), relayed by the main chat.
**Affects**: FR-62, `docs/api_reference.md`.
**Consequences**: none.

## 2026-11-04 — Fixture fix of TC34.5 and validMin below frostTemp (D52)

**Context**: TC34.5 (sensor timeout) fails because of its own fixture (the test zone heated at the program level and DTC-101 was read globally, hit by the real Kids sensor); the safety review found `validMin` may be set at or above `frostTemp`, which blinds frost protection.
**Decision**: the owner approved in chat on 04.11.2026: (1) the TC34.5 fixture is repaired (test zone target 15 C, DTC-101 read for the test zone only), the asserted value does not change, no other test changes; (2) `validMin` must stay below `frostTemp`, a setting that breaks this is refused.
**Decided by**: owner.
**Affects**: `tools/sh_tests` TC34.5, FR-51 / FR-16.1 parameter ranges (requirements section 6), `ParamStore`.
**Consequences**: the group session writes the requirement change and its test first; memory work follows before any new step (owner, 04.11.2026).

## 2026-11-04 — Flash limit is the real OTA slot; permanent memory endpoint (D-53, C-32, C-33)

**Context**: memory review (PB-115): the limit 1.25 MiB was the old default slot; the real OTA slot is 1 966 080 B. Heap dips under 100 KB (min 90.7 KB, 47 of 180 samples under 100 KB) and nothing shows who uses it.
**Decision**: (a) the firmware limit is the OTA slot minus 100 KiB = 1 863 680 B (NFR-2.8). (b) `GET /api/diag/mem` stays permanently (FR-10.6), with a heap line in the debug area. (c) the first heap batch runs in one session: `WebApi` streaming instead of `String`, weather task stack, serial TX buffer; stop at heap typically >= 105 KB with minimum >= 100 KB.
**Decided by**: owner (chat, 04.11.2026), relayed by the main chat.
**Affects**: NFR-2.8, FR-10.6, AGENTS.md §3, `architecture.md` §14.1 / AR-18, `plan.md` R-1 / R-2, `tools/sh_process_check.py`, PB-115.
**Consequences**: more room for steps 12 to 14; the safety list code is not touched by the batch.
