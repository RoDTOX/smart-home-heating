# Requirements Review — `docs/requirements_and_features.md`

> ASPICE SYS.2 / SWE.1 review of the requirement baseline, done **before** fixing tests or code.
> Date: 31.10.2026. Status: **proposal — awaiting row-by-row approval.** Nothing in the requirements
> document, tests or firmware has been changed.

## 1. Scope and method

Each requirement was checked for: necessary, correct (physics + hardware: Immergas Victrix Tera 24,
230 V NC wax actuators, ESP32-Relay X4), testable (measurable value), unambiguous, consistent (with other
requirements and with the AGENTS.md safety list), in scope (no cloud / fleet / HA / kiosk).

Sources read: `AGENTS.md`, `docs/requirements_and_features.md`, `docs/plan.md`, `docs/changelog.md`,
`docs/underfloor_heating_hydraulics_and_failsafes.md`, `docs/api_reference.md`, `src/`, `include/`
(incl. uncommitted `include/Config.h`), and the requirement IDs cited in `tools/test_system.py`.

Categories: **wrong** (contradicts physics/hardware/code reality), **conflict** (two sources disagree),
**untestable**, **ambiguous**, **obsolete** (feature removed / out of scope), **missing** (behaviour
without requirement).
Proposals: **keep**, **rewrite** (proposed English text given), **retire** (strike through with `~~`),
**new** (new ID). `D-n` = needs an owner decision (section 3); the proposed text shows the recommended option.

Verified values in firmware today (for reference):

| Item | Firmware value | Where |
|---|---|---|
| Actuator interlock | 120 s (API accepts 1–600 s) | `Config.h:61`, `WebApi.cpp:831` |
| Overrun / keeper valve | 180 s, **only when GPA pump enabled** | `Config.h:62`, `ThermalZoneManager.cpp:1307` |
| Burner min run / min pause | 180 s / 180 s (NVS `min_run`/`min_pause`, API 10–600 s) | `BoilerRadioTrigger.cpp:105,124` |
| `DEFAULT_MIN_BOILER_OFF_MS` | 180 s, stored but never read | `Config.h:63` |
| RF cooldown / heartbeat | 3 s / 15 min re-send of current state | `BoilerRadioTrigger.cpp:106,230` |
| Sensor timeout (zone failsafe) | 600 s (setter 60–86400, config API 60–3600) | `ThermalZoneManager.cpp:30,313`, `WebApi.cpp:869` |
| DTC-101 in DiagnosticEngine | any heard sensor silent > 1200 s | `DiagnosticEngine.cpp:125` |
| Frost guard | < 12 °C, release at 13.5 °C, enabled zones only | `ThermalZoneManager.cpp:1085,1117,1124` |
| Boot warmup | 300 s, early exit when all paired sensors seen | `ThermalZoneManager.cpp:1163` |
| TPI | 60 min cycle, min ON/OFF 10 min, band 1.5 °C, learned duty 0–60 %, overshoot cut at target + hysteresis | `ThermalZoneManager.h:13`, `.cpp:744` |
| Pre-heat lead | 120 + 9·(5 − T_out) min, clamped 45–240 min, 90 min without weather | `ThermalZoneManager.cpp:737` |
| Window detection | drop ≥ threshold within 5 min, threshold 0.8–2.0 °C (default 1.2), 30 min pause or +0.6 °C rebound | `ThermalZoneManager.cpp:793` |
| Boost setpoint | API 15–30 °C, any step; UI 16–22.5 °C / 0.5 | `ThermalZoneManager.cpp:342`, `api_reference.md:24` |
| Level temps | 5–30 °C, factory 22 / 20 / 16 | `ThermalZoneManager.cpp:571,585` |
| TWDT | 90 s | `main.cpp:237` |
| DTC-402 heap | raise < 60 KB, clear > 75 KB | `DiagnosticEngine.cpp:166` |
| Wi-Fi supervisor | check 10 s, reconnect, full re-begin after 90 s, DTC-401; SoftAP fallback at boot | `main.cpp:133,144` |
| Anti-seize | every 7 days (epoch), 20 s, all enabled valves + pump | `ThermalZoneManager.cpp:983,1019` |

## 2. Findings

### 2.1 Safety-relevant rows

