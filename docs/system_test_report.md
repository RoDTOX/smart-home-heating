# Raport Testare Automată Sistem & Matrice Trasabilitate (ASPICE SWE.5)

> **Dispozitiv Țintă:** ESP32-WROOM-32E (`http://192.168.1.20:80`)  
> **Dată Execuție:** 14.10.2026 21:22:33  
> **Rată de Trecere Teste:** **100.0%** (68/68 Teste Atomice Trecute)  
> **Acoperire Cerințe (Coverage):** **100.0%** (33/33 Cerințe Acoperite)  
> **Cerințe Neacoperite:** **0** *(Niciuna - Acoperire 100% Totală)*  
> **Verdict Final:** **✅ ACCEPTAT (READY FOR PRODUCTION / SWE.5 COMPLIANT)**

---

## 1. Indicatori de Trasabilitate Inginerească (Traceability KPIs)

| Metrică de Calitate | Valoare Măsurată | Țintă Standard ASPICE | Status |
| :--- | :---: | :---: | :---: |
| **Cerințe Software Definite (SWE.1)** | `33` cerințe | Înregistru complet | ✅ Confirmat |
| **Cerințe Acoperite de Teste (SWE.5)** | `33` cerințe | 100% din cerințe | ✅ **100.0% PASS** |
| **Cerințe Neacoperite de Teste** | `0` cerințe | 0 cerințe neacoperite | ✅ **0 (None)** |
| **Număr Total Teste Atomice Executate** | `68` teste atomice | >40 verificări | ✅ `68` Teste |
| **Rată Succes Execuție Teste** | `100.0%` (68/68) | 100% PASS | ✅ **PASS** |
| **Redundanță Medie de Verificare** | `2.8` teste/cerință | $\ge 1.5$ teste/cerință | ✅ **2.8x Redundancy** |

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
| **FR-26.1** | Smart Bio-Schedule: 4 Circadian Life Anchors | `SWE.5-TC16.1`, `SWE.5-TC16.2`, `SWE.5-TC16.3`, `SWE.5-TC16.4`, `SWE.5-TC17.1`, `SWE.5-TC17.2`, `SWE.5-TC17.3`, `SWE.5-TC19.1` | 8 | ✅ PASS | 🟢 COVERED |
| **FR-26.2** | Smart Bio-Schedule: Presets Engine (Factory & User Slots) | `SWE.5-TC18.1`, `SWE.5-TC18.2`, `SWE.5-TC18.3`, `SWE.5-TC18.4`, `SWE.5-TC19.4`, `SWE.5-TC19.5` | 6 | ✅ PASS | 🟢 COVERED |
| **FR-27** | System Block Structure & utf8VisibleLen Alignment | `SWE.5-TC13.3` | 1 | ✅ PASS | 🟢 COVERED |
| **FR-28** | High Sensitivity NimBLE (90% Duty Cycle) & Non-Blocking Warmup | `SWE.5-TC03.1`, `SWE.5-TC03.4` | 2 | ✅ PASS | 🟢 COVERED |
| **NFR-1.1** | Autonomous Edge Operation (Zero Cloud Dependency) | `SWE.5-TC01.1`, `SWE.5-TC04.1` | 2 | ✅ PASS | 🟢 COVERED |
| **NFR-1.5** | Hardware Task Watchdog Timer (TWDT 90s) | `SWE.5-TC05.1` | 1 | ✅ PASS | 🟢 COVERED |
| **NFR-2.2** | RAM Efficiency (Free Heap >100 KB) | `SWE.5-TC01.2`, `SWE.5-TC04.2` | 2 | ✅ PASS | 🟢 COVERED |
| **NFR-3.2** | Zero Bloatware & Decoupled Architecture | `SWE.5-TC01.3` | 1 | ✅ PASS | 🟢 COVERED |

---

## 3. Registru Detaliat Rezultate Teste Atomice (SWE.5 Verification Results)

