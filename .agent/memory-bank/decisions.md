# Decisions Log: Smart Heating System

<!-- Append-only Architecture Decision Record (ADR) log. -->

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



