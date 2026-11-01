# Decisions Log: Smart Heating System

<!-- Append-only Architecture Decision Record (ADR) log. -->

## 2026-10-18 — Dedicated Home Server (ThinkPad L590) & Cloudflare Named Tunnel on mysmarthome.cc
**Context**: External mobile access without VPN/Tailscale client apps was required for Home Assistant, Smart Heating Odaia Clasic, and Galaxy-A6 services. Furthermore, running long-term server daemons (Home Assistant, PostgreSQL, TeslaMate, Nginx, Mosquitto) on a mobile phone (Galaxy-A6) and WSL laptop introduces battery swelling risks, resource limits, and WSL port bindings.
**Decision**:
- Acquired custom domain `mysmarthome.cc` managed via Cloudflare authoritative nameservers (`devin` / `elisabeth.ns.cloudflare.com`).
- Deployed a permanent outbound Cloudflare Tunnel (`cloudflared.service` via QUIC) eliminating port forwarding, dynamic DNS issues, and public IP vulnerability.
- Enforced edge Basic Authentication (credentials kept outside the repo) in front of the local Nginx proxy (`127.0.0.1:8088`).
- Formally decided to migrate central infrastructure onto a dedicated Lenovo ThinkPad L590 (Intel Core i7 Gen 8) running Linux + Docker Compose. The laptop's battery acts as an integrated UPS, providing uninterrupted operations during power cuts, while freeing the ESP32 to focus strictly on real-time hardware safety, relays, and its headless REST API.
**Consequences**: Solid multi-tiered architecture: ESP32 remains hard real-time edge controller; ThinkPad L590 serves as durable 24/7 Home Server; Cloudflare delivers globally cached, encrypted, and bot-protected access.

## 2026-10-07 — Permanent Native ESP32 Migration & Elimination of Legacy ESP8266 Code
**Context**: The new 4-relay board arrived physically equipped with an ESP32-WROOM-32E module (DOIT ESP32-32E N4, ESP32-D0WD-V3, 4MB Flash). To ensure long-term maintainability and prevent technical debt when changing machines, the user requested removing all `#ifdef ESP8266` preprocessor hacks and standardizing 100% on native ESP32 code.
**Decision**:
- Fully remove all `#ifdef ESP8266` / `#elif defined(ESP32)` conditional compilation branches across `Config.h`, `main.cpp`, `BoilerRadioTrigger.cpp`, and `MqttTelemetryClient`.
- Standardize purely on native ESP32 libraries (`<WiFi.h>`, `<WebServer.h>`, `<HTTPUpdateServer.h>`, ESP32 Efuse MAC, fast standard GPIO).
- Set `platformio.ini` default to `[env:esp32dev]`.
- Created comprehensive hardware, pinout, wiring, and flashing reference in `docs/hardware_esp32_relay.md`.
**Consequences**: The codebase is clean, readable, modern, and completely immune to stale environment quirks or laptop changes.

## 2026-10-06 — ESP32 Autonomous Edge Controller vs Smartphone Script
**Context**: The user has a Samsung Galaxy A6 already running Termux/PostgreSQL/MQTT, and wanted to decide where the core heating logic and web interface should live.
**Decision**: Place the core heating state machine, relay switching, failsafes, and embedded Web UI directly on the ESP32 (in flash/LittleFS), rather than running a Python daemon on the Galaxy A6.
**Consequences**: The heating system is 100% immune to smartphone reboots, Wi-Fi drops, Android battery manager actions, or database glitches. The A6 is retained as a complementary MQTT broker, telemetry bridge for distant sensors, and long-term Grafana visualizer.
**Alternatives considered**: Pure Python daemon on Samsung Galaxy A6 controlling relays via HTTP/MQTT commands to an ESP8266/ESP32. Rejected due to single point of failure (if phone/Android OS restarts or freezes in winter, heating stops).

## 2026-10-06 — Dual BLE + MQTT Telemetry Ingestion
**Context**: Xiaomi LYWSD03MMC sensors broadcast BTHome v2 beacons. ESP32 has native BLE, but the manifold may be shielded behind thick reinforced concrete walls.
**Decision**: Implement a hybrid telemetry feed. ESP32 passively scans BLE directly; if a room sensor is not heard for > 90 seconds, ESP32 falls back to the room temperature published by the Galaxy A6 over local MQTT.
**Consequences**: Maximizes autonomy while providing guaranteed apartment-wide coverage.
**Alternatives considered**: Relying solely on ESP32 BLE (risk of blind spots) or solely on MQTT from A6 (destroys offline autonomy).