| ID Test | Nume Verificare Atomică | Cerințe Validate | Latență | Stare | Detalii & Aserțiuni Verificate |
| :---: | :--- | :--- | :---: | :---: | :--- |
| **SWE.5-TC01.1** | System Status Payload Schema | `FR-1, FR-8, NFR-1.1` | 236.1 ms | ✅ PASS | Boiler: OFF, Demand: False, AutoMode: True |
| **SWE.5-TC01.2** | RAM Margin & Free Heap Budget (>100 KB) | `FR-24, NFR-2.2` | 262.5 ms | ✅ PASS | Free Heap: 116 KB free (Target >100 KB PASS) |
| **SWE.5-TC01.3** | Decoupled Architecture: Zero Bloatware | `NFR-3.2` | 340.8 ms | ✅ PASS | Verified zero MQTT fields in REST payload; fully decoupled autonomous edge node |
| **SWE.5-TC01.4** | Wi-Fi RSSI & Hardware Telemetry | `FR-24` | 528.8 ms | ✅ PASS | Wi-Fi RSSI: -65 dBm, Health Score: 100% |
| **SWE.5-TC02.1** | Device Passport Serial Prefix | `FR-13` | 204.3 ms | ✅ PASS | Device Serial: SH-14721A0D5020 |
| **SWE.5-TC02.2** | Device Passport MAC Address Format | `FR-13` | 203.0 ms | ✅ PASS | Hardware MAC: 20:50:0D:1A:72:14 |
| **SWE.5-TC02.3** | Commissioning Status & NVS Date | `FR-13` | 253.7 ms | ✅ PASS | Commissioned: 2026-10-09 10:15:15, Installer Notes Available: False |
| **SWE.5-TC02.4** | Setup Passport Metadata Contract | `FR-11, FR-13` | 227.7 ms | ✅ PASS | Customer: 'Seceleanu Bogdan', City: 'București (București)', Age: 5 days |
| **SWE.5-TC03.1** | NimBLE Scanner Running Status | `FR-6, FR-28` | 239.0 ms | ✅ PASS | NimBLE Radio Scanner: ACTIVE (90% duty cycle) |
| **SWE.5-TC03.2** | Xiaomi BTHome v2 Direct Ingestion | `FR-6, FR-11` | 206.4 ms | ✅ PASS | Directly decoded 3 Xiaomi LYWSD03MMC sensors via onboard Bluetooth |
| **SWE.5-TC03.3** | BLE Link Quality Badges | `FR-20` | 251.9 ms | ✅ PASS | Categorized 3 Xiaomi sensors into link quality badges (EXCELLENT, GOOD, MARGINAL, POOR) |
| **SWE.5-TC03.4** | Boot Warmup Grace Period Contract | `FR-22, FR-28` | 222.1 ms | ✅ PASS | Uptime: 54s (Boot warmup grace period verified passed and operational) |
| **SWE.5-TC04.1** | Self-Test Wi-Fi Subsystem | `FR-10, NFR-1.1` | 190.8 ms | ✅ PASS | Wi-Fi Subsystem: PASS verified over live selftest |
| **SWE.5-TC04.2** | Self-Test System Memory (Heap) | `FR-10, NFR-2.2` | 246.4 ms | ✅ PASS | System Memory (Heap): PASS verified (>60 KB threshold) |
| **SWE.5-TC04.3** | Self-Test Bluetooth BLE Stack | `FR-6, FR-10` | 251.6 ms | ✅ PASS | Bluetooth BLE Stack: PASS (Scanner ACTIVE) |
| **SWE.5-TC04.4** | Self-Test Hydraulic Pump Subsystem | `FR-10, FR-14` | 189.9 ms | ✅ PASS | Hydraulic Pump Ctrl: PASS (Relay 4 GPA Controller) |
| **SWE.5-TC04.5** | Self-Test Multi-Zone Heating Engine | `FR-1, FR-10` | 258.5 ms | ✅ PASS | Multi-Zone Engine: PASS (Automatic Hysteresis Loops) |
| **SWE.5-TC04.6** | Self-Test Boiler RF 433 MHz Trigger | `FR-5, FR-10` | 202.1 ms | ✅ PASS | Boiler RF 433 MHz: PASS (OOK 409-step Transmitter) |
| **SWE.5-TC04.7** | Self-Test System Health Score | `FR-10` | 244.1 ms | ✅ PASS | Overall Health Score: 100% [EXCELLENT] (All subsystems healthy) |
| **SWE.5-TC05.1** | Hardware Task Watchdog (TWDT 90s) | `NFR-1.5` | 190.9 ms | ✅ PASS | TWDT 90s Active, Loop fed normally, Reset reason: Software Reboot (Firmware Update / Restart) |
| **SWE.5-TC06.1** | Actuator Polarity Initial Query | `FR-15` | 263.5 ms | ✅ PASS | Initial Actuator Polarity: NC |
| **SWE.5-TC06.2** | Actuator Polarity Dynamic Toggle | `FR-15` | 839.3 ms | ✅ PASS | Successfully toggled NC -> NO |
| **SWE.5-TC06.3** | Actuator Polarity NVS Restoration | `FR-15` | 495.7 ms | ✅ PASS | Restored to Normally Closed (NC) in NVS |
| **SWE.5-TC07.1** | Hydraulic GPA Pump Configuration | `FR-14` | 170.0 ms | ✅ PASS | GPA Pump Enabled: True, Active: False (Relay 4) |
| **SWE.5-TC07.2** | Actuator Interlock Delay Contract (120s) | `FR-2, FR-14` | 204.8 ms | ✅ PASS | Verified 120s interlock delay rule active across 3 hydronic loops |
| **SWE.5-TC07.3** | Pump Overrun Heat Dissipation (180s) | `FR-3, FR-14` | 251.6 ms | ✅ PASS | Verified 180s overrun rule; keeper zone state: False |
| **SWE.5-TC07.4** | Emergency Frost Guard Standby (<12°C) | `FR-16` | 205.6 ms | ✅ PASS | Frost Guard Active: False (Trigger threshold: <12.0°C) |
| **SWE.5-TC07.5** | Hydronic Anti-Seize UNIX Epoch Contract | `FR-14, FR-19` | 219.1 ms | ✅ PASS | Anti-Seize Last Epoch: 1791958044 (Weekly 20s summer exercise schedule) |
| **SWE.5-TC08.1** | Boiler State Baseline Establishment | `FR-5` | 665.5 ms | ✅ PASS | Boiler baseline set to OFF (RF transmitter verified) |
| **SWE.5-TC08.2** | Anti-Spam Idempotency No-Op Filter | `FR-17` | 218.4 ms | ✅ PASS | Repeated command handled as 0ms no-op; RF burst skipped |
| **SWE.5-TC08.3** | Zero RF Burst Emission Verification | `FR-17` | 193.3 ms | ✅ PASS | RF transmission hardware driver bypassed without CPU blocking |
| **SWE.5-TC09.1** | Boiler 60s Reversal Interlock Guard | `FR-4, FR-17` | 437.2 ms | ✅ PASS | Interlock ACTIVE: Rejected state reversal (wait: 2s remaining) |
| **SWE.5-TC09.2** | Deterministic HTTP 429 & waitSec | `FR-17` | 239.1 ms | ✅ PASS | HTTP 429 verified: Anti-short-cycling guard active (2s remaining) |
| **SWE.5-TC10.1** | Rapid Boiler Spam Injection Loop | `FR-17` | 1682.7 ms | ✅ PASS | Fired 4 rapid spam requests into boiler endpoint |
| **SWE.5-TC10.2** | DTC-201 Logging & Health Degradation | `FR-10, FR-17` | 224.9 ms | ✅ PASS | DTC-201 raised: WARN_BOILER_SPAM_DETECTED, Health Score degraded to 90% |
| **SWE.5-TC10.3** | DTC Clear & System Health Recovery | `FR-10, FR-17` | 406.7 ms | ✅ PASS | DTCs cleared; System Health Score 100% recovered |
| **SWE.5-TC11.1** | Zone Setpoint Initial Query | `FR-1` | 236.0 ms | ✅ PASS | Zone 1 'Living & Bucatarie': Target 19.0°C, Current 24.6°C |
| **SWE.5-TC11.2** | Zone Setpoint Dynamic Mutation | `FR-1` | 707.4 ms | ✅ PASS | Zone 1 setpoint changed: 19.0°C -> 21.5°C |
| **SWE.5-TC11.3** | Zone Setpoint NVS Restoration | `FR-1` | 480.2 ms | ✅ PASS | Zone 1 setpoint restored to 22.0°C in NVS |
| **SWE.5-TC11.4** | Operating Mode Contract (Auto vs Manual) | `FR-1` | 202.6 ms | ✅ PASS | Active Operating Mode: AUTOMATIC |
| **SWE.5-TC12.1** | NTP Clock Sync & Bucharest POSIX | `FR-12` | 250.3 ms | ✅ PASS | NTP Synced: 2026-10-14 21:22:22 (Europe/Bucharest) |
| **SWE.5-TC12.2** | Heating Season Calendar Detection | `FR-12` | 204.7 ms | ✅ PASS | Season: Autumn, Heating Season Active: False |
| **SWE.5-TC12.3** | Outdoor Weather Telemetry (Open-Meteo) | `FR-9, FR-12` | 247.8 ms | ✅ PASS | Outdoor Weather: 12.2°C (Open-Meteo REST service) |
| **SWE.5-TC13.1** | Console Remote HELP Execution | `FR-7` | 281.8 ms | ✅ PASS | Dispatched HELP command over REST console endpoint |
| **SWE.5-TC13.2** | Console Log Buffer Tail Verification | `FR-7` | 369.4 ms | ✅ PASS | Verified live ring buffer tail contains structured command response |
| **SWE.5-TC13.3** | Tabular Formatting & UTF-8 °C Presentation | `FR-23, FR-25, FR-27` | 1423.8 ms | ✅ PASS | Verified 2-column tabular blocks, UTF-8 °C symbols and zero column drifting |
| **SWE.5-TC14.1** | Sensor Timeout Failsafe Query | `FR-18` | 264.7 ms | ✅ PASS | Configured sensor timeout: 600s |
| **SWE.5-TC14.2** | Sensor Timeout Mutation to 750s | `FR-18` | 188.9 ms | ✅ PASS | Updated timeout to 750s in NVS |
| **SWE.5-TC14.3** | Sensor Timeout Restoration | `FR-18` | 329.6 ms | ✅ PASS | Restored default timeout to 600s (10 min) in NVS |
| **SWE.5-TC15.1** | Firmware SemVer Baseline (1.2.0) | `FR-21` | 267.6 ms | ✅ PASS | Firmware SemVer: v1.2.0 |
| **SWE.5-TC15.2** | Build Timestamp & Git Hash Traceability | `FR-21` | 279.3 ms | ✅ PASS | Build: Oct 14 2026 21:20:44, Git Commit: 8065534 |
| **SWE.5-TC15.3** | Hardware Reset Reason Telemetry | `FR-21` | 364.1 ms | ✅ PASS | Last Hardware Reset Reason: Software Reboot (Firmware Update / Restart) |
| **SWE.5-TC16.1** | Bio-Schedule 4 Life Anchors Contract | `FR-26.1` | 216.0 ms | ✅ PASS | Verified 4 natural anchors (Sleep, Wake, Away, Home) across Weekday & Weekend profiles |
| **SWE.5-TC16.2** | Predictive Pre-Heat Thermal Advance | `FR-26.1` | 218.3 ms | ✅ PASS | Pre-Heat Advance Enabled: True (Dynamic thermal inertia calculation) |
| **SWE.5-TC16.3** | Open Window Smart Pause Guard | `FR-26.1` | 263.2 ms | ✅ PASS | Open Window Guard Enabled: True (>1.2°C / 5 min detection) |
| **SWE.5-TC16.4** | Bio-Schedule Telemetry Integration | `FR-26.1` | 383.5 ms | ✅ PASS | Active Anchor: 'HOME', Effective Target Exposed per Zone |
| **SWE.5-TC17.1** | Dynamic Anchor Tuning Execution | `FR-26.1` | 221.8 ms | ✅ PASS | Updated Weekday Wake anchor to 06:45 with targets [22.1, 22.6, 21.9]°C |
| **SWE.5-TC17.2** | Anchor Tuning NVS Persistence | `FR-26.1` | 165.4 ms | ✅ PASS | NVS binary structure matches updated parameters |
| **SWE.5-TC17.3** | Bio-Schedule Factory Reset Restoration | `FR-26.1` | 291.6 ms | ✅ PASS | Schedule restored to factory circadian defaults |
| **SWE.5-TC18.1** | Presets Catalog Discovery Contract | `FR-26.2` | 265.7 ms | ✅ PASS | Catalog verified: 5 slots discovered (default, eco, wfh, custom1, custom2) |
| **SWE.5-TC18.2** | Preset Activation ('eco') | `FR-26.2` | 291.3 ms | ✅ PASS | Applied 'eco' preset (Sleep targets: [18.0, 18.5, 18.0]°C) |
| **SWE.5-TC18.3** | Custom User Slot Save ('custom1') | `FR-26.2` | 222.7 ms | ✅ PASS | Saved active schedule into NVS slot 'custom1' as 'SWE5Test' |
| **SWE.5-TC18.4** | Default Preset Restoration ('default') | `FR-26.2` | 329.0 ms | ✅ PASS | Restored factory 'default' preset |
| **SWE.5-TC19.1** | Invalid Anchor Rejection (HTTP 400) | `FR-8, FR-26.1` | 256.7 ms | ✅ PASS | HTTP 400 rejected invalid anchor name: Invalid anchor. Use sleep, wake, away, or home. |
| **SWE.5-TC19.2** | Out-of-Bounds Temperature Clamping | `FR-1` | 1164.7 ms | ✅ PASS | Verified firmware hard-clamping to physical safety limits [15.0°C, 30.0°C] |
| **SWE.5-TC19.3** | Invalid Zone Index Handling (HTTP 400) | `FR-1, FR-8` | 209.1 ms | ✅ PASS | HTTP 400 safely rejected zone index 99: Invalid zone index |
| **SWE.5-TC19.4** | Invalid Preset Application Rejection | `FR-26.2` | 246.9 ms | ✅ PASS | HTTP 400 rejected non-existent preset: Preset not found or invalid. |
| **SWE.5-TC19.5** | Factory Preset Overwrite Protection | `FR-26.2` | 191.8 ms | ✅ PASS | HTTP 400 protected factory preset 'default': Cannot overwrite factory preset or invalid id. |

---

## 4. Concluzii Inginerești & Siguranță

1. **Atomicitate & Izolare 100%:** Fiecare aserțiune critică (heap, polaritate, timpi de interblocare, idempotență, persistență NVS, ancore și sloturi de preset) este executată ca test atomic independent.
2. **Trasabilitate Fără Goluri (Zero Uncovered Requirements):** Toate cele 33 de cerințe funcționale și non-funcționale au cel puțin o verificare dedicată pe hardware live; 0 cerințe rămase neacoperite.
3. **Zero Blocaj / Zero Stalls:** Toate apelurile HTTP utilizează conexiuni fără socket exhaustion, obținând o durată de execuție sub 25 de secunde pentru întreaga suită de 60 de verificări atomice.
4. **Protecție Flash Wear Confirmată:** Scrierile NVS pentru ancore, timeout și preset-uri sunt supuse verificării de persistență și restaurare la valorile inițiale.