| ID | Finding | Category | Proposal | Safety |
|---|---|---|---|:-:|
| FR-2 ✅ | Interlock "120–180 s" is a range, not a value; code default 120 s; plan.md says wax actuator lag is 3–5 min; API accepts 1 s. Range alone is untestable. | conflict / ambiguous | **rewrite** (D1): "The controller shall not command boiler ON until at least one zone valve has been energised for ≥ `interlockDelay` (default 180 s, allowed 120–600 s; values below 120 s shall be rejected)." | y |
| FR-3 ✅ | "Overrun 120–180 s" vs AGENTS.md 180 s. Code keeps the last valve open only when the GPA pump is enabled — in the reference install (no pump) the last valve closes at flame OFF, so FR-3 is not met. | conflict / wrong | **rewrite** (D3): "After a boiler OFF that followed a lit flame, the last zone valve that was open shall stay open for `overrun` = 180 s (allowed 180–600 s), independent of the GPA pump setting." | y |
| FR-38 ✅ | Duplicates FR-3 (flame-conditioned overrun) and is described only for the GPA pump. | conflict | **rewrite**: merge into FR-3 as FR-3.2: "If the flame was never lit (start cancelled), overrun shall be 0 s and valves without demand close immediately." | y |
| FR-4 ✅ | "3–5 min" range; NFR-1.6 says 60 s + 60 s; tests and FR-32 say 60 s; hydraulics doc says 3 min run / 5 min pause; code 180 s / 180 s. | conflict | **rewrite** (D2): "Burner minimum run time 180 s and minimum pause 180 s, applied to every ON/OFF source (auto, API, console). Allowed configuration 120–600 s; lower values rejected." | y |
| NFR-1.6 ✅ | 60 s + 60 s contradicts FR-4 and the code; duplicate of FR-4. | conflict | **retire** (merged into FR-4) | y |
| FR-17.3 ✅ | Says 60 s / 60 s and allows `force=true` / Service Mode to override for ON. Service Mode and forced START were removed in phase 2 (they bypassed the guard). | wrong / obsolete | **rewrite**: "Manual ON/OFF requests are subject to FR-4. Only OFF may be forced (`force=true`), which stops the burner immediately. No mechanism shall start the burner inside the guard." | y |
| FR-32 ✅ | Deferred action is described with the 60 s guard. | conflict | **rewrite**: "A request rejected by FR-4 is queued (`PENDING_ON` / `PENDING_OFF`) and executed when the guard expires. An opposite request or `POST /api/boiler/cancel` cancels it." | y |
| FR-16.2 ✅ | Sensor timeout 30 min; NFR-1.2 and code 600 s; FR-18 range 60–7200 s; config API 60–3600, setter 60–86400; DiagnosticEngine raises DTC-101 at 20 min for any heard sensor. Four values for one behaviour. | conflict | **rewrite** (D4): "If a zone receives no valid reading for longer than `sensorTimeout` (default 600 s), its valve shall close, the zone enters FAULT (FR-51) and DTC-101 is raised. Normal control resumes on the first valid reading." Owner: default `sensorTimeout` = 1200 s (20 min). | y |
| NFR-1.2 ✅ | Duplicate of FR-16.2. | conflict | **retire** (merged into FR-16.2) | y |
| FR-18 ✅ | Range 60–7200 s differs from both code paths (3600 / 86400). | conflict | **rewrite** (D4): "`sensorTimeout` configurable 60–3600 s via API and console, stored in NVS; out-of-range values rejected." | y |
| FR-10.2 (DTC-101) ✅ | ">20 min" contradicts FR-16.2 / FR-18. | conflict | **rewrite** (D4): DTC-101 = "zone sensor silent > `sensorTimeout`" (single source). | y |
| FR-16.1 ✅ | Says frost guard overrides MANUAL "or OFF"; code skips disabled zones; release hysteresis (+1.5 °C) and configurable threshold (5–16 / API 5–20) not specified. | ambiguous / conflict | **rewrite** (D6, D7): "If any enabled **or disabled** zone with a valid reading is below `frostTemp` (default 12 °C, allowed 8–16 °C), that zone heats regardless of mode until it reaches `frostTemp` + 1.5 °C. Interlock (FR-2) and burner guard (FR-4) still apply." | y |
| NFR-1.3 ✅ | Duplicate of FR-16.1. | conflict | **retire** (merged into FR-16.1) | y |
| FR-51 (new) ✅ | Frost protection with a lost sensor is undefined; the plan lists "sensor-loss fallback duty" as open (frost risk vs runaway). | missing | **new** (D5 → B-17): "A zone in sensor FAULT runs its learned duty for the current outdoor temperature (bounded by NFR-5.3; `faultDuty` default 20 %, 0–30 %, when nothing is learned), still subject to frost guard (FR-16.1), interlock (FR-2) and burner guard (FR-4)." | y |
| FR-37 ✅ | Correct and needed; the "invariant" omits the overrun keeper exception that the code has. | ambiguous | **rewrite**: "A zone without demand shall have its valve de-energised within one control cycle (1 s), except the overrun keeper valve (FR-3). If demand drops during the interlock, the valve closes and no burner start is sent for it." | y |
| NFR-1.4 ✅ | "Relays LOW at boot" is wrong for NO actuators (LOW = open). Needed behaviour: valves closed, OFF burst, warmup. | wrong | **rewrite**: "At every boot the controller shall drive all zone valves to closed (per NC/NO setting), send one boiler OFF burst, and run the boot warmup (FR-22) before any heating command." | y |
| NFR-1.1 ✅ | Mentions MQTT broker and cloud (removed). Core idea correct. | obsolete wording | **rewrite**: "Loss of Wi-Fi, router or internet shall not stop heating control; all decisions run on the ESP32." | y |
| NFR-1.5 ✅ | 90 s in code and requirement; hydraulics doc says 8 s. | conflict (doc) | **keep** 90 s; fix hydraulics doc table later. | y |
| NFR-1.7 ✅ | Descriptive FMEA, not a requirement. Claims ESP32 detects boiler lockout via DTC-102 — no such detection exists (DTC-102 is battery). | wrong / untestable | **retire** (move hardware notes to the hydraulics doc). | y |
| FR-17.1 / 17.2 ✅ | Idempotent no-op and 3 s RF cooldown. Correct, testable, matches code. | — | **keep** (English). | y |
| FR-17.7 (new) ✅ | Code tag only: every 15 min the current boiler state is re-sent (receiver may have missed an OFF). Real safety behaviour, no requirement. | missing | **new**: "While no command is pending, the controller shall re-send the current boiler state every 15 min (configurable 1–60 min)." | y |
| FR-42 ✅ | Used by tests only. Config API allows interlock / overrun from 1 s and short-cycling from 10 s — weakens FR-2/3/4. | missing | **new** (keep ID): "`/api/config/system` exposes hydraulic and RF timings and shall reject values below the safety floors of FR-2, FR-3, FR-4." | y |
| FR-43 ✅ | Used by tests only. `/api/relay/test` works in AUTO mode and ignores NC/NO polarity. | missing | **new** (keep ID, D15): "Relay test is accepted only in MANUAL mode and only for configured relay pins; otherwise HTTP 409." | y |
| FR-1.3 ✅ | MANUAL lets the user force the boiler ON with all valves closed. Reference install has always-open towel radiators, other installs may not. `openLoopBypass` exists in config but is unused. | ambiguous | **rewrite** (D14): "MANUAL mode: relays and boiler commanded by the user. Boiler ON requires at least one valve open ≥ interlock, unless `openLoopBypass` is set." | y |
| FR-40 ✅ | Window threshold 0.8–2.0 °C, detection window: doc says ≤ 5 min, hydraulics doc 10 min, code 5 min. Threshold is stored per zone today; out-of-range stored value (board: 0.3 °C on Z2/Z3) is silently replaced by 1.2 °C in control. Owner: window detection is **one global setting**, not per zone. | ambiguous / conflict | **rewrite** (D9): "Window detection uses one global `windowThreshold` (0.8–2.0 °C, default 1.2) for all zones: a drop ≥ threshold within 5 min closes that zone's valve for 30 min or until +0.6 °C rebound from the minimum, or manual clear. Values outside 0.8–2.0 are rejected with HTTP 400. Per-zone thresholds are removed. This rule is the fallback when the smart detector (G-22 / FR-40.2) has no outdoor or humidity data." | y |
| FR-14.2 / 14.3 ✅ | GPA pump interlock / overrun — correct if a pump exists (D11). | — | **keep** as optional (merged with FR-3 text). | y |

### 2.2 Other rows