## 2026-10-06 — Electrothermal Actuator Opening Interlock
**Context**: 230V NC electrothermal actuators rely on wax expansion and require 2 to 3 minutes to physically open the valve port.
**Decision**: Enforce a software interlock delay (120–180s) between powering an actuator relay and firing the boiler flame radio trigger.
**Consequences**: Prevents boiler circulation pump cavitation and overpressure against closed valves.

## 2026-10-06 — Hardware Board Selection & Zone Allocation
**Context**: The apartment has 7 underfloor heating loops and 2 bathrooms with wall-mounted towel radiators. We needed to choose the relay hardware and assign zones.
**Decision**: 
- Selected the ESP32 4-Relay Board (Songle 10A 250VAC) with on-board ESP32-WROOM.
- Grouped the 7 manifold loops into 3 thermal zones:
  - Relay 1: Living + Kitchen + Dining + Vestibule (loops 1, 2, 3 in parallel).
  - Relay 2: Child Bedroom (loops 6, 7 in parallel).
  - Relay 3: Master Bedroom (loops 4, 5 in parallel).
  - Relay 4: Spare / Auxiliary.
- Bathrooms remain un-actuated on manual bimetal valves to provide permanent hydraulic bypass protection for the boiler pump.
- Boiler trigger will NOT consume a mechanical relay; it is driven via GPIO to an external 433.92 MHz transmitter (SYN115).

## 2026-10-06 — Boiler RF Protocol & Transceiver Hardware
**Context**: Need to trigger the boiler wirelessly without opening or modifying the boiler receiver unit.
**Decision**:
- Replay the sniffed 410-transition pre-modulated signal from `control_RF_Centrala.ino` via ESP32 GPIO.
- Purchased 2x SYN115 (TX) + SYN480R (RX) sets. SYN115 uses quartz PLL; SYN480R is superheterodyne (-107 dBm).
- Quarter-wave 17.3 cm copper wire antennas attached to ANT pins.
- Weather compensation integrates Tronic 4-LD7396-2 on 433.92 MHz with Open-Meteo REST API as Wi-Fi fallback.

## 2026-10-06 — Git Push Protocol: Explicit User Confirmation Required
**Context**: Remote git operations affect upstream repositories.
**Decision**: NEVER execute `git push` automatically or as a side-effect. Always display `git status` / pending commits and wait for explicit confirmation from the user in chat before pushing.
**Consequences**: Eliminates unintended upstream pushes; ensures user reviews all changes first.

## 2026-10-07 — Unified Gateway Integration via Galaxy-A6 (Nexus) & Cross-Repo Tracking
**Context**: Heating controller must be safely controllable from outside the house without exposing ports to the public internet, fitting into the existing single pane of glass (`http://galaxy-a6/`).
**Decision**: 
- Integrate Smart Heating as the 4th service in Samsung Galaxy A6's Smart Services Portal (Nginx reverse proxy under `/heating/`).
- Remote access is achieved securely via Tailscale (`http://100.83.135.74/heating/`).
- Portal configuration files live canonically in `smart-aquarium-esp8266/tools/nexus/` (`nexus_nginx.conf`, `nexus_portal.html`). All cross-repository edits are logged and tracked in `docs/plan.md` so that commits/pushes across repos remain synchronized and under user control.
**Consequences**: Seamless unified access across local LAN and remote Tailscale; zero port forwarding on home router; strict change-tracking across multiple git repos.

## 2026-10-07 — Dedicated Wall Kiosk Display (Galaxy Tab A9) & Separation of Concerns
**Context**: The wall-mounted user interface needs to be clean, responsive, locked against tampering/children, battery-safe 24/7, and occasionally usable for personal entertainment.
**Decision**: 
- Selected **Samsung Galaxy Tab A9 4G LTE (SM-X115)** with 8.7" PLS LCD and MediaTek Helio G99 (6nm).
- The tablet remains a dedicated **Kiosk Client (Display)** running a PWA via Fully Kiosk Browser, NOT a backend server.
- The tablet remains unrooted (preserving Widevine L1 for Full HD Netflix/YouTube, Knox 0x0, and warranty) while using native Samsung *Protect Battery* (85% limit) for safe 24/7 wall power.
**Consequences**: The tablet can be detached from its magnetic wall mount for couch media consumption without risking household heating or telemetry downtime.

