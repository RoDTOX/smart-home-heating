# Raport Testare Automată Sistem & Matrice Trasabilitate (ASPICE SWE.5)

> **Dispozitiv Țintă:** ESP32-WROOM-32E (`http://192.168.1.20:80`)  
> **Dată Execuție:** 18.10.2026 01:34:25  
> **Rată de Trecere Teste:** **100.0%** (85/85 Teste Atomice Trecute)  
> **Acoperire Cerințe (Coverage):** **100.0%** (44/44 Cerințe Acoperite)  
> **Cerințe Neacoperite:** **0** *(Niciuna - Acoperire 100% Totală)*  
> **Verdict Final:** **✅ ACCEPTAT (READY FOR PRODUCTION / SWE.5 COMPLIANT)**

---

## 1. Indicatori de Trasabilitate Inginerească (Traceability KPIs)

| Metrică de Calitate | Valoare Măsurată | Țintă Standard ASPICE | Status |
| :--- | :---: | :---: | :---: |
| **Cerințe Software Definite (SWE.1)** | `44` cerințe | Înregistru complet | ✅ Confirmat |
| **Cerințe Acoperite de Teste (SWE.5)** | `44` cerințe | 100% din cerințe | ✅ **100.0% PASS** |
| **Cerințe Neacoperite de Teste** | `0` cerințe | 0 cerințe neacoperite | ✅ **0 (None)** |
| **Număr Total Teste Atomice Executate** | `85` teste atomice | >40 verificări | ✅ `85` Teste |
| **Rată Succes Execuție Teste** | `100.0%` (85/85) | 100% PASS | ✅ **PASS** |
| **Redundanță Medie de Verificare** | `2.5` teste/cerință | $\ge 1.5$ teste/cerință | ✅ **2.5x Redundancy** |

---

## 2. Matrice de Trasabilitate Bidirecțională (Traceability Matrix: SWE.1 -> SWE.5)

Fiecare cerință funcțională și non-funcțională din specificația de sistem este mapată direct la testele atomice care o validează live pe hardware:

| Cod Cerință | Denumire Cerință de Sistem (SWE.1) | Teste Atomice SWE.5 Asociate | Nr. Teste | Stare Verificare | Verdict Acoperire |
| :---: | :--- | :--- | :---: | :---: | :---: |
| **FR-1** | Multi-Zone Hysteresis & Loop Control | `SWE.5-TC01.1`, `SWE.5-TC04.5`, `SWE.5-TC11.1`, `SWE.5-TC11.2`, `SWE.5-TC11.3`, `SWE.5-TC11.4`, `SWE.5-TC19.2`, `SWE.5-TC19.3` | 8 | ✅ PASS | 🟢 COVERED |
| **FR-2** | Actuator Opening Interlock Delay (120s) | `SWE.5-TC07.2` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-3** | Pump Overrun Heat Dissipation (180s) | `SWE.5-TC07.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-4** | Anti-Short-Cycling Boiler Guard (60s) | `SWE.5-TC09.1` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-5** | 433.92 MHz Radio Boiler Flame Command | `SWE.5-TC04.6`, `SWE.5-TC08.1` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-6** | Xiaomi BLE Thermometer Ingestion via NimBLE | `SWE.5-TC03.1`, `SWE.5-TC03.2`, `SWE.5-TC04.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-7** | 2-Way Network Virtual Console (Telnet & Web) | `SWE.5-TC13.1`, `SWE.5-TC13.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-8** | Headless REST / JSON API & Web Dashboard | `SWE.5-TC01.1`, `SWE.5-TC19.1`, `SWE.5-TC19.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-9** | Outdoor Weather & Climate Compensation | `SWE.5-TC12.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-10** | Diagnostic Engine, DTC Trouble Codes & Self-Test | `SWE.5-TC04.1`, `SWE.5-TC04.2`, `SWE.5-TC04.3`, `SWE.5-TC04.4`, `SWE.5-TC04.5`, `SWE.5-TC04.6`, `SWE.5-TC04.7`, `SWE.5-TC10.2`, `SWE.5-TC10.3` | 9 | ✅ PASS | 🟢 COVERED |
| **FR-11** | Dynamic Sensor Mapping & Setup Wizard | `SWE.5-TC02.4`, `SWE.5-TC03.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-12** | NTP Time Synchronization & Bucharest POSIX Clock | `SWE.5-TC12.1`, `SWE.5-TC12.2`, `SWE.5-TC12.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-13** | Device Commissioning Passport & NVS Identity | `SWE.5-TC02.1`, `SWE.5-TC02.2`, `SWE.5-TC02.3`, `SWE.5-TC02.4` | 4 | ✅ PASS | 🟢 COVERED |
| **FR-14** | Hydraulic GPA Secondary Pump Controller & Relay 4 | `SWE.5-TC04.4`, `SWE.5-TC07.1`, `SWE.5-TC07.2`, `SWE.5-TC07.3`, `SWE.5-TC07.5` | 5 | ✅ PASS | 🟢 COVERED |
| **FR-15** | Universal Actuator Polarity Support (NC vs NO) | `SWE.5-TC06.1`, `SWE.5-TC06.2`, `SWE.5-TC06.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-16** | Emergency Frost Guard (<12°C) & DTC-101 Isolation | `SWE.5-TC07.4` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-17** | Boiler Anti-Spam, Idempotency & HTTP 429 Guard | `SWE.5-TC08.2`, `SWE.5-TC08.3`, `SWE.5-TC09.1`, `SWE.5-TC09.2`, `SWE.5-TC10.1`, `SWE.5-TC10.2`, `SWE.5-TC10.3` | 7 | ✅ PASS | 🟢 COVERED |
| **FR-18** | Configurable Sensor Timeout Window (60-7200s) | `SWE.5-TC14.1`, `SWE.5-TC14.2`, `SWE.5-TC14.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-19** | Hydronic Calendar Anti-Seize on UNIX Epoch | `SWE.5-TC07.5` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-20** | BLE Radio Link Quality Categorization | `SWE.5-TC03.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-21** | Firmware Baseline SemVer, Build & Reset Reason | `SWE.5-TC15.1`, `SWE.5-TC15.2`, `SWE.5-TC15.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-22** | Sensor Boot Warmup Grace Period & Selective Activation | `SWE.5-TC03.4` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-23** | Tabular Formatting & Status Clean Alignment | `SWE.5-TC13.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-24** | Hardware Telemetry (CPU Die Temp, RAM Lowest/MaxBlock, NVM) | `SWE.5-TC01.2`, `SWE.5-TC01.4` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-25** | Integrated Zone Telemetry Cards & UTF-8 °C | `SWE.5-TC13.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-26** | Web Console Exhaustive Status Highlighting & Coloring | `SWE.5-TC13.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-27** | System Block Structure & utf8VisibleLen Alignment | `SWE.5-TC13.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-28** | High Sensitivity NimBLE (90% Duty Cycle) & Non-Blocking Warmup | `SWE.5-TC03.1`, `SWE.5-TC03.4` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-29** | Circadian Bio-Schedule & Intelligent Thermal Guards | `SWE.5-TC16.1`, `SWE.5-TC16.2`, `SWE.5-TC16.3`, `SWE.5-TC16.4`, `SWE.5-TC19.1` | 5 | ✅ PASS | 🟢 COVERED |
| **FR-30** | Dynamic Anchor Tuning & NVS Binary Persistence | `SWE.5-TC17.1`, `SWE.5-TC17.2`, `SWE.5-TC17.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-31** | Bio-Schedule Presets Engine & User Custom Slots | `SWE.5-TC18.1`, `SWE.5-TC18.2`, `SWE.5-TC18.3`, `SWE.5-TC18.4`, `SWE.5-TC19.4`, `SWE.5-TC19.5` | 6 | ✅ PASS | 🟢 COVERED |
| **FR-32** | Boiler Flame Re-Arming & Deferred Action Cancellation | `SWE.5-TC08.1`, `SWE.5-TC08.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-33** | Temperature Trend Telemetry & Sparkline Vector Dynamics | `SWE.5-TC19.6` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-34** | LittleFS Flash Telemetry History Buffer (24h @ 5m & 5d @ 30m, /api/history) | `SWE.5-TC20.1`, `SWE.5-TC20.2`, `SWE.5-TC20.3` | 3 | ✅ PASS | 🟢 COVERED |
| **FR-35** | Thermal Gas Consumption Model & Runtime Telemetry (0.40m³ ACM, 0.60m³/h flow) | `SWE.5-TC21.1`, `SWE.5-TC21.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-36** | Home Assistant Full REST Package & Modern Template Integration | `SWE.5-TC22.1`, `SWE.5-TC22.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-37** | FSM Actuator Hardware Safety Invariant & Instant Warmup Abort | `SWE.5-TC23.1`, `SWE.5-TC23.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-38** | Intelligent Overrun & Conditional Post-Circulation Heat Dissipation | `SWE.5-TC24.1` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-39** | Temporary Setpoint Hold & Circadian Bio-Schedule Non-Destructive Override | `SWE.5-TC25.1`, `SWE.5-TC25.2` | 2 | ✅ PASS | 🟢 COVERED |
| **FR-40** | Open Window Detection Suite & Smart Pause Guard | `SWE.5-TC26.1`, `SWE.5-TC26.2`, `SWE.5-TC26.3`, `SWE.5-TC26.4` | 4 | ✅ PASS | 🟢 COVERED |
| **NFR-1.1** | Autonomous Edge Operation (Zero Cloud Dependency) | `SWE.5-TC01.1`, `SWE.5-TC04.1` | 2 | ✅ PASS | 🟢 COVERED |
| **NFR-1.5** | Hardware Task Watchdog Timer (TWDT 90s) | `SWE.5-TC05.1` | 1 | ✅ PASS | 🟢 COVERED |
| **NFR-2.2** | RAM Efficiency (Free Heap >100 KB) | `SWE.5-TC01.2`, `SWE.5-TC04.2` | 2 | ✅ PASS | 🟢 COVERED |
| **NFR-3.2** | Zero Bloatware & Decoupled Architecture | `SWE.5-TC01.3` | 1 | ✅ PASS | 🟢 COVERED |