| ID | Finding | Category | Proposal | Safety |
|---|---|---|---|:-:|
| §1, §2 ✅ | Vision lists Tailscale, kiosk-like UI, Tronic outdoor receiver; topology table hard-codes the reference install. | obsolete / ambiguous | **rewrite**: short vision (AGENTS.md §1); topology as "Reference installation (example configuration)". | n |
| FR-1.1 ✅ | ON/OFF hysteresis loop replaced by TPI in phase 4; hysteresis is now only the overshoot cut. | obsolete | **retire**, replaced by FR-47. | n |
| FR-1.2 ✅ | 15–30 °C / 0.5 steps; API accepts any float 15–30; UI 16–22.5; levels 5–30. | conflict | **rewrite** (D10): "Boost raises a zone's target above its current schedule level by 0.5–2.0 °C in 0.5 °C steps (never lower; to cool use away), enforced by firmware; expires after `boostDuration` (default 3 h, 1–6 h) or at the next schedule slot, whichever comes first (FR-39)." | n |
| FR-5.1 ✅ | Matches code (409 steps, 65 ms pre/post). | — | **keep**. | n |
| FR-5.2 ✅ | Antenna hardware; not verifiable by firmware tests. | untestable | **rewrite**: "The boiler receiver shall acknowledge (burner state change) ON and OFF from the controller's installed position — verified at commissioning." | n |
| FR-6.1 / 6.2 ✅ | Correct. | — | **keep**. | n |
| FR-6.3 ✅ | MQTT / Galaxy A6 fallback does not exist. | obsolete | **retire** the MQTT part; keep the NimBLE passive scan sentence. | n |
| FR-7.1 ✅ | Telnet :23 exists. | — | **keep**. | n |
| FR-7.2 ✅ | Web console page removed in phase 2. | obsolete | **retire**. | n |
| FR-7.3 ✅ | Command list partly outdated. | ambiguous | **rewrite**: "Console commands are accepted via Serial, Telnet and `POST /api/console/send`; `HELP` lists them." | n |
| FR-45 ✅ | Test-only ID (console over HTTP + `CLEAR`). | missing | **new** (keep ID): "`/api/console/send` executes a console command and returns new log lines; `CLEAR` empties the log buffer." Merge with FR-7.3 acceptable. | n |
| FR-8.1 ✅ | Describes old dashboard, Galaxy A6 reverse proxy. | obsolete | **rewrite**: "One page served at `/`: family view (rooms, boost, away, alerts) and `#admin` view." | n |
| FR-8.2 ✅ | Tailscale VPN is the user's network setup, not product behaviour. | obsolete | **retire**. | n |
| FR-8.3 / NFR-2.5 ✅ | OTA "< 7 s" vs "< 14 s"; 12 ms pacing is a tool detail. | conflict | **rewrite** as FR-8.3: "Firmware update via `/update` with HTTP Basic auth; `tools/ota_upload.py` completes a 1.25 MB image in < 30 s on the home LAN." Retire NFR-2.5. | n |
| FR-9 / FR-12.3 ✅ | SYN480R / Tronic outdoor receiver not implemented; Open-Meteo every 15 min is. Duplicate. | obsolete / conflict | **rewrite** FR-9: "Outdoor temperature from Open-Meteo every 15 min (retry 5 min on failure)." Retire FR-12.3. | n |
| FR-9.2 (new) ✅ | Location setting exists (`/api/setup/location`, `LOCATION`); tests cite it as FR-13. | missing | **new** (D18): "Weather location as lat,lon stored in NVS." | n |
| FR-10.1 / 10.3 / 10.5 ✅ | Cloud, Service Mode, fleet OTA. | obsolete | **retire**. | n |
| FR-10.2 ✅ | DTC-102 battery ✓, 103 ✓, 401 ✓, 402 ✓; DTC-201 described as relay cycles/hour but code = boiler request spam; DTC-202 not implemented. | wrong | **rewrite**: list only implemented codes with their real triggers; drop DTC-202. | n |
| FR-10.4 ✅ | Self-test lists MQTT bridge; "< 500 ms" never measured. | obsolete | **rewrite**, merged with G-10: "`/api/diag/selftest` reports Wi-Fi, heap, BLE, zones, RF and health score 0–100 % (quick part, any time); the boiler part (relays, boiler link, burn, flow rise) runs only on explicit request (G-10 c)." | n |
| FR-11.1 ✅ | "No hard-coded MACs" — code has reference MACs in `Config.h:77` and `ThermalZoneManager.cpp:61`, and forces zone 3's name (`.cpp:138`). | conflict (req vs code) | **keep** requirement (rewrite in English): "Firmware ships without sensor MACs or zone names; they are set at commissioning and stored in NVS." Code mismatch reported in §5. | n |
| FR-11.2 ✅ | Setup wizard `/setup` removed. | obsolete | **retire**; keep `/api/setup/discovered-sensors` under FR-11.3. | n |
| FR-11.3 / 11.4 ✅ | Mapping zone ↔ relay ↔ sensor and NVS persistence exist. | — | **rewrite** (English, current endpoints). | n |
| FR-12.1 ✅ | NTP + Romanian TZ — matches code. | — | **keep**. | n |
| FR-12.2 ✅ | Season is display only; anti-seize is not season-gated. | wrong | **rewrite** (D34): "Heating season mode AUTO / ON / OFF. AUTO: season ON when the daily mean outdoor temperature is < `heatLimit` (default 15 °C, 10–18) for `seasonOnDays` (default 2) consecutive days; OFF when > `heatLimit` + 2 °C for `seasonOffDays` (default 5) days. Safety net: in season OFF, if any zone stays below its night level for 12 h, season turns ON (logged). No weather data → state unchanged. Season OFF = global OFF (G-14): only frost guard and valve exercise. Fixed dates removed." | n |
| FR-12.4 ✅ | City search, county list, GPS button — none exist. | obsolete | **retire** (replaced by FR-9.2). | n |
| FR-13.x ✅ | Device passport removed in phase 2; tests reuse FR-13 for location. | obsolete | **retire** (D18). | n |
| FR-14.1 ✅ | GPA pump on relay 4 — optional per AGENTS.md, unused here. | ambiguous | **rewrite** (D11): "Optional circulation pump relay, disabled by default." | n |
| FR-14.4 / FR-19 ✅ | Weekly anti-seize, 20 s. 20 s does not move a wax actuator (needs 2–5 min); only the pump benefits. | wrong | **rewrite** (D27): "A valve closed for ≥ `exerciseIdleDays` (default 14, 7–14) is opened for 5 min with the boiler OFF; the optional pump runs during the exercise; idle time tracked per valve, stored in NVS (on change only)." | n |
| FR-15 ✅ | NC/NO polarity — exists; `/setup` reference obsolete. | — | **keep** (English, API/CLI only). | n |
| FR-17.4 ✅ | 429 body also has `status:"pending"` variant. | ambiguous | **rewrite**: list both bodies (`rate_limited`, `pending`) with `waitSec`. | n |
| FR-17.5 ✅ | ≥ 3 rejected requests within 10 s → DTC-201. Matches code. | — | **keep**. | n |
| FR-17.6 ✅ | "< 1 ms pre-validation" unmeasured; RF burst blocks ~260 ms. | untestable | **rewrite**: "One RF burst blocks `loop()` ≤ 300 ms, interrupts disabled ≤ 150 ms; rejected requests send no burst." | n |
| FR-20 ✅ | Link-quality classes exist (console/API); `/setup` reference obsolete. | — | **keep** (rewrite location). | n |
| FR-21 ✅ | Version, build, git hash, reset reason. | — | **keep**. | n |
| FR-22 ✅ | Correct; "beacons 30–60 s" wrong for PVVX (default advertising ~2.5 s, measurement ~10 s). | wrong (detail) | **rewrite**: drop the beacon interval sentence; keep 300 s / early exit / DTC-101 per missing zone. | n |
| FR-28 ✅ | Scan window is an implementation detail. | ambiguous | **rewrite**: "BLE passive scan at ≥ 90 % duty; zones whose sensors are acquired control normally during warmup." | n |
| FR-23 / 24 / 25 / 27 ✅ | Console table formatting, column widths, colours, `utf8VisibleLen`, TelinkMiFlasher link (not in code); cards = old UI. | obsolete / untestable | **retire** FR-25, FR-27; **rewrite** FR-23 + FR-24 into one (D16): "`STATUS` shows zones, sensors, boiler, protections, heap (free / min / max block) and OTA margin." | n |
| FR-26 ✅ | Web console colouring — page removed. | obsolete | **retire**. | n |
| FR-29 / 30 / 31 ✅ | Bio-schedule, anchors, presets — replaced by levels and slot lists in phase 2; tests already use the new meaning. | obsolete | **rewrite** with same IDs: FR-29 "Levels day / night / away per zone (5–30 °C, factory 23 / 21 / 18 (owner, B-04))", FR-30 "Level temps in NVS, write on change", FR-31 "One weekday and one weekend list, ≤ 8 slots `HH:MM=level`, sorted". | n |
| FR-44 ✅ | Test-only ID: global away. | missing | **new** (keep ID): "Away mode sets every zone's target to its away level and clears boosts; off returns to the schedule." | n |
| FR-33 ✅ | Sparklines from old UI; not in current page. | obsolete | **retire**. | n |
| FR-34 ✅ | LittleFS history 24 h / 5 d exists. | — | **keep** (English). | n |
| FR-35 ✅ | Burn-time counters exist; gas/cost model and HA / `index_v2.html` do not. | obsolete (part) | **rewrite**: counters only (total, today, yesterday, month, last month, current burn). | n |
| FR-36 ✅ | Home Assistant removed in phase 1. | obsolete | **retire**. | n |
| FR-39 ✅ | "Anchor", HA wording obsolete; 3 s NVS debounce correct. | obsolete wording | **rewrite**: "A boost ends at the first of: `boostDuration` elapsed, next slot change, `hold/clear`." | n |
| FR-41 ✅ | Test-only ID: Wi-Fi supervisor. | missing | **new** (keep ID): "Wi-Fi checked every 10 s; reconnect; full re-init after 90 s down; DTC-401 while down or RSSI < −82 dBm." | n |
| FR-46 ✅ | Appears in a test group title only ("Dynamic Time Blocks"), removed feature. | obsolete | **retire** (never defined). | n |
| FR-47 (new) ✅ | TPI valve control has no requirement. | missing | **new**: "Each zone uses time-proportional control: 60 min cycle, duty = error / 1.5 °C + learned base duty (0–60 %), min ON / OFF 10 min, new cycle on target change > 0.4 °C, overshoot cut at target + hysteresis. Learned duty stored in NVS when it changes ≥ 5 %." | n |
| FR-48 (new) ✅ | Weather-shifted pre-heat has no requirement. | missing | **new**: "If the next slot is warmer, the next level starts early by 120 + 9·(5 − T_out) min, limited to 45–240 min; 90 min without weather data." | n |
| FR-49 (new) ✅ | Sensor calibration offset has no requirement. | missing | **new**: "Per-zone sensor offset −3.0…+3.0 °C added to the reading, stored in NVS." | n |
| FR-50 (new) ✅ | SoftAP fallback when Wi-Fi is not reachable at boot — no requirement. | missing | **new**: "If the configured Wi-Fi is not joined at boot, the controller opens access point `SmartHeating-Setup`; control continues." | n |
| NFR-2.1 ✅ | < 1.2 W needs a meter; no test. | untestable | **retire**. | n |
| NFR-2.2 ✅ | > 115 KB vs test > 100 KB vs DTC-402 60 KB. | conflict | **rewrite** (D8): "Free heap ≥ 100 KB during normal operation; DTC-402 below 60 KB." | n |
| NFR-2.3 ✅ | API < 100 ms — test measures latency. | — | **keep**. | n |
| NFR-2.4 ✅ | Console timestamps OK; web console / Nginx parts obsolete. | obsolete (part) | **rewrite**: timestamps only. | n |
| NFR-2.6 ✅ | Lists `DevicePassport`; "> 115 KB". | obsolete | **rewrite**: "No heap decrease > 5 KB over 24 h of operation." | n |
| NFR-2.7 ✅ | Dev server is tooling, not product. | obsolete | **retire** (README). | n |
| NFR-2.8 (new) ✅ | Firmware < 1.25 MB (AGENTS.md) — no requirement. | missing | **new**. | n |
| NFR-2.9 (new) ✅ | NVS writes only on value change (AGENTS.md). | missing | **new**. | n |
| NFR-3.1 ✅ | Says UI texts English; API messages are Romanian (`WebApi.cpp:33`); family UI language not decided. | conflict | **rewrite** (D17): "Code, API, logs and docs in English; family UI language configurable RO / EN." | n |
| NFR-3.2 ✅ | Module list includes `DevicePassport`; add `WebApi`, `ConsoleCommands`, `TelemetryHistoryManager`. | obsolete | **rewrite**. | n |
| NFR-4.1 (new) ✅ | Plan goal "every occupied room within ±0.3 °C" has no requirement. | missing | **new**: "In steady state (no schedule change for 3 h, window closed) room temperature stays within ±0.3 °C of target." | n |
| §5 matrix ✅ | Statuses "100 % PASS" are claims, not evidence; links point to `c:/Users/BSeceleanu/...`. | wrong | **retire** the static matrix; `process_check.py trace --matrix` generates it. | n |
| Form ✅ | Two sections numbered 3.3; whole document in Romanian (AGENTS.md: docs in English); inline `Status: [VERIFIED]`. | form | **rewrite**: English, single numbering, no inline status. | n |