## 2026-10-07 — Future Server Strategy: Lenovo ThinkPad L590 Evaluation
**Context**: Samsung Galaxy A6 ("Cinderella") is an aging smartphone running multiple heavy server stacks (PostgreSQL 17, TeslaMate, Mosquitto, Nginx, Python BLE scanner). User considers long-term alternatives.
**Decision**: 
- Cataloged user's unused **Lenovo ThinkPad L590** (Intel 8th Gen Quad-Core, NVMe SSD, Gigabit LAN, integrated battery acting as UPS) as the prime zero-cost candidate for future server migration.
- Benchmarked idle power consumption on Linux headless with lid closed: 4.5W–6.5W (virtually identical to a Raspberry Pi 4 with SSD, negligible electricity cost).
- Logged as a "Nice to Have / Future" milestone in `docs/plan.md` without rushing migration while A6 remains functional.

## 2026-10-15 — Boiler Anti-Short-Cycling Reversal Guard & Cooldown Countdown
**Context**: Rapid user setpoint adjustments or oscillating hysteresis could trigger repeated boiler start/stop RF commands, violating manufacturer safety limits and increasing component wear.
**Decision**:
- Enforce strict 60s minimum burner flame duration and 60s cooldown pause guard.
- When boiler is turned off during flame phase, schedule `PENDING_OFF` rather than dropping or blocking.
- Expose `boilerCooldownSec` and `pendingAction` in `/api/zones` for live countdown badge in UI.
**Consequences**: Hardware safety guaranteed; zero boiler spam; user sees precise real-time countdown.

## 2026-10-16 — Underflow Protection & Non-Volatile Runtime Tracking
**Context**: Negative timestamp delta during rapid reversal tests caused integer underflow in `(millis() - _flameStartTimeMs)`, wrapping `uint32_t` to ~4.29 billion seconds (~1193h) in NVS runtime counters.
**Decision**:
- Added timestamp sanity check `now >= _flameStartTimeMs` and upper bound clamp `burnSec < 86400UL`.
- Added `resetRuntime()` method, `POST /api/boiler/runtime/reset` REST endpoint, and `BOILER RESET RUNTIME` network console command.
**Consequences**: Rock-solid telemetry; user can calibrate runtime and billing counters anytime.

## 2026-10-16 — ASPICE SWE.5 Non-Destructive State Isolation & Preset Preservation
**Context**: Running the automated test suite `test_system.py` restored the factory 'default' preset (`Standard / Confort`) and factory circadian schedule via `SWE.5-TC17.3` and `SWE.5-TC18.4`, overwriting the user's active preset (`custom1` / `Bogdan`).
**Decision**:
- Capture live `user_initial_preset` at test suite launch (`run_all`).
- Automatically restore `self.user_initial_preset` in the final defuse/cleanup phase.
- Add explicit transactional transition logging in firmware: `[Schedule][PRESET] Preset switched: 'old' -> 'new'`.
**Consequences**: 100% test pass rate achieved without altering or resetting real-world user operational states.

## 2026-10-16 — High-Density Responsive Web Interface (Widescreen 16:9 + 720p Mobile/Tablet Optimization)
**Context**: UI cards were cramped in 500px columns on widescreen displays, boiler action buttons took excessive space in automatic mode, and 3 separate header cards consumed vertical real estate on wall tablets.
**Decision**:
- Expanded `.container` `max-width` to 1560px with centered margin for 16:9 widescreen displays.
- Raised 2-column breakpoint to 1140px; below 1140px, elements stack vertically in natural order via `display: contents;`.
- Combined 3 top cards into a single unified top navigation header bar (`.top-nav-card`).
- Boiler card compacted to a single 40px strip in AUTO mode (action buttons hidden; runtime counters moved to `#modal-gas-costs`).
- Zone row power button moved to left between BLE indicator and name; intrusive green 'Repaus' badge removed; 'țintă' label removed.
- Implemented reverse proxy in `tools/dev_server.py` and embedded `>_ Consolă Live` tab in UI.
## 2026-10-18 — Test Integrity Invariant (ASPICE SWE.5) & Daily Zero-Gap Requirements Audit Protocol
**Context**: Automated testing must serve as a rigorous detector of real implementation flaws and safety hazards. In addition, new firmware features or endpoints must never exist without formal requirements and test verification.
**Decision**:
- Enforce the Anti-Weakening Invariant: Weakening assertions to artificially pass tests is strictly prohibited. When tests fail, the firmware implementation must be corrected or a defect logged.
- Enforce Daily Zero-Gap Requirements Audit: Proactively alert the user at the start of each session if any implementation lacks requirements or test coverage, requesting explicit authorization to address it.
**Consequences**: Eliminates technical debt, guarantees 100% ASPICE SWE.5 bidirectional traceability, and ensures physical heating safety.

