# Requirements Review — `docs/requirements_and_features.md`

> ASPICE SYS.2 / SWE.1 review of the requirement baseline, done **before** fixing tests or code.
> Date: 31.10.2026. Status: **closed 01.11.2026, owner (chat).** Every row approved by the owner (31.10.2026;
> architecture observations §6 and process review rows §8 on 01.11.2026) and applied in requirements baseline v2.2;
> the exit criteria of `process.md` §3 hold (§7). **Open action** (not a decision): the blueprint's location and
> version, to be given by the owner (D37, §4.5). Changes after closure: §9 (baselines v2.3, v2.4). Tests and firmware
> follow in the migration steps (`architecture.md` §18).

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
| D16 | Console formatting FR-23, FR-24, FR-25, FR-26, FR-27 | **one compact FR** / keep all | one FR | ✅ owner |
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

**Source:** the owner's PDF `underfloor_heating_blueprint_2.pdf`; it stays outside the repository (D37 = B,
process review QA-15). Location and version: to be given by the owner.

Each blueprint item checked against the requirements, the review rows above and the reference install.
Verdict: **covered** (already a requirement / review row), **strengthens** (confirms or changes a row above),
**new** (candidate requirement), **reject** (does not fit this install or the scope).

| # | Blueprint item | Verdict | Assessment | Proposal | Phase | Safety |
|---|---|:-:|---|---|:-:|:-:|
| B-01 ✅ | Several loops per room act as one entity | covered | Zones = circuits on one relay (FR-11.3, FR-69; IDs corrected 01.11.2026, QA-15). Mechanical balancing is commissioning. | add "manifold flow meters balanced per zone" to G-19 checklist | now (doc) | n |
| B-02 ✅ | TPI, 1 h cycle, 100 % at −1.5 °C, ~25 % at −0.2 °C | covered | FR-47 (60 min, band 1.5 °C). The 0.2 °C → 25 % point is a tuning expectation, not a requirement. | keep FR-47; use the point as a test case for the duty curve | — | n |
| B-03 ✅ | Weather early start: 2 h at +5 °C, 3.5 h at −5 °C | covered | FR-48 formula `120 + 9·(5 − T_out)` gives exactly 120 / 210 min. | keep | — | n |
| B-04 ✅ | Setback day 20–21 / night 17–18 / away 16 | covered | Levels are configuration (FR-29, FR-30, FR-31); factory defaults 22/20/16 differ. | Owner: factory defaults day 23 / night 21 / away 18 °C (FR-29) | — | n |
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

Raised by the architecture chat in [`architecture.md`](architecture.md) §16, decided by the owner on 01.11.2026
and applied by the requirements chat: O-1…O-11 to baseline v2, O-12…O-15 to baseline v2.2.

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
| O-12 | NFR-1.8, FR-23 | ✅ owner: `STATUS` and `/api/status` report the longest control-tick delay and the longest web/loop pass since boot, so NFR-1.8 can be checked on the board (control runs in its own task, so both are named neutrally) | FR-23, NFR-1.8 |
| O-13 | NFR-1.8, FR-14.5, FR-17.3 | ✅ owner: pump stop (FR-14.5) and forced manual OFF (FR-17.3) are safety reactions of NFR-1.8; FR-17.3 stops the burner at the next control tick | NFR-1.8, FR-17.3 |
| O-14 | FR-68 | ✅ owner, option A: the installer records the commissioning result in a document; the controller does not store it (LEVEL `install`) | FR-68 |
| O-15 | FR-4 | ✅ owner: a violated safety rule found by the self-check may also end a run earlier than `burnerMinRun` (safety > burner wear, NFR-5.1) | FR-4 |

## 7. Accepted checker warnings

A `reqs` or `trace` WARN on a requirement is fixed in the text. Only a heuristic false positive approved by the
owner (process review QA-4) is listed here; `sh_process_check.py` reports a WARN that contains the WARN text of a
row as INFO. `WARN text` is a substring of the WARN line, without line numbers.