---

## 3. Registru Detaliat Rezultate Teste Atomice (SWE.5 Verification Results)

| ID Test | Nume Verificare Atomică | Cerințe Validate | Latență | Stare | Detalii & Aserțiuni Verificate |
| :---: | :--- | :--- | :---: | :---: | :--- |
| **SWE.5-TC01.1** | System Status Payload Schema | `FR-1, FR-8, NFR-1.1` | 272.0 ms | ✅ PASS | Boiler: OFF, Demand: False, AutoMode: True |
| **SWE.5-TC01.2** | RAM Margin & Free Heap Budget (>100 KB) | `FR-24, NFR-2.2` | 710.9 ms | ✅ PASS | Free Heap: 101 KB free (Target >100 KB PASS) |
| **SWE.5-TC01.3** | Decoupled Architecture: Zero Bloatware | `NFR-3.2` | 430.1 ms | ✅ PASS | Verified zero MQTT fields in REST payload; fully decoupled autonomous edge node |
| **SWE.5-TC01.4** | Wi-Fi RSSI & Hardware Telemetry | `FR-24` | 259.6 ms | ✅ PASS | Wi-Fi RSSI: -49 dBm, Health Score: 100% |
| **SWE.5-TC02.1** | Device Passport Serial Prefix | `FR-13` | 213.7 ms | ✅ PASS | Device Serial: SH-14721A0D5020 |
| **SWE.5-TC02.2** | Device Passport MAC Address Format | `FR-13` | 229.4 ms | ✅ PASS | Hardware MAC: 20:50:0D:1A:72:14 |
| **SWE.5-TC02.3** | Commissioning Status & NVS Date | `FR-13` | 241.0 ms | ✅ PASS | Commissioned: 2026-10-09 10:15:15, Installer Notes Available: False |
| **SWE.5-TC02.4** | Setup Passport Metadata Contract | `FR-11, FR-13` | 421.0 ms | ✅ PASS | Customer: 'Seceleanu Bogdan', City: 'București (București)', Age: 8 days |
| **SWE.5-TC03.1** | NimBLE Scanner Running Status | `FR-6, FR-28` | 200.5 ms | ✅ PASS | NimBLE Radio Scanner: ACTIVE (90% duty cycle) |
| **SWE.5-TC03.2** | Xiaomi BTHome v2 Direct Ingestion | `FR-6, FR-11` | 310.4 ms | ✅ PASS | Directly decoded 3 Xiaomi LYWSD03MMC sensors via onboard Bluetooth |
| **SWE.5-TC03.3** | BLE Link Quality Badges | `FR-20` | 374.2 ms | ✅ PASS | Categorized 3 Xiaomi sensors into link quality badges (EXCELLENT, GOOD, MARGINAL, POOR) |
| **SWE.5-TC03.4** | Boot Warmup Grace Period Contract | `FR-22, FR-28` | 295.8 ms | ✅ PASS | Uptime: 290s (Boot warmup grace period verified passed and operational) |
| **SWE.5-TC04.1** | Self-Test Wi-Fi Subsystem | `FR-10, NFR-1.1` | 319.8 ms | ✅ PASS | Wi-Fi Subsystem: PASS verified over live selftest |
| **SWE.5-TC04.2** | Self-Test System Memory (Heap) | `FR-10, NFR-2.2` | 176.3 ms | ✅ PASS | System Memory (Heap): PASS verified (>60 KB threshold) |
| **SWE.5-TC04.3** | Self-Test Bluetooth BLE Stack | `FR-6, FR-10` | 201.6 ms | ✅ PASS | Bluetooth BLE Stack: PASS (Scanner ACTIVE) |
| **SWE.5-TC04.4** | Self-Test Hydraulic Pump Subsystem | `FR-10, FR-14` | 245.6 ms | ✅ PASS | Hydraulic Pump Ctrl: PASS (Relay 4 GPA Controller) |
| **SWE.5-TC04.5** | Self-Test Multi-Zone Heating Engine | `FR-1, FR-10` | 223.1 ms | ✅ PASS | Multi-Zone Engine: PASS (Automatic Hysteresis Loops) |
| **SWE.5-TC04.6** | Self-Test Boiler RF 433 MHz Trigger | `FR-5, FR-10` | 285.6 ms | ✅ PASS | Boiler RF 433 MHz: PASS (OOK 409-step Transmitter) |
| **SWE.5-TC04.7** | Self-Test System Health Score | `FR-10` | 157.3 ms | ✅ PASS | Overall Health Score: 100% [EXCELLENT] (All subsystems healthy) |
| **SWE.5-TC05.1** | Hardware Task Watchdog (TWDT 90s) | `NFR-1.5` | 212.0 ms | ✅ PASS | TWDT 90s Active, Loop fed normally, Reset reason: Software Reboot (Firmware Update / Restart) |
| **SWE.5-TC06.1** | Actuator Polarity Initial Query | `FR-15` | 339.2 ms | ✅ PASS | Initial Actuator Polarity: NC |
| **SWE.5-TC06.2** | Actuator Polarity Dynamic Toggle | `FR-15` | 552.0 ms | ✅ PASS | Successfully toggled NC -> NO |
| **SWE.5-TC06.3** | Actuator Polarity NVS Restoration | `FR-15` | 427.9 ms | ✅ PASS | Restored to Normally Closed (NC) in NVS |
| **SWE.5-TC07.1** | Hydraulic GPA Pump Configuration | `FR-14` | 220.8 ms | ✅ PASS | GPA Pump Enabled: True, Active: False (Relay 4) |
| **SWE.5-TC07.2** | Actuator Interlock Delay Contract (120s) | `FR-2, FR-14` | 230.2 ms | ✅ PASS | Verified 120s interlock delay rule active across 3 hydronic loops |
| **SWE.5-TC07.3** | Pump Overrun Heat Dissipation (180s) | `FR-3, FR-14` | 218.5 ms | ✅ PASS | Verified 180s overrun rule; keeper zone state: False |
| **SWE.5-TC07.4** | Emergency Frost Guard Standby (<12°C) | `FR-16` | 223.6 ms | ✅ PASS | Frost Guard Active: False (Trigger threshold: <12.0°C) |
| **SWE.5-TC07.5** | Hydronic Anti-Seize UNIX Epoch Contract | `FR-14, FR-19` | 233.1 ms | ✅ PASS | Anti-Seize Last Epoch: 1791958044 (Weekly 20s summer exercise schedule) |
| **SWE.5-TC08.1** | Boiler State Baseline Establishment | `FR-5, FR-32` | 917.8 ms | ✅ PASS | Boiler baseline set to OFF (RF transmitter verified) |
| **SWE.5-TC08.2** | Anti-Spam Idempotency No-Op Filter | `FR-17, FR-32` | 266.5 ms | ✅ PASS | Repeated command handled as 0ms no-op; RF burst skipped |
| **SWE.5-TC08.3** | Zero RF Burst Emission Verification | `FR-17` | 223.7 ms | ✅ PASS | RF transmission hardware driver bypassed without CPU blocking |
| **SWE.5-TC09.1** | Boiler 60s Reversal Interlock Guard | `FR-4, FR-17` | 423.6 ms | ✅ PASS | Interlock ACTIVE: Rejected state reversal (wait: 2s remaining) |
| **SWE.5-TC09.2** | Deterministic HTTP 429 & waitSec | `FR-17` | 315.5 ms | ✅ PASS | HTTP 429 verified: Anti-short-cycling guard active (3s remaining) |
| **SWE.5-TC10.1** | Rapid Boiler Spam Injection Loop | `FR-17` | 1350.9 ms | ✅ PASS | Fired 3 rapid requests to trigger DTC-201; immediately cancelled pending startup |
| **SWE.5-TC10.2** | DTC-201 Logging & Health Degradation | `FR-10, FR-17` | 147.1 ms | ✅ PASS | DTC-201 raised: WARN_BOILER_SPAM_DETECTED, Health Score degraded to 90% |
| **SWE.5-TC10.3** | DTC Clear & System Health Recovery | `FR-10, FR-17` | 1439.6 ms | ✅ PASS | DTCs cleared; System Health Score 100% recovered |
| **SWE.5-TC11.1** | Zone Setpoint Initial Query | `FR-1` | 256.8 ms | ✅ PASS | Zone 1 'Living & Bucatarie': Target 21.0°C, Current 23.7°C |
| **SWE.5-TC11.2** | Zone Setpoint Dynamic Mutation | `FR-1` | 714.5 ms | ✅ PASS | Zone 1 setpoint changed: 21.0°C -> 21.5°C |
| **SWE.5-TC11.3** | Zone Setpoint NVS Restoration | `FR-1` | 443.1 ms | ✅ PASS | Zone 1 setpoint restored to 22.0°C in NVS |
| **SWE.5-TC11.4** | Operating Mode Contract (Auto vs Manual) | `FR-1` | 237.7 ms | ✅ PASS | Active Operating Mode: AUTOMATIC |
| **SWE.5-TC12.1** | NTP Clock Sync & Bucharest POSIX | `FR-12` | 187.0 ms | ✅ PASS | NTP Synced: 2026-10-18 01:33:58 (Europe/Bucharest) |
| **SWE.5-TC12.2** | Heating Season Calendar Detection | `FR-12` | 322.3 ms | ✅ PASS | Season: Autumn, Heating Season Active: True |
| **SWE.5-TC12.3** | Outdoor Weather Telemetry (Open-Meteo) | `FR-9, FR-12` | 158.9 ms | ✅ PASS | Outdoor Weather: 8.7°C (Open-Meteo REST service) |
| **SWE.5-TC13.1** | Console Remote HELP Execution | `FR-7` | 413.2 ms | ✅ PASS | Dispatched HELP command over REST console endpoint |
| **SWE.5-TC13.2** | Console Log Buffer Tail Verification | `FR-7` | 334.7 ms | ✅ PASS | Verified live ring buffer tail contains structured command response |
| **SWE.5-TC13.3** | Tabular Formatting & UTF-8 °C Presentation | `FR-23, FR-25, FR-26, FR-27` | 1861.2 ms | ✅ PASS | Verified 2-column tabular blocks, UTF-8 °C symbols and zero column drifting (buffer cleaned) |
| **SWE.5-TC14.1** | Sensor Timeout Failsafe Query | `FR-18` | 152.5 ms | ✅ PASS | Configured sensor timeout: 600s |
| **SWE.5-TC14.2** | Sensor Timeout Mutation to 750s | `FR-18` | 289.5 ms | ✅ PASS | Updated timeout to 750s in NVS |
| **SWE.5-TC14.3** | Sensor Timeout Restoration | `FR-18` | 339.5 ms | ✅ PASS | Restored default timeout to 600s (10 min) in NVS |
| **SWE.5-TC15.1** | Firmware SemVer Baseline (1.2.0) | `FR-21` | 166.9 ms | ✅ PASS | Firmware SemVer: v1.2.0 |
| **SWE.5-TC15.2** | Build Timestamp & Git Hash Traceability | `FR-21` | 191.3 ms | ✅ PASS | Build: Oct 18 2026 01:28:28, Git Commit: 8065534 |
| **SWE.5-TC15.3** | Hardware Reset Reason Telemetry | `FR-21` | 224.7 ms | ✅ PASS | Last Hardware Reset Reason: Software Reboot (Firmware Update / Restart) |
| **SWE.5-TC16.1** | Bio-Schedule 4 Life Anchors Contract | `FR-29` | 256.8 ms | ✅ PASS | Verified 4 natural anchors (Sleep, Wake, Away, Home) across Weekday & Weekend profiles |
| **SWE.5-TC16.2** | Predictive Pre-Heat Thermal Advance | `FR-29` | 182.6 ms | ✅ PASS | Pre-Heat Advance Enabled: True (Dynamic thermal inertia calculation) |
| **SWE.5-TC16.3** | Open Window Smart Pause Guard | `FR-29` | 275.0 ms | ✅ PASS | Open Window Guard Enabled: True (>1.2°C / 5 min detection) |
| **SWE.5-TC16.4** | Bio-Schedule Telemetry Integration | `FR-29` | 422.8 ms | ✅ PASS | Active Anchor: 'SLEEP', Effective Target Exposed per Zone |
| **SWE.5-TC17.1** | Dynamic Anchor Tuning Execution | `FR-30` | 406.3 ms | ✅ PASS | Updated Weekday Wake anchor to 06:45 with targets [22.1, 22.6, 21.9]°C |
| **SWE.5-TC17.2** | Anchor Tuning NVS Persistence | `FR-30` | 253.9 ms | ✅ PASS | NVS binary structure matches updated parameters |
| **SWE.5-TC17.3** | Bio-Schedule Factory Reset Restoration | `FR-30` | 403.1 ms | ✅ PASS | Schedule restored to factory circadian defaults |
| **SWE.5-TC18.1** | Presets Catalog Discovery Contract | `FR-31` | 321.5 ms | ✅ PASS | Catalog verified: 7 slots discovered (default, eco, wfh, custom1, custom2) |
| **SWE.5-TC18.2** | Preset Activation ('eco') | `FR-31` | 230.6 ms | ✅ PASS | Applied 'eco' preset (Sleep targets: [18.0, 18.5, 18.0]°C) |
| **SWE.5-TC18.3** | Custom User Slot Save ('custom4') | `FR-31` | 424.8 ms | ✅ PASS | Saved active schedule into NVS slot 'custom4' as 'SWE5Test' & cleanly reset |
| **SWE.5-TC18.4** | Default Preset Restoration ('default') | `FR-31` | 319.2 ms | ✅ PASS | Restored factory 'default' preset |
| **SWE.5-TC19.1** | Invalid Anchor Rejection (HTTP 400) | `FR-8, FR-29` | 225.5 ms | ✅ PASS | HTTP 400 rejected invalid anchor name: Invalid anchor. Use sleep, wake, away, or home. |
| **SWE.5-TC19.2** | Out-of-Bounds Temperature Clamping | `FR-1` | 1968.8 ms | ✅ PASS | Verified firmware hard-clamping to physical safety limits [15.0°C, 30.0°C] (relays isolated) |
| **SWE.5-TC19.3** | Invalid Zone Index Handling (HTTP 400) | `FR-1, FR-8` | 335.9 ms | ✅ PASS | HTTP 400 safely rejected zone index 99: Invalid zone index |
| **SWE.5-TC19.4** | Invalid Preset Application Rejection | `FR-31` | 215.8 ms | ✅ PASS | HTTP 400 rejected non-existent preset: Preset not found or invalid. |
| **SWE.5-TC19.5** | Invalid Preset Slot Rejection | `FR-31` | 231.8 ms | ✅ PASS | HTTP 400 safely rejected invalid slot: Invalid preset slot id. Valid slots: default, eco, wfh, custom1..custom4. |
| **SWE.5-TC19.6** | Temperature Trend & Sparkline Dynamics | `FR-33` | 327.2 ms | ✅ PASS | Verified live telemetry for 3 zones feeding 3h SVG sparkline dynamics |
| **SWE.5-TC20.1** | LittleFS 24h History Buffer Schema | `FR-34` | 273.8 ms | ✅ PASS | 24h history verified: 4 points acquired from LittleFS ring buffer (stepSec=300s) |
| **SWE.5-TC20.2** | LittleFS 5d History Buffer Schema | `FR-34` | 190.2 ms | ✅ PASS | 5d history verified: 2 points acquired from LittleFS 30m downsampled ring buffer (stepSec=1800s) |
| **SWE.5-TC20.3** | LittleFS Binary Packing & Resolution | `FR-34` | 312.1 ms | ✅ PASS | LittleFS 10-byte binary record packing verified (288 pts 24h @ 5m, 240 pts 5d @ 30m) |
| **SWE.5-TC21.1** | Boiler Runtime Schema for Gas Model | `FR-35` | 440.7 ms | ✅ PASS | Boiler runtime schema validated (Today: 778s, Month: 1285s) |
| **SWE.5-TC21.2** | Calibrated Gas Model Calculations | `FR-35` | 422.7 ms | ✅ PASS | Gas model: Today 0.53 m³ (0.13 m³ heat + 0.4 m³ ACM) = 1.75 Lei |
| **SWE.5-TC22.1** | HA Package Syntax & Modern Template | `FR-36` | 62.0 ms | ✅ PASS | Verified valid YAML in homeassistant/packages/smart_heating.yaml using modern template: syntax (zero deprecation warnings) |
| **SWE.5-TC22.2** | HA Dashboard Structure & Cards | `FR-36` | 106.1 ms | ✅ PASS | Verified Lovelace dashboard structure with 3 views and conditional warning cards |
| **SWE.5-TC23.1** | FSM Instant Warmup Abort Contract | `FR-37` | 365.3 ms | ✅ PASS | Verified FSM instant warmup abort & immediate relay cut-off (0V) when demand drops |
| **SWE.5-TC23.2** | Orphaned Relay Safety Invariant | `FR-37` | 203.4 ms | ✅ PASS | Verified 0 orphaned relays across all 3 hydronic loops (firmware forces 0V) |
| **SWE.5-TC24.1** | Intelligent Overrun Flame Condition | `FR-38` | 553.1 ms | ✅ PASS | Intelligent Overrun active: boiler=OFF, pending=NONE, pumpActive=False |
| **SWE.5-TC25.1** | Temporary Setpoint Hold Override | `FR-39` | 1068.6 ms | ✅ PASS | Verified temporary hold flag & non-destructive circadian anchor preservation (restored 21.0°C) |
| **SWE.5-TC25.2** | Temporary Setpoint Hold Clear API | `FR-39` | 453.9 ms | ✅ PASS | Verified /api/zones/hold/clear endpoint for single zone and 'all' broadcast |
| **SWE.5-TC26.1** | Open Window Telemetry Schema | `FR-40` | 208.1 ms | ✅ PASS | Verified 5 open window telemetry fields present across all 3 zones |
| **SWE.5-TC26.2** | Open Window Threshold Mutation & NVS | `FR-40` | 1738.8 ms | ✅ PASS | Verified threshold mutation (1.5°C) & NVS restore (1.2°C) via /api/zones/window/threshold |
| **SWE.5-TC26.3** | Open Window Manual Clear API | `FR-40` | 458.2 ms | ✅ PASS | Verified /api/zones/window/clear endpoint with status 'ok'/'noop' response contract |
| **SWE.5-TC26.4** | Open Window Safety Cutoff Contract | `FR-40` | 744.7 ms | ✅ PASS | Thermodynamic safety contract validated (0.8-2.0°C strict bounds, HTTP 400 out-of-bounds rejection, 30m timeout, 0V cutoff) |

---

## 4. Concluzii Inginerești & Siguranță

1. **Atomicitate & Izolare 100%:** Fiecare aserțiune critică (heap, polaritate, timpi de interblocare, idempotență, persistență NVS, ancore și sloturi de preset, LittleFS, model gaz, Home Assistant, overrun și detectare geam deschis) este executată ca test atomic independent.
2. **Trasabilitate Fără Goluri (Zero Uncovered Requirements):** Toate cele 44 de cerințe funcționale și non-funcționale au cel puțin o verificare dedicată pe hardware live; 0 cerințe rămase neacoperite.
3. **Zero Blocaj / Zero Stalls:** Toate apelurile HTTP utilizează conexiuni fără socket exhaustion, obținând o durată de execuție optimă pentru întreaga suită de 85 de verificări atomice.
4. **Protecție Flash Wear Confirmată:** Scrierile NVS pentru ancore, timeout, praguri geam deschis și preset-uri sunt supuse verificării de persistență și restaurare la valorile inițiale.