## 3. Decisions for the owner

| # | Question | Options | Recommendation |
|---|---|---|---|
| D1 | Actuator interlock default and floor | A 120 s default / B **180 s default**, floor 120 s in both cases | B — plan.md itself says 3–5 min lag | ✅ owner |
| D2 | Burner min run / min pause | A **180 / 180 s** (code) / B 180 / 300 s (hydraulics doc) / C 60 / 60 s (NFR-1.6, tests) | A; check the Victrix own anti-cycling parameter | ✅ owner |
| D3 | Overrun without GPA pump | A **keeper valve open 180 s always** (needs code change) / B FR-3 only with pump, rely on towel radiators | A — other installs may lack an open bypass | ✅ owner |
| D4 | Sensor timeout | one value for control and DTC-101 | ✅ owner: default 1200 s (20 min), range 60–3600 s, separate 20 min DTC path dropped |
| D5 | Zone in FAULT when cold outside (plan open item) | A keep full close / B fallback duty x % / C fallback only below 0 °C outdoor | decide later; FR-51 stays TBD | ✅ resolved by B-17: 20 % fault duty (FR-51) |
| D6 | Frost guard on disabled zones | **yes** / no | yes | ✅ owner |
| D7 | Frost threshold range | 5–16 (setter) / 5–20 (API) / **8–16** | 8–16 | ✅ owner |
| D8 | Free heap limit | **≥ 100 KB** / > 115 KB | 100 KB (matches test and board) | ✅ owner |
| D9 | Window detection | one global threshold or per zone | ✅ owner: one global threshold, default 1.2 °C, 5 min window, out-of-range rejected; per-zone values (Z2/Z3 = 0.3) dropped |
| D10 | Boost range | 16–22.5 / 0.5 vs 15–30 any step | ✅ owner: upward only, current level + 0.5…2.0 °C in 0.5 °C steps (day 23 → max 25) |
| D11 | GPA pump (FR-14) | **keep as optional** / retire | keep (AGENTS.md mission lists it) | ✅ owner |
| D12 | Anti-seize duration | **valves ≥ interlock, pump 20 s** / keep 20 s / retire valve part | valves ≥ interlock | ✅ superseded by D27 |
| D13 | Hard-coded MACs (FR-11.1) | **keep requirement, code to blank defaults** / relax requirement | keep requirement | ✅ owner: no MACs / names in firmware, set at commissioning |
| D14 | Manual boiler ON with valves closed | **require open valve unless `openLoopBypass`** / allow | require | ✅ owner |
| D15 | Relay test in AUTO mode | **MANUAL only** / allow | MANUAL only | ✅ owner |
| D16 | Console formatting FR-23..27 | **one compact FR** / keep all | one FR | ✅ owner |
| D17 | Language of family UI and API messages | **API/logs English, UI Romanian** / all English | API English, UI Romanian | ✅ owner: UI configurable RO / EN |
| D18 | Location ID | **new FR-9.2, retire FR-13** / reuse FR-13 with new meaning | FR-9.2 (tests re-mapped later) | ✅ owner |

## 4. Domain gaps — what the product needs and neither the document nor the code has

Derived from the mission ("install once, calibrate once, forget it"), the physics of a screed floor fed
directly by a condensing combi boiler, one-way 433 MHz control, unauthenticated BLE sensors and a LAN
device — **not** from the firmware. "Code" column only says whether anything exists today.
`Phase` = when it belongs (AGENTS.md §2: no features outside the current phase); a requirement can be
baselined now and implemented later.

### 4.1 Safety