| # | Check | WARN text | Reason | Owner |
|---|---|---|---|---|
| W-1 | reqs | free heap: 3 different values - 100 KB (NFR-2.2: | Four different heap quantities, not one value stated twice: NFR-2.2 normal minimum 100 KB, DTC-402 below 60 KB, controlled restart below 30 KB; NFR-2.6 drift at most 5 KB over 24 h. | ✅ 01.11.2026 |

The other false positives approved in QA-4 no longer occur after QA-9 and QA-11: FR-37 and FR-3.2 use the control
tick of requirements §0 instead of "1 s" / "0 s", and FR-2, FR-3, FR-16.1, FR-16.2 cite the parameter instead of
its §6 default, so the frost quantity has one value left (the "+1.5 °C" release).

**Exit criteria of `process.md` §3, requirement-side findings only (01.11.2026):**

- `trace` no ERROR on the requirement side — **met**: 0 ERROR; the 16 test-side errors (tests and registry citing
  the retired FR-13, FR-24, FR-25, FR-26, FR-27, FR-33, FR-38, one docstring mismatch) are KNOWN under PB-11.
  "NFR-1.8 has no module" is architecture side.
- `reqs` no un-waived "different values" WARN on a safety quantity — **met** (W-1 is heap, waived).
- `reqs` no requirement-side DTC warning — **met**: the 14 WARN "defined … but the firmware never raises it" are
  code side (migration steps), not review rows.
- every `D-n` decided and recorded — **met**: D37, D38, D39 decided 01.11.2026 (§8.1) and recorded in
  `decisions.md`; O-12…O-15 likewise (§6). Open action: the blueprint's location and version (D37).
- owner sign-off — **met**: closed 01.11.2026, owner (chat).

## 8. Process review rows applied (01.11.2026)

Rows of [`process_review.md`](process_review.md), approved by the owner on 01.11.2026 with the recommended options,
applied to requirements baseline v2.1 by the requirements chat.

| # | Requirement | Decision | Applied to |
|---|---|---|---|
| QA-4 | review record | waiver table for heuristic false positives; "closed" in the header only when the exit criteria hold | §7; header stays open |
| QA-7 | every active requirement | LEVEL `SW` / `HW` / `install` | requirements §0 (attribute `{LEVEL; METHOD}`), every active requirement |
| QA-8 | every active requirement | METHOD `test` / `board` / `inspection` / `analysis` | requirements §0, every active requirement |
| QA-9 | FR-2, FR-4 | parameter names only, values in §6 (rule R2) | FR-2, FR-4 (forced OFF named as the only early end, FR-17.3, FR-54); same R2 fix in FR-3, FR-16.1, FR-16.2 |
| QA-10 | FR-3, FR-3.2 | "flame lit" = an ON frame was sent and not cancelled (tier A); tier B may use real feedback | requirements §0 glossary, FR-3, FR-3.2 |
| QA-11 | FR-3, FR-3.2, FR-37, FR-47 | glossary: control tick, TPI cycle, request, command, demand, accepted reading, flame lit | requirements §0 glossary, FR-3, FR-3.2, FR-37 (also "per FR-15.1"), FR-47; wording aligned in FR-1.3, FR-58 |
| QA-12 | FR-60 | new image valid when at least one advert from a mapped sensor is received within 60 s, loop and watchdog alive, Wi-Fi up | FR-60; sensor check skipped without a mapped sensor (D38) |
| QA-13 | NFR-1.4 | the boot OFF starts `burnerMinPause` | NFR-1.4 |
| QA-14 | phase tags | tags map to the migration steps of `architecture.md` §18 | requirements §0 table |
| QA-15 | review record | B-01 cites FR-11.3, FR-69; blueprint source recorded | B-01 (full IDs also in B-04, D16), §4.5 source note, D37 |
| QA-16 | new NFR | every safety reaction within 2 s of its trigger, fixed (not in §6); only exception an OTA upload with the boiler OFF | NFR-1.8 (new); radio cooldown of FR-17.2 may add up to 3 s (D39) |

`sh_process_check.py` output (line numbers dropped):

| Check | Before (baseline v2) | After (baseline v2.1) |
|---|---|---|
| `reqs` | 0 ERROR, 18 WARN: interlock (FR-2 180 s / FR-37 1 s), overrun (FR-3 180 s / FR-3.2 0 s / FR-37 1 s), frost (FR-16.1 12 °C / 1.5 °C), heap, 14 DTC never raised | 0 ERROR, 14 WARN (DTC never raised, code side), 1 waived (W-1 heap) |
| `trace` | 16 ERROR, 84 WARN | 0 ERROR, 16 KNOWN (PB-11, tests), 76 WARN (checker with R7 coverage per method); new: NFR-1.8 has no module (architecture side); 0 requirements without attribute |

Baseline v2.2 (D37, D38, D39, O-12, O-13, O-14, O-15 applied; review closed): `reqs` 0 ERROR, 14 WARN (DTC never
raised, code side), 1 waived (W-1); `trace` 0 ERROR, 16 KNOWN (PB-11), 75 WARN, NFR-1.8 allocated by the
architecture, 0 requirements without attribute.

### 8.1 Decisions raised by this pass

| # | Question | Options | Recommendation | Owner |
|---|---|---|---|---|
| D37 | Where is the owner's blueprint `underfloor_heating_blueprint_2.pdf`, the source of B-01…B-21, and which version was reviewed? It is not in the repository (QA-15). | A add the PDF to `docs/` / B record its location and version (file date, page count or SHA-256) in §4.5 | A — the rows can be re-checked against the same file | ✅ owner 01.11.2026: B. Open action: location and version, to be given by the owner (§4.5) |
| D38 | FR-60 on a controller with no mapped sensor (before commissioning, FR-11.1): "at least one advert from a mapped sensor within 60 s" can never pass, so every OTA would roll back. | A skip the sensor check while no zone has a mapped sensor / B accept an advert from any sensor / C keep: OTA only after commissioning | A | ✅ owner 01.11.2026: A (FR-60; loop / watchdog and Wi-Fi checks stay) |
| D39 | NFR-1.8 vs FR-17.2: a boiler OFF that is due (flow limit FR-54, deferred OFF FR-32) can wait up to 3 s for the radio cooldown when another frame (e.g. the FR-17.7 state repeat) was just sent, so the 2 s bound cannot hold for a radio frame. NFR-1.8 as approved names OTA as the only exception. | A the 2 s apply to the decision and to every relay; a radio frame may also wait for the FR-17.2 cooldown (worst case 2 s + 3 s) / B a safety OFF frame skips the cooldown / C shorter cooldown | A — the cooldown protects the receiver; the valves keep the 2 s | ✅ owner 01.11.2026: A (NFR-1.8); **superseded** 01.11.2026 by the 10 s bound (§9): the 3 s cooldown fits inside it |

## 9. Changes after closure

A requirement change after the review was closed gets a row here (`process.md` §3: "after any requirement change")
and a new baseline version; the review stays closed.

| # | Requirement | Change | Owner | Applied to |
|---|---|---|---|---|
| O-16 | NFR-1.5, NFR-1.8 | With the control tick in its own task (AR-21): the task watchdog covers every task that runs control or interfaces (`loop()` and the control task), either one hanging resets into the safe boot (NFR-1.4); the trigger of a manual request (FR-17.3) is its acceptance by the controller, the network before it is outside the bound; the bound is checked with the longest control-tick delay of FR-23; the OTA exception stays (flash writes stall both cores, boiler OFF per FR-60). **Safety.** | no preference — recommended option, 01.11.2026 | NFR-1.5, NFR-1.8; baseline v2.3 |
| C-1 | NFR-1.8 | One simple bound: every safety reaction has changed the outputs within **10 s** of its trigger (was 2 s); the only exception is a firmware update, which runs only with the boiler OFF (FR-60). The radio-cooldown clause of D39 is removed (the 3 s fit inside 10 s), so D39 is superseded. Kept from O-16: watchdog on both tasks (NFR-1.5), a manual request counts from its acceptance by the controller, verification with the longest control-tick delay of FR-23; FR-37 and FR-17.3 keep "at the next control tick". Reason: the owner wants one simple bound; the plant reacts in minutes, and since AR-21 control no longer depends on the web page. **Safety.** | ✅ owner 01.11.2026 | NFR-1.8; D39 superseded; baseline v2.4 |
| C-2 | FR-8.1 | The page also works behind a reverse proxy that serves the controller under a path prefix (e.g. `/heating/`): every URL the page requests is relative to the page address. Not tied to one server: today the owner uses a phone as home server, later another machine. Baseline v2 had dropped proxy access with FR-8.2 as user network setup; the network setup stays the user's, the page must not break it. Found as PB-15. | ✅ owner 01.11.2026 | FR-8.1; baseline v2.5 |
| C-3 | FR-8.1 | The family view header says in words what the schedule does now and when it changes next (`Program: zi până la 22:30, apoi noapte`), or why the house does not follow it; the bare `acum zi` was not understood. Found as PB-17. | ✅ owner 01.11.2026 | FR-8.1; baseline v2.6 |
| C-4 | FR-11.3, FR-8.1 | Commissioning from zero in the page: name, sensor, relay output, on / off per zone in the engineering view; a relay serves one zone and not the pump; a zone is switched on only with a sensor and a relay; the number of zones is the number switched on. While a valve is energised its relay is not changed and the zone is not switched off (safety review B1, R1: FR-3 until step 4). The owner treats the controller as blank: the zone data copied from the old software by step 1 was erased on the board (01.11.2026). FR-11.2 stays retired (no separate page). Found as PB-18. | ✅ owner 01.11.2026 | FR-11.3, FR-8.1; baseline v2.6 |
| C-5 | FR-8.4 (new) | A fixed list of zone states with code and Romanian text; the old firmware texts (`Idle (Temp OK)`) are gone. The codes are those of architecture §7 (`FROST`, `FAULT`, `WINDOW`, `HEATING`, `IDLE`, `DISABLED`) plus `ACQUIRING` (`Așteaptă senzorul`) for the first minutes after boot (owner, 01.11.2026). Found as PB-19. | ✅ owner 01.11.2026 | FR-8.4; baseline v2.6 |
| C-6 | FR-21 | Version format `1.MM.DD.NN` (month, day, count of firmware commits that day), e.g. `1.11.01.01`, computed from git. The owner had asked for it earlier; it never reached the requirements. Found as PB-20. | ✅ owner 01.11.2026 | FR-21; baseline v2.6 |
| C-7 | FR-10.4 | No health score: a percentage made of penalties per active DTC claimed health the checks never measured (no boiler, valve or radio check). Instead a list of checks, each with its rule, value and `OK` / `FAIL`, or `INFO` when it cannot be verified. Found as PB-21. | ✅ owner 01.11.2026 | FR-10.4; baseline v2.6 |
| C-8 | FR-8.1 | The admin view becomes an engineering view (like a CANoe / Trace32 watch window): every value the API reports, with its API name, an explanation and the requirement; changes highlighted; a change trace with time and CSV export, kept by the page. The family view stays simple. | ✅ owner 01.11.2026 | FR-8.1; baseline v2.6 |
| C-9 | FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8, FR-58, FR-8.1 | Configuration export / import moves from `[later]` (step 14) to `[now]`, as its own change before any migration step, so the owner can keep using the board while the software changes. FR-65 becomes the parent of seven leaves: one JSON file (`format` 1, firmware version) with every setting the user or installer made (stored §6 parameters under their `/api/config/system` names, zones, schedule and levels, location, learned duty); no credential, counter or state. Import applies each value that passes the check of the same setting set alone, skips unknown names and failed values, keeps missing ones, checks zones together (one relay per zone, not the pump, energised valve keeps relay and on state), reports applied / kept / skipped with the reason in the engineering view; not JSON or unknown format → 400. Derived, not separate owner decisions: from the compliance check (`sh-compliance-reviewer` 02.11.2026) import joins the FR-58 list, a credential in the file is skipped, size bound 16 384 bytes (413), only `application/json` (415), no cross-origin header, export always valid JSON and within the bound, NFR-1.8 kept; the 100 ms of NFR-2.3 does not apply to an import (answer within 2 s, NVS writes). Whether the export needs authentication stays with D49; the unbounded HTTP body of every POST is PB-56. After the requirements review (`sh-req-reviewer`) FR-65.8 (round trip) was added and the leaves were made exact; four detail choices were taken as recommended, owner may change them: the pump setting is checked before the zones; a repeated `id` skips every entry with it; names are 1-31 / 0-31 bytes UTF-8 without `"`, `\`, control characters (FR-11.3); whether an import creates a zone that does not exist waits for C-10 (until then `no-zone`). | ✅ owner 02.11.2026 (export / import now; apply what is valid and report the rest) | FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8, FR-58, FR-8.1; baseline v2.7 |
| C-10 | FR-69, FR-73, FR-74, FR-11.3, FR-8.1, §6 `number of zones` | A new controller has no zone (default 0, range 0-32). The user adds zones one at a time, only while a relay output is free (outputs minus zones minus the pump), and deletes them; deleting frees the relay, later zones move up, so numbers stay 1…N. A zone whose valve is energised cannot be deleted (FR-3, as it cannot be switched off). A controller that already has zones keeps them (the board's three). Switching a zone on or off no longer changes the number of zones. The owner thinks like a new user: zones are set up by the user, nobody supplies a mapping. Import creating missing zones is left for a follow-up (until then an import skips a zone number that does not exist, `no-zone`). **Safety (FR-3).** | ✅ owner 02.11.2026 | FR-69, FR-73, FR-74, FR-11.3, FR-8.1, §6; baseline v2.8 |
| C-11 | FR-11.3, FR-11.5, FR-11.6 | A zone's sensor is set by typing its MAC or from the list of a search the user starts (`Caută senzori`), until Stop and at most 10 min; the list shows MAC, temperature, battery, RSSI. Outside a search only the sensors assigned to zones are kept, so a zone never loses its sensor to 8 neighbours' sensors (PB-55, safety: the zone would have no frost protection). The search list holds the first 32 sensors heard. **Safety (FR-16.1, FR-51).** | ✅ owner 02.11.2026 | FR-11.3, FR-11.5, FR-11.6; baseline v2.8 |
| C-12 | FR-11.3, FR-65.5, FR-65.6 | Each zone had its own Save button, so two zones could not exchange sensors: the first save was refused because the sensor still belonged to the other zone (owner on the board, 02.11.2026: sensors assigned wrongly, could not be reassigned). The engineering view now has one button for all zones; the zones are checked together on the result (a sensor and a relay serve one zone), so an exchange passes; a zone that breaks a check is skipped with its reason and the rest are saved. The import (FR-65.5) gets the same rule for sensors, new reason `sensor-taken` (was `invalid`). | ✅ owner 02.11.2026 | FR-11.3, FR-65.5, FR-65.6; baseline v2.9 |
| C-13 | FR-65.1, FR-65.3 | The zone `target` in the settings file is dropped (PB-62): while the schedule is on the controller overwrites it every pass, so an import of it could not stay. What the owner needs is the day, night and away setpoint of every zone saved and restored after a test or a reflash ("să nu rămân în frig noaptea"); these are the `schedule.levels`, already in the file; a test now proves the restore. | ✅ owner 02.11.2026 | FR-65.1; baseline v2.10 |
| C-14 | FR-47, FR-8.1, FR-8.4 | Underfloor heating loses heat during the pause of the 60 min cycle (corner room, PB-67): while (target − room) >= 0.4 °C the duty is 100 % (no pause); below 0.4 °C the duty stays proportional with an outdoor compensation of +1.5 % per °C below 15 °C, at most 30 %, plus the learned duty. The page shows `Pauză TPI` instead of `Temperatură atinsă` while the room is more than 0.2 °C below target (PB-66); one search toggle; form edits are not overwritten by the refresh. **Safety (control law; FR-16.1 and FR-3 unchanged).** Decision record: `decisions.md` 03.11.2026. Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. | ✅ owner 03.11.2026 (chat) | FR-47, FR-8.1, FR-8.4; code `ThermalZoneManager.cpp`, `web/index.html` (a8b2d17); baseline v2.10 text |
| C-15 | FR-29, FR-30, FR-31, FR-8.1 | Schedule: up to 4 moments per day (P1…P4, names set by the user), a global Away temperature per zone, and 4 modes (Constant 24/7, Weekly unified, 5+2, 7-day individual); times in 24 h; the active moment is shown only for the current day group. Replaces the fixed day / night / away levels (PB-68, PB-69). Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. | ✅ owner 03.11.2026 (chat) | FR-29, FR-30, FR-31 (0008edf, 6856ac8); no baseline number was given then |
| C-16 | FR-35.1 (new), §6 | Gas meter index with calibration, calibration history, estimated gas used today / yesterday / this month, and the boiler flow setpoint. New parameters in §6: `boilerSetTemp` (40 °C, 25–60), `boilerFlowObserved` (43 °C, 25–65), `gasFlameRate` (0.60 m³/h, 0.20–2.50), `boilerPowerKw` (24 kW, 10–45), `gasPrice` (3.30 Lei/m³, 1.00–10.00). Estimate only; it does not drive the boiler. Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. | ✅ owner 03.11.2026 (chat) | FR-35.1, §6 (63accff, 4532f7b) |
| C-17 | FR-34, FR-35 | History chart: ranges 1 h, 12 h, 24 h, 3 d and 5 d (the text still names 24 h and 5 d only), valve activity per zone, restart markers, gaps drawn as gaps. Zone heating runtime today / yesterday on the zone card and in `/api/zones`. Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. Open: the ranges in FR-34 are not yet rewritten (PB row). | ✅ owner 03.11.2026 (chat) | FR-34, FR-35 (f0af40b, 6d084ff, 4532f7b); text change pending |
| C-18 | FR-40, FR-8.1 | The page shows an open-window pause on the room card (countdown and a cancel button, `POST /api/zones/window/clear`) and on the history chart. The detection compares with the peak of a sliding window instead of fixed 5 min blocks (code only; FR-40 text not changed: drop of at least the threshold within 5 min). Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. Open: the requirement text of the sliding window and of the display is not written (PB row). | ✅ owner 03.11.2026 (chat) | FR-40, FR-8.1 (a9c5175); text change pending |
| C-19 | FR-8.1 | Configuration import shows a preview first: the page compares the file with the current configuration, warns about values outside §6, and sends it only after the user confirms; each room card has vertical setpoint buttons. Found as PB-70 (limits taken from §6). Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. | ✅ owner 03.11.2026 (chat) | FR-8.1; baseline v2.11 (1f162c5, f5ccb5d, 04a2a84) |
| C-20 | FR-8.1, FR-10.2, FR-10.4, FR-45 | The engineering view has four collapsible groups, an active-DTC panel with cause text, a Clear DTC button and, since a9c5175, a live console that reads `/api/console/logs` and sends commands to `/api/console/cmd` (FR-45 names only `/api/console/send`). Written after the fact on 03.11.2026 by the process review of 31b8496..6170f16: the change was made in the requirements and the code outside the chain (another AI tool) and decided by the owner in chat 03.11.2026. **Safety / security:** the console endpoints need no authentication (see PB rows of the review `review_agy_03_11.md`); owner decision pending, no requirement written. | ✅ owner 03.11.2026 (chat) | FR-8.1, FR-10.2, FR-10.4, FR-45 (f75c4e0, 90a14dd, a9c5175); text partly written by the tool |
| C-21 | FR-58, FR-45, FR-7.1 | Console login: the HTTP console and Telnet need a login (the page gets a login step); the log never shows a password; only `POST /api/console/send` stays (`/api/console/cmd` removed); a security-event log (failed logins, console commands, time) is visible in the engineering view. Requirement text is written in its own round, checked by `sh-compliance-reviewer` (PB-43, PB-82, CMP-5, CMP-13). **Security.** | ✅ owner 03.11.2026 (chat, relayed by the main chat) | FR-58, FR-45, FR-7.1; text pending |
| C-22 | NFR-2.8 | The firmware limit 1.25 is in MiB (the OTA partition size), not 10^6 bytes. | ✅ owner 03.11.2026 (chat, relayed by the main chat) | NFR-2.8 |
| C-23 | FR-29, FR-31 | One schedule model with 4 moments; the old 8-slot list is removed; old export files are converted on first read (PB-73, PB-75, PB-92). Text pending. | ✅ owner 03.11.2026 (chat, relayed by the main chat) | FR-29, FR-31; text pending |
| C-24 | FR-34, FR-35, FR-40 | Requirement wording written from what the board does now (history ranges, zone counters, sliding window and window display); texts shown to the owner before the tests (PB-90). FR-40 window baseline stays 5 min (PB-72). | ✅ owner 03.11.2026 (chat, relayed by the main chat) | FR-34, FR-35, FR-40; text pending |
| C-25 | FR-35.1, FR-65.1, §6 | Gas meter readings are data, not a calibration (owner, chat 03.11.2026): every reading the user types is stored with its time in a LittleFS file that survives a flash, a reset and an import; never overwritten or dropped (1000 entries, then monthly compaction of entries older than 13 months, with an `INFO` in the self-test); only a typed reading writes an entry (PB-80 second half, PB-99); the page says `Citire contor`; no default reading. The out-of-range values of the gas endpoints are refused (PB-78). Open for the owner: the cap, the compaction rule and the optional reading time are proposals (recommended options). Changed 03.11.2026 (owner): the 13-month compaction is dropped (1000 entries are about 10 years of readings typed now and then); when the log is full the oldest entry is replaced and the self-test reports `INFO` with the number replaced. Added 03.11.2026 (owner): the old ring with its test entries is dropped at the first start of the new firmware (the owner's confirmed reading seeds the new log); no system test writes or leaves an entry or changes the stored reading (PB-87, PB-98). Text shown to the owner before the tests. | ✅ owner 03.11.2026 (chat, relayed by the main chat) for the principle; text pending his OK | FR-35.1; code and tests in the round of C-25 |
| C-26 | FR-35.1 | The burner flow is not typed by the user (owner, chat 03.11.2026: nobody knows it): it is measured from the readings and the burner runtime between them (rise of the index / burner hours, average of the last 10 intervals of at least 1 h, weighted by hours); until then the §6 default `gasFlameRate` is used; each entry also stores the burner runtime when it was typed. Owner 03.11.2026 (chat): let it correct itself; the user cannot enter `gasFlameRate` or `boilerPowerKw` at all (page and `/api/config/system` refuse them); the flow counts boiler ON hours, not flame hours. | ✅ owner 03.11.2026 (chat) | FR-35.1; code and tests in the round of C-26 |
| C-27 | FR-35.1 | The page showed 8.10 m³ of gas for today, from 13.5 boiler ON hours × the starting flow 0.60 m³/h, while the real consumption is about a third (owner, chat 03.11.2026): gas figures and costs are shown only once a flow is measured, otherwise `—` with a note; the page also showed `Timp ardere azi: 0.0h` (wrong source), now the boiler ON time of today. | ✅ owner 03.11.2026 (chat, the display question) | FR-35.1; page only |
| C-29 | FR-35.1 | The owner will not type past readings: the two readings he gave (2265.304 at 15:39 and 2265.996 at 21:59 on 03.11.2026) are all the data. The measured flow of an interval takes the boiler ON time from the telemetry history when a reading has no stored runtime: 24 h history (5 min steps, error at most 10 min) while both readings are under 24 h old, else the 5 day history (30 min steps, error at most 1 h, interval of at least 4 h ON time), only when the history is complete for the interval; otherwise no flow and the page says so. | ✅ owner 03.11.2026 (chat, relayed by the main chat); text shown to the owner first | FR-35.1; code and tests in the round of C-29 |
| C-28 | FR-35.1 | Correct a stored gas meter reading: proposed, then rejected (owner, chat 03.11.2026: the first reading was taken at 15:39, the board entry is right). No requirement, no code, no tests. The number is not reused. | ✅ owner 03.11.2026 (chat): rejected | none |
| C-30 | FR-75 (new, FR-75.1 ... FR-75.5), FR-76, FR-77, FR-7.3, FR-45, FR-58, FR-59, §6 | Console login (owner, chat 03.11.2026, decisions D40 option C, D42 Telnet stays): commands over HTTP and Telnet need a password; the first password is set from Serial and must be changed at the first network login (no fixed `admin` / `admin123` for the console); lockout after `loginAttempts` failures; `/api/console/cmd` removed; the console log never shows secrets or the coordinates of the house; a security-event log (failed and successful logins, lockouts, setting and output commands by name, password change, factory reset) shown in the engineering view. **Reading the log and the events stays public on the LAN (owner)**: `sh-compliance-reviewer` 03.11.2026: depends on the risk assessment (CMP-1), to be accepted there; owner answers 03.11.2026 (D49 addendum): client address shown as `a.b.c.x`; on the setup access point reading the log and the events needs the password too (public only on the home network); tests get the password from `SH_CONSOLE_PASSWORD` and skip without it; plain text on the LAN is noted in the risk assessment. OTA stays on its own credentials until D40 / step 9 (DTC-403). **Development phase (owner 03.11.2026):** the owner has no USB cable on the board and works over the network, so a development build (profile `esp32dev`, flag `SH_DEV_OPEN`) keeps console, Telnet and `/update` open, allows the first password over the network and shows a banner; the release profile `esp32release` enforces the rules as written; the release check fails while the open mode can reach a release (FR-75.6, PB-104). **Security.** | ✅ owner 03.11.2026 (chat) for the principle; text shown to the owner | FR-75, FR-76, FR-77; code and tests in the round of C-30 |

| C-31 | FR-53, FR-16.1, §6 | The §6 ranges of `validMin` (0–10 °C) and `frostTemp` (8–16 °C) overlap: a reading between them is ignored as out of bounds and frost protection can never start (finding 4 of the safety review of the group of steps 6, 10, 8, 11, 04.11.2026). Rule: `validMin` stays below `frostTemp`; a setting that breaks it is refused for every source. | ✅ owner 04.11.2026 (chat, D-52) | FR-53; code and test TC34.6 |
| C-32 | NFR-2.8 | The firmware limit is the real OTA slot (0x1E0000 = 1 966 080 B, `architecture.md` §3) minus a reserve of 100 KiB = 1 863 680 B; the 1.25 MiB of C-22 was the old default slot and a habit (memory review, 04.11.2026). | ✅ owner 04.11.2026 (chat, D-53) | NFR-2.8; AGENTS.md §3; `architecture.md` §14.1, AR-18; plan R-2; checker |
| C-34 | NFR-2.8 | Supersedes C-32: the board still has the old partition table, its OTA slot is 1 310 720 B and an update is accepted up to 1 306 624 B (the upload of 1 307 840 B was refused: Not Enough Space, 04.11.2026). The limit is 1 306 624 B with a working margin of 20 KB; a bigger slot needs a USB reflash (erases LittleFS), the owner's later decision. | ✅ owner 04.11.2026 (no USB in development, via the main chat, D-54) | NFR-2.8; AGENTS.md §3; `architecture.md` §14.1, AR-18; plan R-2; checker |
| C-33 | FR-10.6 (new) | `GET /api/diag/mem` stays permanently in Diagnostics (read only): heap minimum since boot, largest free block, stack minimum of the known tasks; a heap line in the debug area. Needed to hold NFR-2.2 (PB-115). | ✅ owner 04.11.2026 (chat, D-53) | FR-10.6; `api_reference.md`; page |

Checker after O-16 (baseline v2.3) and after C-1 (baseline v2.4): `reqs` 0 ERROR, 14 WARN (DTC never raised, code
side), 1 waived (W-1); `trace` 0 ERROR, 16 KNOWN (PB-11), 75 WARN.

## 10. EU conformity review (02.11.2026)

First full run of `sh-compliance-reviewer` against `docs/compliance.md` (the owner sells the device in the EU
under their brand). The review of baseline v2 stays closed: a row decided here becomes a `C-n` row in §9 and a new
baseline version (`process.md` §3). Code gaps are PB-43 ... PB-54 in `docs/problems.md`.

### 10.1 Findings

| ID | Finding | Category | Proposal | Safety |
|---|---|---|---|:-:|
| CMP-3 / FR-59 | FR-59 and §6 allow universal default credentials (OTA, access point, Wi-Fi); the EU rule is no universal default password. Code: PB-53. | conflict | rewrite FR-59 by D40 |  |
| CMP-14 / FR-66 | Factory reset keeps the Wi-Fi credentials and is `[later]` (after step 14); the EU rule is that all data and settings can be removed, at the first sale. | conflict | rewrite FR-66, phase `[now]`, by D44 |  |
| CMP-11, CMP-6 / FR-7.1 | Telnet server on port 23: plain text, no requirement authenticates it. | conflict | by D42 |  |
| CMP-5 / FR-58 | Comfort settings (including away and schedule, which show when nobody is home) and every read need no authentication. | conflict | by D49, after the risk assessment (CMP-1) |  |
| CMP-4 / §1 | "Out of scope: cloud services" vs security updates automatic by default where applicable, with the user told. | conflict | by D43; whether "applicable" - consultant |  |
| CMP-5 / FR-58 | FR-58 does not refuse cross-site writes (CORS stops neither a form POST nor a GET with an effect), does not report failed authentication, and does not say how the HTTP and Telnet console authenticate protected commands. Code: PB-44, PB-46. | missing | extend FR-58 | ✓ |
| CMP-11 / FR-50 | No requirement lists the only services and ports; FR-50 does not say when the setup access point closes or which secret it uses; undocumented endpoints exist. Code: PB-43, PB-51. | missing | new requirement; FR-50 gets a closing rule (Wi-Fi joined or a timeout) and a per-unit secret | ✓ |
| CMP-13 | Security events (failed authentication, setting and credential changes, OTA, factory reset) are not recorded; FR-62 covers safety events only; the log can be forged and cleared. Code: PB-51. | missing | new requirement, with an opt-out |  |
| CMP-9 | No bound on input size and request rate per interface; flooding is not kept away from the control tick (NFR-1.8); the sensor table can lose the mapped sensors. Code: PB-48, PB-50, PB-49, PB-47. | missing | new requirement | ✓ |
| CMP-8 / FR-9.2 | No requirement lists the data collected, the data that leave the controller (weather location, time servers) and how long history and foreign sensors are kept. Code: PB-54. | missing | new requirement; D48 |  |
| CMP-7 / FR-53, FR-55 | Room readings are not authenticated (encryption is only "optional later"); boiler frames can be replayed if the product ships its own receiver. | missing | D45 | ✓ |
| CMP-6 | No requirement keeps credentials and settings from being sent or logged in clear, or protects stored secrets against read-out. | missing | D41 |  |
| CMP-4, CMP-26 / FR-8.3, FR-60 | No requirement accepts only images signed by the manufacturer or tells the user that a security update exists. Code: PB-52. | missing | D43 |  |
| CMP-40, CMP-41 | No requirement records the software class of each safety function, or the Annex H measure and safe state per fault class (CPU, program flow, interrupts, clock, memories, communication, I/O). | missing | D47 | ✓ |
| CMP-50 / FR-17.2 | No requirement bounds the worst-case on-air time of the 433.92 MHz transmitter from every source, forced OFF included, to the band limit. Code: PB-47. | missing | new requirement, METHOD `analysis` plus the lab's limit |  |
| CMP-51 | No requirement sets the EU Wi-Fi regulatory domain or keeps power and channels inside the radio module's certificate. | missing | new requirement |  |
| CMP-2, CMP-20, CMP-21, CMP-22 | No process rule for an SBOM and a vulnerability check per release, a security test per release, or a security fix outside the migration step (PB-1: "no hotfix" will not hold after a sale). | missing (process) | `docs/process.md`; D50 |  |
| CMP-23 ... CMP-31 | Owner documents: disclosure policy and contact, free updates, support period, ENISA reporting, declaration of conformity and technical file, user information. | missing (owner) | `docs/compliance.md` §4; D46 |  |

### 10.2 Decisions for the owner

| # | Question | Options | Recommendation | Owner |
|---|---|---|---|---|
| D40 | CMP-3 / FR-59: how does each unit get its credentials, with no universal default? | A a unique secret per unit, printed on the label (factory) / B none until the user sets them at first setup / C A, and the user is asked to change it at first setup | A - heating works out of the box (frost protection needs no setup) | open |
| D41 | CMP-6: encryption on the home LAN. | A HTTPS on the controller (heap, NFR-2.2, plan risk R-1) / B plain text on the LAN, risk accepted in the risk assessment (CMP-1) / C B, plus flash encryption set at the factory (irreversible) | decide after the risk assessment (CMP-1) | open |
| D42 | CMP-11 / FR-7.1: the Telnet console. | A removed (Serial and the authenticated HTTP console stay, FR-45) / B off by default, switched on by an authenticated command / C kept | A - one port less, FR-45 covers the use | open |
| D43 | CMP-4, CMP-26: security updates without a cloud. | A signed images on a download page, the user uploads them / B the controller checks a published list and tells the user, who installs / C B, installed automatically; with A-C: signed images and secure boot set at the factory (irreversible) | B - the user is told, no account; consultant confirms whether automatic is "applicable" | open |
| D44 | CMP-14 / FR-66: factory reset. | A one full erase (data, settings, credentials) / B two levels: settings reset (keeps Wi-Fi) and full erase; either way FR-66 moves to `[now]` | B | open |
| D45 | CMP-7 / FR-55: does the product ship its own boiler receiver, and does it authenticate frames? | A own receiver with authenticated frames / B the boiler's original receiver (frames can be replayed, risk recorded in CMP-1) | decide with D19 (receiver hardware) | open |
| D46 | CMP-28: support period (security updates) promised to the buyer. | A 5 years / B the expected life of a heating controller (longer) / C another period | A at least; consultant confirms | open |
| D47 | CMP-40, CMP-41: software class of the safety functions and Annex H measures. | A ask the lab before migration step 4 (`SafetySupervisor`) / B assume class B now and add the measures to steps 4 and 12 | A, then B with the lab's answer; hardware measures (relay read-back, independent clock) may follow | open |
| D48 | CMP-8 / FR-9.2: precision of the location sent to the weather service. | A full precision / B rounded to about 10 km / C rounded to about 1 km | B - the weather grid is coarser | open |
| D49 | CMP-5 / FR-58: reads (temperatures, presence, away) and comfort settings without authentication. | A kept open on the LAN, argued in the risk assessment / B away and schedule need authentication, the rest stays open / C every read and write needs authentication | decide after the risk assessment (CMP-1) | open |
| D50 | CMP-21: a security fix after a sale. | A may go ahead of the migration step, on its own branch, through the same gates / B waits for its step | A | open |
