# Decisions Log: Smart Heating System

<!-- Append-only Architecture Decision Record (ADR) log. -->

## 2026-10-18 — Dedicated Home Server (ThinkPad L590) & Cloudflare Named Tunnel on mysmarthome.cc
**Context**: External mobile access without VPN/Tailscale client apps was required for Home Assistant, Smart Heating Odaia Clasic, and Galaxy-A6 services. Furthermore, running long-term server daemons (Home Assistant, PostgreSQL, TeslaMate, Nginx, Mosquitto) on a mobile phone (Galaxy-A6) and WSL laptop introduces battery swelling risks, resource limits, and WSL port bindings.
**Decision**:
- Acquired custom domain `mysmarthome.cc` managed via Cloudflare authoritative nameservers (`devin` / `elisabeth.ns.cloudflare.com`).
- Deployed a permanent outbound Cloudflare Tunnel (`cloudflared.service` via QUIC) eliminating port forwarding, dynamic DNS issues, and public IP vulnerability.
- Enforced edge Basic Authentication (`seceleanu:bogdan`) in front of the local Nginx proxy (`127.0.0.1:8088`).
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
## 2026-10-28 — ADR-28: Decoupled LittleFS Web UI & 1.25 MB Firmware Partition Budget
**Context**: Embedding large gzipped HTML/CSS/JS into PROGMEM caused irmware.bin to exceed the 1,310,720 byte (1.25 MB) OTA application slot on the ESP32.
**Decision**:
- Serve web assets directly from the 1.375 MB LittleFS partition (/index.html.gz).
- Pin build platform to espressif32@6.5.0 to avoid mbedtls/SSL bloat from Arduino Core 3.x.
- Keep firmware binary compact (~1.23 MB) with >= 15 KB headroom.

## 2026-10-28 — ADR-29: ASPICE 4.0 Capability Level 1 Documentation Architecture
**Context**: Excessive AI chat summaries and unstructured markdown created technical debt. Need automotive-grade rigor.
**Decision**:
- Establish canonical process-prefixed directories: docs/sys/ (SYS.2-SYS.5), docs/swe/ (SWE.1-SWE.5), docs/sup/ (SUP.1, SUP.8), docs/biz/ (BIZ.1).
- Implement unified bidirectional tagging: [SYS-REQ-xxx], [SWE-REQ-xxx], [TC-SYS-xxx], [TC-SWE-xxx].
- Establish cryptographic baseline ledger (sup8_baseline_BL_2026_v1.10.28.yaml).

## 2026-10-28 — ADR-30: 2R2C State-Space Thermodynamic Engine & Autonomous Calibration
**Context**: Floor heating suffers from 30–90 min thermal dead time and overshoot.
**Decision**:
- Implement discrete state-space lumped parameter 2R2C model (, R_s, C_a, R_e$).
- Autonomous passive night-time cooling calibration (01:00–05:00) using Open-Meteo external temperature.
- Autonomous Away-mode step-response identification for dead time, slope, and overshoot.
- Predictive preheat lead-time calculation with proportional overshoot compensation.

## 2026-10-28 — ADR-31: Central-Difference Derivative Rapid Window Detection
**Context**: Static temperature drop thresholds fail in mild weather (15°C) and trigger false positives on sensor noise.
**Decision**:
- Implement rolling 12-sample circular FIFO buffer (60m history at 5m intervals).
- Compute central-difference 1st derivative (/dt$) and 2nd derivative acceleration (^2T/dt^2$).
- Trigger threshold: $|dT/dt| > 2.5 	imes 	ext{Rate}_{natural}(T_{ext})$. Auto-recover on /dt \ge 0$.

## 2026-10-28 — ADR-32: Custom Turnkey ESP32-S3 8-Zone PCBA & External Antenna
**Context**: Commercial production requires silent operation, OpenTherm modulation, and metal cabinet signal bypass.
**Decision**:
- Custom ESP32-S3-WROOM-1-N8R2 mainboard with 8 zero-cross SSRs (MOC3043M + BTA16-600B) for 0dB silent switching.
- Isolated OpenTherm Master circuit (EN 50559) for dynamic water temperature modulation (35°C–45°C).
- External bulkhead SMA antenna bracket (+32 dB signal boost outside metal manifold cabinet).
- Turnkey unit cost: .80 USD at MOQ 1000 (<  USD target).