| # | Gap | Why it matters | Proposed requirement | Code | Phase | Safety |
|---|---|---|---|:-:|:-:|:-:|
| G-01 | **Flow temperature limit** ✅ owner (changed) | Reference install has no mixing valve / GPA: boiler water goes straight into the screed. EN 1264 limits floor surface to 29 °C (occupied). Owner: add flow and return sensors in the manifold box. | "(a) Commissioning sets the boiler CH flow setpoint ≤ 45 °C. (b) Optional flow and return temperature sensors at the manifold, enabled/disabled in config (`flowSensors`). If flow > `flowMax` (default 45 °C, 35–55) for > 2 min → boiler OFF + DTC. Flow/return values are used by G-04 and the G-10 self-test and shown in `STATUS`." | no | now (req), hardware D30 | y |
| G-02 | **Controller dead while boiler ON** ✅ owner (changed) | ESP32 hang, power loss or brown-out: NC valves close, the boiler link keeps the last ON. Boiler then runs only through the towel radiators, indefinitely. Owner: the current receiver is dumb and will be replaced by an ESP32-class receiver (link BLE, Wi-Fi or 433 MHz — open). | "The boiler link fails safe. Wireless: the controller sends a heartbeat every `hbInterval` (default 60 s); the receiver opens the boiler contact (OFF) if no valid frame arrives within `hbTimeout` (default 3 × `hbInterval`). Wired: the boiler contact is a dry NO relay contact on the controller, so loss of power = open = OFF. The heartbeat is a config option (`boilerLinkHeartbeat` on/off) for installs without a capable receiver; while off, the page shows 'boiler link not fail-safe'." | no | now (req), receiver hardware later | y |
| G-03 | **Room overheat / valve stuck open / wrong NC-NO** ✅ owner | If a zone keeps warming while its valve is commanded closed, polarity is wrong or the valve is stuck open; the screed heats the room with no limit. | "If a zone is > target + `overheatMargin` (default 2 °C, 1–5) and still rising for `overheatTime` (default 60 min, 30–180) with its valve closed, raise DTC (valve stuck open / polarity) and show it in the page." | no | phase 4 | y |
| G-04 | **Heating without effect** (old DTC-202) ✅ owner | Valve stuck closed, air in loop, boiler in lockout (EN 298) or link lost: zone demands for hours, temperature does not rise. | "If a zone has demand ≥ `noEffectDuty` (default 50 %, 30–100) for `noEffectTime` (default 3 h, 1–6) and rises < `noEffectRise` (default 0.2 °C, 0.1–1.0), raise DTC. If all demanding zones show it, or flow temperature (G-01) does not rise within 15 min of boiler ON, raise 'boiler not heating'." | no | phase 4 | y |
| G-05 | **Spoofed or foreign BLE sensor** ✅ owner (plausibility) | BLE adverts are unauthenticated; any nearby device can broadcast the sensor MAC. A fake 25 °C kills heating and frost guard. Encryption is possible: PVVX supports encrypted BTHome v2 (AES-CCM, bindkey); ESP32 mbedtls is part of the core, no new library. | "A reading that differs from the previous one by > `maxJump` (default 2 °C, 0.5–5) is rejected and counted. Optional later: per-sensor bindkey; unencrypted adverts ignored when a key is set." | no | now (plausibility), later (encryption) | y |
| G-06 | **Unauthenticated control API + `Access-Control-Allow-Origin: *`** ✅ owner | Any LAN device, and any web page the user opens in a browser (cross-site POST), can start the boiler, set interlock to 1 s or switch NC/NO. | "Write endpoints that change safety timings, polarity, relays or boiler state require authentication (same credentials as OTA); CORS limited to the device's own origin." | no | now | y |
| G-07 | **Default credentials** ✅ owner (changed) | AP `12345678`, OTA `admin/admin123`, Wi-Fi placeholder in `Config.h`. Owner: a universal default is acceptable for the first flash. | "First flash may use universal default credentials (OTA, AP, Wi-Fi). While any default is active, DTC 'default credentials' is raised and shown in the page. Credentials are changeable via API / console without re-flashing and stored in NVS." | no | now | y |
| G-08 | **Manual mode left on** ✅ owner | MANUAL boiler ON with no expiry contradicts "forget it"; schedule, TPI and window guard are bypassed. | "MANUAL mode returns to AUTO after `manualTimeout` (default 3 h, 1–6, aligned with boost expiry B-19) and is shown as a warning while active." | no | now | y |
| G-09 | **Sensor plausibility** (merged with G-05) ✅ owner (changed) | Range 5–45 °C exists only as a DTC; a reading still drives control. Owner: a fixed rate limit (1 °C/min) would flag an open window as a fault. So reject isolated spikes, not trends: a real window drop repeats in the next adverts, a glitch does not. | "A reading outside `validMin`–`validMax` (default 5–45 °C) is ignored for control. A reading that differs by > `maxJump` (default 2 °C, 0.5–5) from the last accepted one is held, and accepted only if the next reading confirms it (within 0.5 °C); otherwise discarded and counted. Window detection uses accepted readings. `spikeCount` ≥ 3 within 1 h → DTC (sensor suspect); zone FAULT only by timeout." | partly | now | y |
| G-10 | **OTA and self-test** ✅ owner (changed) | Flash write blocks the loop up to ~30 s with the boiler possibly ON; a bad image boots into an unknown state. Owner: self-test with boiler start, run manually when needed. | "(a) OTA starts only with boiler OFF. (b) After boot of a new image, automatic firmware checks without boiler (loop/TWDT alive, Wi-Fi up, sensors received for 60 s); image marked valid only if they pass, otherwise ESP-IDF rollback + DTC. (c) Full self-test on request (API / console): each zone relay switched, RF/boiler link frame sent, one zone opened (interlock respected), boiler ON for `selfTestBurn` (default 10 min), flow rise checked if G-01 sensors are enabled, boiler OFF with all guards; result stored and shown. A plant failure raises DTC, never a rollback." | no | now | y |

### 4.2 Robustness and failure modes

| # | Gap | Why | Proposed requirement | Code | Phase | Safety |
|---|---|---|---|:-:|:-:|:-:|
| G-11 ✅ | **No valid time** | No RTC: after a power cut without internet the schedule does not run. Which target applies is undefined. | "Without valid time, each zone keeps the last active schedule level (stored in NVS on level change) until NTP succeeds; the page shows 'time not set'." Owner: last known level. | partly (static target) | now | n |
| G-12 ✅ | **DTC history lost on reboot** | FR-22 claims DTC-101 is stored in NVS — it is not; after a watchdog reset nobody knows what happened. | "The last 50 safety events (DTC, frost, FAULT, watchdog reset) are kept across reboots with timestamp (uptime if time not set); written only when an event occurs." Owner: 50. | no | now | n |
| G-13 ✅ | **Boiler minimum load** | Victrix Tera 24 has a minimum modulation output; one small zone (2 loops) cannot absorb it → the boiler short-cycles on its own flow thermostat even if our guard holds. | "TPI cycles of all zones shall start aligned, so open valves overlap; a single zone below `minLoops` (config) is extended or grouped with another zone." | no | phase 4 | n |
| G-14 ✅ | **Summer / heating OFF** | There is no "heating off for summer" except MANUAL. | "Global OFF mode: no heating except frost guard and valve exercise. Entered manually or by season AUTO (FR-12.2)." | no | now | n |
| G-15 ✅ | **Board replacement** | "Calibrate once" is lost when the board dies: mapping, offsets, learned duty live only in NVS. | "Export / import of the full configuration as JSON via API." | no | later | n |
| G-16 ✅ | **Factory reset** | No way to clear NVS without flashing. | "Console / API factory reset with confirmation, keeps Wi-Fi credentials." | no | later | n |