## 2026-10-19 — Multi-Tenant IoT SaaS Architecture & Edge Decoupling
**Context**: Transitioning from a single-home server to a commercial Multi-Tenant IoT Platform as a Service (PaaS) under `mysmarthome.cc`. The platform must serve Client 0 (Admin / personal services), Client 1 (Residential Smart Heating + private TeslaMate), Client 2 (Standalone offline-first ESP32 with cloud telemetry/OTA updates), and Client 3 (Remote heating control via Web/Mobile app without Tailscale).
**Decision**:
- Adopt three-tier domain topology: `mysmarthome.cc` (public showcase), `app.mysmarthome.cc` (authenticated multi-tenant portal with tenant filtering), and `mqtt.mysmarthome.cc` (secure outbound device ingestion and fleet management).
- ESP32 firmware remains offline-first for physical heating safety, while adopting dual-mode outbound MQTT connectivity using persistent device credentials (`DevicePassport`).
- Decouple Web UI from hardcoded local IPs (`192.168.1.20`) to allow running identical frontends locally, via Nginx gateway, or hosted in the cloud.
- Comprehensive technical specification documented in `docs/architecture_multitenant_iot_saas.md`.
**Consequences**: Clean separation of concerns; zero port-forwarding on client routers; hardware safety guaranteed at the edge; infrastructure ready for multi-client scale.




## 2026-10-22 — Architectural Consensus (/grill-me): Hybrid Server, Master-Slave ESP-NOW Topology, PWA & PocketBase Multi-Tenant Backend
**Context**: The user initiated a systematic /grill-me architectural review to lock in commercial strategy, hardware hierarchy for large properties, UI delivery, and backend technology stack for the Smart Heating IoT platform.
**Decision**:
1. **Server Role (ThinkPad L590)**: Operates as a Hybrid Server hosting personal private services (TeslaMate, Aquarium, Home Assistant) alongside the MVP Smart Heating platform on mysmarthome.cc with zero cloud operating costs during development.
2. **Autonomous Edge & Optional Home Assistant**: The ESP32 heating controller remains 100% offline-first and self-sufficient. Home Assistant is strictly an optional MQTT integration (heating never fails if HA reboots).
3. **Master-Slave Multi-Manifold Architecture**: In multi-story houses with multiple hydraulic manifolds, each manifold receives its own dedicated ESP32 controller. One controller is designated Master (firing the boiler via 433 MHz RF trigger); other controllers act as Slaves. Inter-controller coordination uses **ESP-NOW** (direct 2.4 GHz peer-to-peer wireless), guaranteeing boiler actuation even if the homeowner's Wi-Fi router is offline or credentials change.
4. **Multi-Property Data Hierarchy**: A single user account manages multiple properties via a switcher ([București ▼ | Sinaia]). Hierarchy: User -> 1..N Properties -> 1..M Controllers (Master/Slaves) -> 1..K Zones.
5. **Commercial Strategy (Dual Channel)**:
   - *B2C*: Complete turnkey retrofit (hardware + physical manifold installation + zero monthly subscriptions for core features, eliminating user frustration with Tado/Nabu Casa).
   - *B2B*: Strategic partnerships with heating/plumbing contractors and property developers to supply pre-assembled kits in new builds.
6. **User Interface Delivery**: Dual form factor:
   - Responsive **Progressive Web App (PWA)** installable with 1 click on iOS & Android (zero App Store friction/fees).
   - Dedicated **Wall Tablet Kiosk Mode** for wall-mounted touch displays.
7. **Multi-Tenant Server Backend**: Selected **PocketBase** (Go + SQLite embedded) for the backend microservice on pp.mysmarthome.cc. Consumes only ~15 MB RAM, includes out-of-the-box Auth (OAuth2 Google/Apple/Email PIN), real-time WebSockets, and built-in Admin UI.
**Consequences**: Crystal-clear architectural alignment; high hardware resilience through ESP-NOW; negligible server footprint; fast time-to-market without App Store delays or recurring cloud bills.
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