### 4.3 Comfort and the mission

| # | Gap | Why | Proposed requirement | Code | Phase | Safety |
|---|---|---|---|:-:|:-:|:-:|
| G-17 ✅ | **Nobody sees the alarms** | Alerts are page-only (plan decision). A sensor battery dies in January → zone closed → cold room discovered days later. Conflicts with "forget it". | Owner: **page-only**, documented limitation: "Alarms (DTC, FAULT, frost, default credentials, boiler not heating) are shown in the page and `/api/status`; there is no push channel." | no | decide | y |
| G-18 ✅ | **Learned per-zone heat-up model** | Plan phase 4 promises a learned model; pre-heat today is a fixed outdoor formula for all zones. | "Per-zone heat-up rate (°C/h vs outdoor) learned and used for pre-heat lead; persisted." | no | phase 4 | n |
| G-19 ✅ | **Commissioning procedure** | "Calibrate once" has no defined steps or pass criteria. | "Commissioning checklist, result stored: (1) installation — manifold flow meters balanced per zone (B-01), always-open bypass present (B-13), boiler CH flow setpoint ≤ 45 °C (G-01); (2) sensors — PVVX/BTHome, advert interval ≤ 10 s (B-08), 24 h side by side, offsets to ±0.2 °C (B-10), mounted 1.2–1.5 m on an internal wall away from floor, sun, doors and drafts (B-11); (3) function — relay test per zone, valve opening time measured, boiler ON/OFF confirmed by the full self-test (G-10)." | no | now (doc) | n |
| G-20 ✅ | **Number of zones is a constant** (owner input: 1–32) | `MAX_THERMAL_ZONES = 3` while AGENTS.md requires zones as configuration. Owner: number of zones configurable 1–32. The X4 board has 4 relays, so more than 4 zones needs extra outputs (I/O expander or relay board — hardware TBD). | "Number of zones configurable 1–32 (each zone: name, sensor, output). Output count is limited by the configured hardware; a zone without an output is rejected. Memory for 32 zones is reserved statically (no heap growth)." | no | phase 2 (config), hardware later | n |
| G-21 ✅ | **Comfort accuracy measurable** | Plan goal ±0.3 °C exists nowhere as requirement (proposed NFR-4.1 in §2). | see NFR-4.1 | — | phase 4 | n |
| G-22 | **Smart window detection** ✅ owner | A fixed temperature-drop threshold misfires both ways: on a mild day an open window cools slowly (missed), a door to a cold hallway or a draft near the sensor cools fast (false). Physics gives three better signals, all already available: (1) **temperature slope** over a sliding window instead of a two-point difference; (2) **absolute humidity** (from T and RH) — winter outdoor air is dry, so absolute humidity drops when a window opens, while RH alone can rise as the air cools; a draft from a hallway does not change absolute humidity much; (3) **indoor–outdoor difference** — the expected cooling rate scales with ΔT, so the threshold must too. | "FR-40.2: Each zone evaluates a sliding window (≥ 3 accepted readings, default 5 min). A window-open event is declared when (a) the temperature slope ≤ −`k`·(T_in − T_out) (°C/min) **and** absolute humidity drops by ≥ `ahDrop` (g/m³) in the same window, or (b) the temperature drop alone exceeds 2 × the FR-40 threshold. Detection is suspended when T_in − T_out < `minDeltaT` (default 5 °C). The event ends when the slope stays ≥ 0 and absolute humidity recovers for `endConfirm` readings, or after `maxPause` (default 30 min, 10–90). All parameters are global; FR-40 is used when outdoor or humidity data is missing. Acceptance on recorded traces from the reference install: ≥ 90 % of window openings ≥ 3 min detected within 5 min; ≤ 1 false event per zone per week." | partly (FR-40 rule) | phase 4 | n |

### 4.4 Decisions added by this section

| # | Question | Recommendation |
|---|---|---|
| D19 | G-02: receiver for the boiler link | ✅ owner: replace the dumb receiver with an ESP32-class one (BLE / Wi-Fi / 433 MHz to be chosen); heartbeat switchable in config |
| D20 | G-05: encrypted BTHome (re-flash sensors with bindkey) or plausibility filter only? | plausibility now, encryption at next battery change | ✅ owner: plausibility now (G-05), encryption optional later |
| D21 | G-06: API authentication now (breaks `esp_cli.py` / tests until updated)? | yes, for safety-relevant writes only | ✅ owner: yes (G-06) |
| D22 | G-17: push channel or "page-only" accepted as a documented limitation? | owner call — it decides whether "forget it" is achievable | ✅ owner: page-only, documented |
| D23 | G-13: zone grouping in phase 4? | yes, together with TPI tuning | ✅ owner |
| D30 | G-01: flow/return sensors | ✅ owner: optional, enabled/disabled in config; sensor type TBD (DS18B20 would need `OneWire` + `DallasTemperature`, new libraries — AGENTS.md §3) |
| D31 | G-10: boiler step of the self-test | ✅ owner: manual, on request only; automatic post-OTA checks without boiler decide rollback |
| D32 | G-22: smart window detection — phase 4 (with the predictive control) or now? | ✅ owner: phase 4; FR-40 stays meanwhile. It needs recorded traces first to set `k`, `ahDrop` and to test |
| D33 | G-22: where do the traces for tuning and acceptance come from? | ✅ owner: log zone T/RH/T_out every 30 s to the PC via `tools/sh_*` (no flash wear), mark real window openings by hand for 2–3 weeks |

### 4.5 Owner's blueprint (`underfloor_heating_blueprint_2.pdf`) vs. this review

Each blueprint item checked against the requirements, the review rows above and the reference install.
Verdict: **covered** (already a requirement / review row), **strengthens** (confirms or changes a row above),
**new** (candidate requirement), **reject** (does not fit this install or the scope).

| # | Blueprint item | Verdict | Assessment | Proposal | Phase | Safety |
|---|---|:-:|---|---|:-:|:-:|
| B-01 ✅ | Several loops per room act as one entity | covered | Zones = circuits on one relay (FR-41..43). Mechanical balancing is commissioning. | add "manifold flow meters balanced per zone" to G-19 checklist | now (doc) | n |
| B-02 ✅ | TPI, 1 h cycle, 100 % at −1.5 °C, ~25 % at −0.2 °C | covered | FR-47 (60 min, band 1.5 °C). The 0.2 °C → 25 % point is a tuning expectation, not a requirement. | keep FR-47; use the point as a test case for the duty curve | — | n |
| B-03 ✅ | Weather early start: 2 h at +5 °C, 3.5 h at −5 °C | covered | FR-48 formula `120 + 9·(5 − T_out)` gives exactly 120 / 210 min. | keep | — | n |
| B-04 ✅ | Setback day 20–21 / night 17–18 / away 16 | covered | Levels are configuration (FR-29..31); factory defaults 22/20/16 differ. | Owner: factory defaults day 23 / night 21 / away 18 °C (FR-29) | — | n |
| B-05 ✅ | Humidity-compensated target (−0.5 °C at 60 % RH) | **new** (nice to have) | Sensors already report RH. Effect at 20–22 °C is small and individual, so it must never hide the owner's target: opt-in, bounded, visible. Owner: keep as nice to have. | "Per-zone opt-in: at RH ≥ 60 % the control target is lowered by up to 0.5 °C; the effective target is shown in the admin view." | phase 4+ | n |
| B-06 | Dew point guard (floor cooling) | reject ✅ owner | Only relevant for cooling; the install is heating-only (gas boiler). | none; note "no cooling" as out-of-scope assumption | — | n |
| B-07 ✅ | Solar gain throttling (south/west rooms) | **new** | Real effect on screed (hours of lag). Needs no new hardware: Open-Meteo already gives `shortwave_radiation` forecast; zone needs an orientation/solar-factor config. Fits phase 4 (predictive). | "If forecast irradiance for the next 3 h exceeds X W/m², zones flagged `solarGain` reduce TPI duty by y %" — values to tune | phase 4 | n |
| B-08 ✅ | PVVX/ATC firmware, broadcast every 5–10 s, BTHome | covered | Assumed by sensor requirements. Interval is a sensor setting, not firmware. | add to G-19: "sensor firmware PVVX, BTHome, advert interval ≤ 10 s (encrypted if D20 = yes)" | now (doc) | n |
| B-09 ✅ | BLE gateway mesh / proxies | **new** (nice to have) | One ESP32 with passive scan covers the reference install; extra receivers matter for larger homes. Owner: keep as nice to have. Must stay local (no HA, no cloud). | "Optional extra BLE receivers (ESP32) forward sensor adverts to the controller on the LAN; the controller uses the strongest recent reading." Interim: DTC when RSSI < −90 dBm for 1 h | later | n |
| B-10 ✅ | 24 h side-by-side sensor calibration | strengthens G-19 | FR-49 offsets exist; procedure for obtaining them does not. | G-19: "all sensors 24 h together next to a reference; offset = mean difference; residual ≤ 0.2 °C" | now (doc) | n |
| B-11 ✅ | Sensor placement 1.2–1.5 m, internal wall, no floor/sun/draft | strengthens G-19 | A sensor seeing the warm floor or sun breaks every control loop; it is an install requirement. | G-19: placement rule as checklist item | now (doc) | n |
| B-12 ✅ | Pump only if ≥ 1 valve open (deadhead) | **new** | Reference install has no GPA pump (boiler pump + towel-radiator bypass), but relay 4 can be a pump (FR-3, G-20). No requirement forbids pump ON with all valves closed. | "The optional zone pump shall run only while ≥ 1 valve is open or during pump overrun with the keeper valve open." | now (req) | y |
| B-13 ✅ | Hydraulic bypass for single-zone loading | strengthens G-13 | Reference install relies on always-open towel radiators. Must be a stated install precondition, otherwise the boiler deadheads on its own pump when all valves close. | "Installation precondition: an always-open bypass (radiator / ΔP valve) exists; documented in commissioning." | now (doc) | y |
| B-14 ✅ | Wax lag 3–5 min → min ON 10–15 min | covered | TPI min ON/OFF 10 min; interlock 120–180 s. | keep | — | y |
| B-15 ✅ | Anti-chatter hold 3 min | covered | TPI min ON/OFF 10 min is stronger. | keep | — | n |
| B-16 ✅ | Valve exercise: closed 14 days → open 5 min | strengthens FR-14.4/19, D12 | Confirms that 20 s is useless for wax actuators. Blueprint triggers on *idle per valve*, not a fixed weekly run — fewer pointless cycles in winter. | rewrite FR-14.4/19: "A valve closed for ≥ 14 days (configurable 7–14) is opened for 5 min (≥ interlock + 2 min); boiler stays OFF; tracked per valve, persisted in NVS." D12 option A updated accordingly | now | n |
| B-17 ✅ | Sensor offline > 30 min → push alert + fixed 20 % duty | strengthens FR-51/D5, G-17 | Gives a concrete answer to D5 (option B, 20 %). 20 % = 12 min ON per hour: bounded runaway, keeps the screed above frost. Timeout differs: firmware 600 s / DTC-101 1200 s vs 30 min. | FR-51: "Zone in sensor FAULT runs fixed duty 20 % (configurable 0–30 %), still subject to the frost guard and the boiler guards." Align timeout (D-new). Alert channel stays D22 | now | y |
| B-18 ✅ | "Flight deck" UI, room cards, colour mapping | covered | Phase 3 page (family + admin). Colour mapping is UI detail. | none in requirements | — | n |
| B-19 ✅ | Manual overrides auto-expire after 2–3 h | strengthens G-08, FR-39 | Today a boost holds to the next slot change, which can be many hours (weekend). Blueprint: fixed expiry. | FR-39: "A boost expires after 3 h (configurable 1–6 h) or at the next slot change, whichever comes first." G-08 (MANUAL mode) expiry aligned to the same value | now | n (G-08: y) |
| B-20 ✅ | Override step 0.5 °C, clamp 16–22.5 °C | changed | Owner: boost only upward, max +2 °C over the current level, 0.5 °C steps. | FR-1.2 / D10 | — | n |
| B-21 ✅ | Family view (rooms, weather, one Home/Away switch); admin: duty, RSSI, calcs | covered | FR-8.1 rewrite + FR-44. Admin view must show duty and RSSI. | FR-8.1: list admin contents "duty per zone, RSSI per sensor, pre-heat lead, active DTCs" | — | n |

New decisions from this comparison:

| # | Question | Recommendation |
|---|---|---|
| D24 | B-05 humidity compensation | ✅ owner: nice to have (phase 4+) |
| D29 | B-09 extra BLE receivers | ✅ owner: nice to have (later) |
| D25 | B-07 solar gain throttling in phase 4 | yes, forecast-based, no new sensor | ✅ owner |
| D26 | B-17 sensor timeout: one value for control and DTC-101 | ✅ owner: 20 min (= D4), then fault duty 20 % |
| D27 | B-16 exercise trigger: per-valve idle 14 days vs fixed weekly | ✅ owner: per-valve idle 14 days, 5 min |
| D28 | B-19 boost expiry: 3 h fixed vs next slot | ✅ owner: min(3 h, next slot) |
| D35 | FR-51 fallback in sensor FAULT: fixed 20 % (approved) or the zone's learned duty for the current outdoor temperature, 20 % only if nothing learned | learned duty — keeps comfort, bounded by NFR-5.3 | ✅ owner: learned duty, 20 % if nothing learned |
| D36 | NFR-5.5 fault injection commands on the board (test only) | yes — otherwise fallbacks cannot be tested | ✅ owner |
| D34 | FR-12.2 heating season: fixed dates, manual only, or automatic | ✅ owner: automatic by outdoor daily mean (`heatLimit` 15 °C, ON after 2 days, OFF after 5) + room safety net + manual override |

### 4.6 Owner principle: every feature degrades gracefully (fallback matrix)

Owner (31.10.2026): algorithms shall be smart and reliable, focused on comfort and safety. **Every feature has a
fallback that stays inside the comfort and safety zone; the occupants shall not notice a fault — it is only
reported through the log and a DTC** (alarms are page-only, D22).

General requirements (new IDs) — ✅ owner (31.10.2026), together with the matrix below:

| ID | Proposed requirement |
|---|---|
| NFR-5.1 | Priority when goals conflict: **safety > comfort > economy**. A fallback may give up economy, never safety, and gives up comfort only when safety requires it. |
| NFR-5.2 | Every functional requirement states its **inputs, its fallback when an input is missing or implausible, and the DTC / log entry** raised. A requirement without a fallback clause is incomplete. |
| NFR-5.3 | Learned values (TPI base duty, heat-up model, window and solar parameters) are **bounded**; a learned value outside its bounds or a corrupt NVS record is discarded, the default is used, and a DTC is raised. Learning never stops control. |
| NFR-5.4 | Interruptions of heating shorter than 60 min are acceptable as fallback (screed thermal mass buffers them; room drop < 0.3 °C). Longer fallbacks keep heating at an estimated duty. |
| NFR-5.5 | Every fallback can be triggered on the board for test (fault injection via console / API: sensor lost, no weather, no time, no humidity, boiler link lost, flow over limit), so that `tools/sh_test_system.py` can verify it. Fault injection is refused while it would bypass a safety rule. |

Fallback matrix — goes into the new requirements document as the "Fallback" clause of each feature:

| Feature | Input / part that can fail | Fallback (comfort + safety) | Reported |
|---|---|---|---|
| Room control TPI (FR-47) | zone sensor silent > 20 min | zone runs its **learned duty for the current outdoor temperature** (NFR-5.3 bounds); 20 % if nothing learned; frost guard and boiler guards stay active (FR-51, D35) | DTC-101 |
| Room control TPI | spikes / implausible readings (G-09) | last accepted reading kept until timeout | DTC sensor suspect |
| Learned TPI duty, heat-up model (G-18) | out of bounds, NVS corrupt | defaults, continue | DTC learning reset |
| Pre-heat (FR-48, G-18) | no weather data / no model | formula with last outdoor value (≤ 6 h old), else fixed 90 min | log |
| Schedule | no valid time (G-11) | last known level per zone | DTC / page 'time not set' |
| Heating season AUTO (FR-12.2) | no weather data | state unchanged; room safety net (zone below night level 12 h → ON) | log |
| Smart window (G-22) | no humidity or outdoor data | simple rule FR-40 | log |
| Window pause (FR-40, G-22) | false detection | pause ends after 30 min max; frost guard overrides | log |
| Solar throttling (B-07) | no forecast | no throttling | log |
| Humidity compensation (B-05) | no RH | no compensation | — |
| Boiler link (G-02) | heartbeat lost | receiver turns boiler OFF (safety); on recovery the controller re-sends state; screed buffers (NFR-5.4) | DTC |
| Boiler not heating (G-04) | lockout / RF lost | re-send ON once after 15 min, then DTC; other functions continue | DTC |
| Flow limit (G-01) | flow > `flowMax` | boiler OFF until flow < `flowMax` − 5 °C, then resume with all guards (no lockout) | DTC |
| Flow sensors (G-01) | sensor missing / implausible | flow limit and flow checks disabled; boiler setpoint ≤ 45 °C is the protection | DTC |
| Wi-Fi / internet (NFR-1.1) | down | local control continues; weather fallbacks above | DTC-401 |
| Free heap (NFR-2.2) | < 60 KB | DTC-402; below 30 KB a controlled restart while boiler is OFF, state restored from NVS | DTC-402, event history |
| Reboot / watchdog | any | state restored from NVS (levels, learned values, season, last level), boot warmup | event history (G-12) |
| OTA (G-10) | bad image | automatic rollback | DTC |
| Valve stuck open / closed (G-03, G-04) | hardware | DTC; other zones continue normally | DTC |
| Default credentials (G-07) | not changed | everything works | DTC |

## 5. Code observations (reported, not fixed)

Found while checking requirements against firmware. Not part of the requirement baseline; listed so they get
a test or a fix after the baseline is approved.

| # | Observation | Where | Confidence |
|---|---|---|---|
| C1 | Frost guard sets state `ZONE_FROST_GUARD` every cycle; the warm-up branch only starts from IDLE / FAULT, so the valve may never be energised while the zone counts as "ready for boiler" → burner ON without interlock and without heating the cold zone. | `ThermalZoneManager.cpp:1121,1201,1218` | plausible — needs a test |
| C2 | Window pause ending by timeout / rebound does not reset `ZONE_WINDOW_PAUSE`; with demand the zone cannot re-enter warm-up until a manual clear. | `ThermalZoneManager.cpp:836,1201` | plausible — needs a test |
| C3 | Keeper valve overrun only runs when the GPA pump is enabled (see FR-3). | `ThermalZoneManager.cpp:1307` | confirmed by reading |
| C4 | `/api/relay/test` works in AUTO and writes the pin directly, ignoring NC/NO. | `WebApi.cpp:885` | confirmed by reading |
| C5 | Config API accepts interlock / overrun 1 s, short-cycling 10 s, frost 5–20 °C. | `WebApi.cpp:831–866` | confirmed (already in changelog) |
| C6 | `_minBoilerOffMs`, `openLoopBypass`, `boilerDelay`, `turReturSensors`, `pumpRelayPin` stored but never used. | `ThermalZoneManager.cpp:35,472–494` | confirmed by reading |
| C7 | Frost guard skipped for disabled zones. | `ThermalZoneManager.cpp:1085` | confirmed by reading |
| C8 | Reference MACs and zone 3 name hard-coded. | `Config.h:77`, `ThermalZoneManager.cpp:61,138` | confirmed |
| C9 | Window threshold outside range: control and `/api/zones` use 1.2, `/api/config/system` shows the stored value. | `ThermalZoneManager.cpp:800,896`, `WebApi.cpp:819` | confirmed by reading |
| C10 | Burner min run / pause come from NVS; the board may still hold older values. Read `/api/config/system` before D2. | `BoilerRadioTrigger.cpp:124` | to check on board |
| C11 | API messages in Romanian. | `WebApi.cpp:33,49,82` | confirmed |
| C12 | DTCs are kept in RAM only; FR-22 claims DTC-101 is stored in NVS. | `DiagnosticEngine.cpp` (no `Preferences`) | confirmed by grep |
| C13 | No authentication on any REST write endpoint; CORS `*`. | `WebApi.cpp` | confirmed by grep |

## 6. Observations from the architecture (01.11.2026)

Raised by the architecture chat in [`architecture.md`](architecture.md) §16 (O-1…O-11), decided by the owner on
01.11.2026 and applied to baseline v2 by the requirements chat.

| # | Requirement | Decision | Applied to |
|---|---|---|---|
| O-1 | FR-3, FR-37 | ✅ owner: the overrun valve stays open while the boiler is ON or `PENDING_OFF`, then for `overrun`; the boiler never burns with all zone valves closed (AR-11) | FR-3 |
| O-2 | FR-16.1, FR-51, FR-64 | ✅ owner: in global OFF (and for a disabled zone) a zone in sensor FAULT runs its fault duty while outdoor < `faultFrostOutdoor` = **5 °C** (owner chose 5 °C over the proposed 0 °C) or outdoor unknown | FR-51, FR-64, §6 |
| O-3 | NFR-3.2 | ✅ owner: modules are those of `architecture.md` | NFR-3.2 |
| O-4 | §6 | ✅ owner: missing configuration items added with the reference-install defaults | §6 |
| O-5 | FR-54 | ✅ owner: the flow-limit OFF is forced, it does not wait for the burner minimum run | FR-54 |
| O-6 | FR-12.2 | ✅ owner: the 12 h safety net only in season mode AUTO | FR-12.2 |
| O-7 | NFR-1.4 | ✅ owner: "enters the boot warmup" (zones with readings may heat during warmup, FR-22) | NFR-1.4 |
| O-8 | FR-62 | ✅ owner: events kept in RAM until reboot when the store cannot be written | FR-62 |
| O-9 | FR-47, FR-51 | ✅ owner: learned base duty per zone and outdoor band of 5 °C (7 bands) (AR-12) | FR-47 |
| O-10 | FR-69 | ✅ owner: outputs of a failed expansion count as closed, DTC-208 `ERR_OUTPUT_EXPANSION_FAULT` | FR-69, §5 |
| O-11 | FR-58 | ✅ owner: MANUAL mode and fault injection require authentication | FR-58 |
