# Changelog: Sistem Inteligent de Încălzire (Smart Heating)

Jurnalul modificărilor, deciziilor tehnice și verificărilor efectuate pentru proiectul de încălzire inteligentă.
Corelat cu planul activ: [`docs/plan.md`](file:///c:/Users/BSeceleanu/.github_repos/smart-heating/docs/plan.md).
Entries before 29.10.2026 (the system before the lean clean-up):
[`archive/changelog_until_2026-10-21.md`](archive/changelog_until_2026-10-21.md).



## [03.11.2026] - 24h telemetry history chart (TPI timeline & temperature curves) and card subtitle cleanup (HEAD)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested optimizing room cards by removing redundant target setpoints, clarifying humidity percentages as 💧 X% RH, formatting TPI heating status in understandable terms (Încălzire TPI · X min/h (Y%) and Pauză ciclu (menține din șapă)), and adding a full-width 24-hour history chart showing burner and valve binary activations labeled by configured zone names, outdoor and zone temperature curves, and interactive cursor inspection.
- **Where:**
  - web/index.html:
    - Cleaned room card subtitles: removed redundant target setpoints (already displayed on steppers), formatted humidity clearly as 💧 X% RH, formatted boost as Boost manual.
    - Humanized TPI cycle status: Încălzire TPI · X min/h (Y%), Încălzire continuă (100%), Pauză ciclu (menține din șapă), Temperatură atinsă.
    - Added dedicated collapsible section <details class=adm-card id=grpHistory> containing full-width responsive SVG 24h chart with burner activity, dynamic zone valve lanes labeled with actual zone names (Living, KidsDorm, etc.), 24h temperature curves, current-time indicator line ("acum"), and interactive hover inspection tooltip.
    - Optimized chart polling: fetches pi/history?range=24h only when grpHistory is expanded, throttled to at most once per minute.
  - docs/requirements_and_features.md: updated FR-8.1 and FR-34 to formally document card subtitle formatting and 24h telemetry chart.
  - 	ools/sh_tests/t_ui_history.py & 	ools/sh_test_system.py: added ASPICE SWE.5 test SWE.5-TC19.16.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Zone heating runtime counters (today/yesterday) & collapse all admin sections (HEAD)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested that Confort & Programare also be minimized by default in the admin panel, and requested per-zone active heating runtime counters (today / yesterday) so the homeowner can see exactly how long each zone called for heat.
- **Where:**
  - `include/ThermalZoneManager.h`: added `_zoneRuntimeTodaySec`, `_zoneRuntimeYesterdaySec`, `_zoneRuntimeMsAcc`, getters and `updateRuntimeDay()`.
  - `src/ThermalZoneManager.cpp`: accumulated exact relay engagement duration per zone in `update()`; added rollover on day change.
  - `src/main.cpp`: invoked `thermalZones.updateRuntimeDay(timeAndWeather.getDay())` on daily sync.
  - `src/WebApi.cpp`: exposed `runtimeTodaySec` and `runtimeYesterdaySec` in `/api/zones`.
  - `web/index.html`: closed `grpSched` by default (all 4 admin sections now collapsed initially); added `zones[].runtimeTodaySec` / `yesterdaySec` in `doc`; rendered runtime badge (`⏱ Încălzit azi: Xh Ym (ieri Ah Bm)`) on zone cards and in the diagnostic `relee` breakdown.
  - `docs/requirements_and_features.md`: updated FR-35 to cover zone-level runtime counters.
  - `tools/sh_tests/t_ui_history.py` & `tools/sh_test_system.py`: added ASPICE SWE.5 test `SWE.5-TC19.15` and updated `TC19.14`.
- **Not changed:** hydronic safety invariants, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Reorganize admin in 4 collapsible groups, clear DTC button & polling throttle (90a14dd)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested reorganizing the long admin panel into minimizable collapsible sections, where by default all sections are collapsed except Confort & Programare; requested a button to clear DTC codes with history; and asked whether collapsed debug/trace sections still poll the ESP32.
- **Where:**
  - `web/index.html`: reorganized `<div id="admin">` into 4 sleek collapsible `<details class="adm-card">` containers: (1) Confort & Programare (open by default), (2) Sănătate & Diagnoză (with `[ Curăță codurile de eroare ]` calling `/api/diag/clear` and session history dropdown), (3) Configurare Instalație (Zone, Senzori, Sistem), (4) Instrumente Inginerie & Debug (Trace & Semnale); throttled `refresh()` polling so when debug is collapsed, heavy endpoints (`DOC.endpoints`) are bypassed and `updateWatch()` is skipped, saving 50% Wi-Fi requests and CPU cycles.
  - `tools/sh_tests/t_ui_history.py` & `tools/sh_test_system.py`: added ASPICE SWE.5 test `SWE.5-TC19.14`.
- **Not changed:** hydronic safety invariants, backend endpoints.

## [03.11.2026] - Day-group aware schedule highlighting, clear check conditions & active DTC panel (f75c4e0)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that weekend moments were falsely highlighted as active on weekdays (Tuesday) in 5+2 schedule mode; Constant 24/7 mode displayed obsolete moment indices (Activ P2) in the schedule card badge; the FR-10.4 checks table showed confusing raw DTC codes in the rule column; and requested operational status rows for actuators/relays and boiler state alongside a dedicated active DTCs panel with human-readable causes.
- **Where:**
  - `src/ThermalZoneManager.cpp`: added `dayOfWeek` index (`0..6`, 0=Sunday) to `/api/schedule` payload.
  - `web/index.html`: made `renderMomentsTableHtml` day-group aware (`groupIsActive` checks `isWknd` for 5+2 and matching day of week for 7-day tabs); updated schedule card badge to display `Constant 24/7` in constant mode; clarified `CHECK_RO` acceptance rules as explicit operating conditions; added operational checks for boiler (cazan ON/OFF/burn sec/protection) and relays (circuite deschise/închise); added `#dtcSection` with reassurance card when 0 DTCs are active and detailed cause table with `DTC_INFO` when DTCs occur.
  - `docs/requirements_and_features.md`: updated FR-31, FR-10.2, FR-10.4 to reflect day-group aware highlighting, active DTC panel, and operational checks.
  - `tools/sh_tests/t_ui_history.py` & `tools/sh_test_system.py`: added ASPICE SWE.5 tests `SWE.5-TC19.12` and `SWE.5-TC19.13`.
- **Not changed:** hydronic safety limits, burner minimum run/pause, actuator interlock.

## [03.11.2026] - Clear obsolete report card & clarify schedule level names in import diff (e2536da)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that the previous green import result card persisted on screen when clicking export/import or selecting a new file; additionally, schedule level diffs appeared ambiguous/duplicated (e.g. 'Living › Țintă day' vs 'Living › Țintă away') because the schedule context was not explicitly specified.
- **Where:** web/index.html (cleared #cfgReport and #cfgPreview on cfgExport, cfgImportBtn, cfgFile.onchange, and btnCancelImport; disambiguated schedule levels in analyzeImport with self-explanatory names: 'Temperatură Program Zi (Day)', 'Temperatură Program Noapte (Night)', 'Temperatură Mod Plecat (Away)').
- **Not changed:** hydronic safety limits, backend endpoints.

## [03.11.2026] - Clean import result card, value changes table & collapsible technical details (7eae5ee)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that the post-import report dumped an unformatted, raw wall of 34 parameter names without values ('Aplicat: params.actuatorDelaySec, params.pumpOverrunSec...').
- **Where:** web/index.html (updated renderImport to display a sleek green result card, a clear table of actual modified settings with friendly names and before/after values, skipped parameters with reasons, and tucked the raw 34 technical API keys into a collapsible <details> dropdown).
- **Not changed:** ASPICE SWE.5-TC29.27 compliance (reads applied, kept, skipped, reason), backend import API.

## [03.11.2026] - Align import preview limits with §6, document & test UI additions (PB-70, f5ccb5d)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner reported that exporting and re-importing the same configuration flagged a false warning on 'params.rfSafetyHeartbeatSec (900)' out of range [10..300], and noted process drift (requirements and tests were not kept in sync with recent UI additions).
- **Where:** docs/problems.md (recorded PB-70), docs/requirements_and_features.md (updated FR-8.1 for pre-import diff preview/confirmation and vertical zone card setpoint steppers, baseline v2.11), web/index.html (aligned PARAM_LIMITS with requirements §6: rfSafetyHeartbeatSec 30..900, pumpOverrunSec 180..600, burnerMinRun/Pause 120..600, frostTemp 8..16, sensorTimeoutSec 60..3600, windowThreshold 0.8..2.0), tools/sh_tests/t_export_import.py (added SWE.5-TC29.38 testing pre-import preview, confirmation buttons, and §6 bounds), tools/sh_tests/t_ui_history.py (added SWE.5-TC19.11 testing vertical room card steppers and room-main quick view).
- **Not changed:** hydronic safety limits, backend endpoints.

## [03.11.2026] - Pre-import diff preview, validation warnings & confirmation dialog (1f162c5)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested that configuration import ('Import setări…') not apply immediately upon file selection; instead, the UI must preview changed values (old vs new), highlight any invalid or out-of-range parameters, and ask for explicit user confirmation before executing the import.
- **Where:** web/index.html (analyzeImport helper comparing uploaded JSON against current config export, #cfgPreview diff table, invalid parameter warning box with Romanian explanations, [ Confirmă și aplică importul ] and [ Anulează ] interactive buttons).
- **Not changed:** hydronic safety limits, backend /api/config/import endpoint and verification logic.

## [03.11.2026] - Fix JS syntax & implement vertical setpoint controls on zone cards (04a2a84)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner reported that removing legacy slots inadvertently left an unclosed brace in syncSchedInput causing a JS SyntaxError; additionally requested that setpoint controls be visible on all zones without clicking and positioned vertically on the right of each zone card to maximize quick-view space below.
- **Where:** web/index.html (restored missing closing brace in syncSchedInput, updated .room to flex row with .room-main and vertical .ctl, widened MAX_T to 30°C, always-visible [ + ] [ val ] [ - ] stepper on every card with ↺ prog boost reset).
- **Not changed:** hydraulic safety invariants, backend endpoints.

## [03.11.2026] - Remove legacy compatibility details and quick-away button (f7ad961)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner confirmed that the obsolete '<details> Avansat / Diagnostic compatibilitate' section and the 'Setare rapidă toate camerele' quick-away helper are unnecessary and clutter the UI.
- **Where:** web/index.html (removed legacy details block, removed quick-away input/button, cleaned up dead slot parsing/rendering JavaScript handlers).
- **Not changed:** backend schedule logic, Away mode per-room target temperature inputs.

## [03.11.2026] - Fix schedule mode & tab switching blocked by refresh guard (d366a91)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that clicking schedule mode buttons (such as 'Zilnic unificat') or day tabs had no effect. Root cause: the isEditing guard inside 
enderSchedule() was returning early on user click events when schedDirty was true. Fixed by moving background refresh guard to 
efresh() via isSchedBusy(), allowing user click actions to immediately update view state while keeping background refreshes from disturbing active typing.
- **Where:** web/index.html (isSchedBusy() helper, 
enderSchedule() early return removed, 
efresh() guarded, ocusout debounced).
- **Not changed:** firmware logic, hydronic safety invariants.

## [03.11.2026] - Schedule form refresh overwrite protection & customizable moment names (C-16, PB-69)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that background 2s polling refreshes reconstructed `c.innerHTML`, destroying active form input focus and reverting uncommitted values before saving; additionally requested that moment names (P1: Trezire, etc.) be user-customizable.
- **Where:** `FR-8.1`, `FR-29` (baseline updated), `include/ThermalZoneManager.h` (`momentNames` array in `ScheduleConfig`), `src/ThermalZoneManager.cpp` (`getMomentName`/`setMomentName`, `getScheduleJson` serialization), `src/WebApi.cpp` (`POST /api/schedule/names`), `web/index.html` (strict `renderSchedule()` guard when editing or dirty, immediate input event synchronization, editable `.mom-name` fields), `tools/sh_tests/t_control.py` (`SWE.5-TC16.7` custom moment names, `SWE.5-TC16.8` refresh dirty protection).
- **Not changed:** hydronic safety invariants and schedule evaluation logic.

## [03.11.2026] - Honeywell 4-Moment Scheduling, 4 Operating Modes & Configurable Away (C-15, PB-68)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner requested an intuitive multi-moment schedule model inspired by Honeywell T6/Evohome with 4 modes (Constant 24/7, Unified L–D, 5+2 Weekday/Weekend, 7-Day individual) supporting up to 4 daily moments (P1 Trezire, P2 Plecat/Zi, P3 Seară, P4 Somn) with direct per-zone target temperatures, user-configurable Away temperatures per room with quick global apply, and clean 24h UI eliminating AM/PM browser confusion and raw token mismatches.
- **Where:** `FR-29`, `FR-31` (baseline updated), `include/ThermalZoneManager.h`, `src/ThermalZoneManager.cpp`, `src/WebApi.cpp`, `src/ParamStore.cpp` (dynamic comparison buffer for blobs up to 1984 bytes preserving write-on-change wear leveling), `web/index.html` (Honeywell UI, mode pills, 24h inputs, copy helpers, away card, undo/save, collapsible legacy compatibility block), `tools/sh_tests/t_control.py` (`SWE.5-TC16.6` multi-moment matrix & modes roundtrip test).
- **Not changed:** hydronic safety invariants (interlockDelay 180s, pump overrun 180s, anti-short-cycling, frost guard). Full backwards compatibility maintained for legacy `/api/schedule` endpoints, slots, and level setpoints.

## [03.11.2026] - Underfloor thermal deficit full duty, weather compensation & UI ergonomics (C-14, PB-66, PB-67)

- **Who / when / why:** firmware session (light mode), 03.11.2026; the owner observed that corner rooms with high heat loss (Adults bedroom) remained cold due to rigid TPI duty (~33 % for 0.5 °C error shutting off after 20 min, dissipating screed momentum) while the UI falsely reported "Temperatură atinsă"; plus UI inputs were wiped by polling refresh before saving, and separate sensor search buttons were confusing.
- **Where:** `FR-47` (deficit rule: 100 % duty when error >= 0.4 °C; weather compensation up to 30 % when outdoor < 15 °C), `FR-8.1`, `FR-8.4` (accurate idle status text distinguishing TPI pause vs target reached, 2-in-1 sensor toggle, dirty tracking with undo for schedule/levels, dynamic visual schedule moments with Away last), `src/ThermalZoneManager.cpp`, `web/index.html`, test `test_tc11_5_tpi_deficit_duty` in `tools/sh_tests/t_control.py`.
- **Not changed:** hydronic safety invariants (interlock delay, pump overrun, burner anti-short-cycling).

## [03.11.2026] - `OutputBank`, second half of step 3 (R-A1, FR-15.1)

- **Who / when / why:** firmware session (Claude), 03.11.2026; migration step 3 as decided by the owner.
- **Where:** new `include/OutputBank.h`, `src/OutputBank.cpp`: pin mode, pin level and the NC / NO polarity of the
  zone valves only there; `ThermalZoneManager::applyRelay` chooses valve or pump/free output and calls it; the
  actuator type is held by `OutputBank`; `architecture.md` §3.2, `plan.md`.
- **Not changed:** behaviour. No new tests: the existing tests of FR-15.1, FR-43 and NFR-1.4 cover it (light mode).
  Not built here: logical channel map, expansion backend, energised-since (later steps).

## [02.11.2026] - Zone target out of the settings file (C-13, PB-62)

- **Who / when / why:** firmware session (Claude), 02.11.2026; the owner: what must survive a test or a reflash is the
  day, night and away setpoint of every zone, so they can re-import it and not be cold at night.
- **Where:** FR-65.1 (baseline v2.10), `ConfigTransfer.cpp` (no `target` in the zone entries; an old file with it skips
  it as unknown), `api_reference.md`; tests TC29.36 (no target in the file), TC29.37 (setpoints restored from an
  earlier export), TC29.1, TC29.5, TC29.9, TC29.18, TC30.5 follow. Test infrastructure from the split of the test
  script: TC29.16 (`_orig_post` import) and TC15.1 (repository root) fixed.
- **Not changed:** the day, night and away levels were already in the file (`schedule.levels`).

## [02.11.2026] - One Save for all zones (C-12)

- **Who / when / why:** firmware session (Claude), 02.11.2026; the owner on the board: sensors assigned to the wrong
  zones could not be moved, each zone had its own Save and the old zone still held the sensor.
- **Where:** FR-11.3, FR-65.5, FR-65.6 (baseline v2.9); `ConfigTransfer.cpp` checks a sensor MAC on the resulting
  configuration (new reason `sensor-taken`); `web/index.html`: one button `Salvează zonele` sends all zones to
  `api/config/import`, a skipped zone shows its reason in its row; `api_reference.md`; tests TC29.34, TC29.35, TC30.12.
- **Not changed:** single-zone `map-zone` (API, console) keeps its strict check. Light mode, no safety run: no
  valve, boiler, frost or sensor-timeout logic changed.

## [02.11.2026] - Process: model and effort per work, reading budget

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner: too many tokens, also in the main chat.
- **Where:** `model` / `effort` in every agent's frontmatter: `sh-safety-reviewer` Opus high; test writer and
  reviewer, code, requirements and compliance reviewers Sonnet medium; `sh-assessor` Haiku low. `sh-write-tests`
  and `sh-review` pass Opus high for tests of the safety list and the full EU review. Every agent has a
  "Reading budget" (grep, then read by section). AGENTS.md §5 "Model per work" table for the main chat and the
  sessions it starts. Field names checked in the Claude Code docs (sub-agents, skills, settings).
- **Not changed:** the model of the main chat and the project's default model are the owner's settings.

## [02.11.2026] - Zones added and deleted by the user, sensor search (C-10, C-11), first half of step 3

- **Who / when / why:** firmware session (Claude), owner decisions 02.11.2026: a new controller has no zone, the user adds
  and deletes zones and picks sensors by MAC or by a search; fixes PB-55 (sensor table filled by neighbours' sensors).
- **Where:** requirements baseline v2.8 (FR-69, FR-73, FR-74, FR-11.3, FR-11.5, FR-11.6, FR-43); `ThermalZoneManager`
  (32 slots, `addZone`, `deleteZone`), `ParamStore` (zone count, records read from NVS), `BleSensorScanner` (search,
  assigned sensors kept), `WebApi` (zone-add, zone-delete, sensor-search, relay test per FR-43), page, API reference.
  A controller that has zone records keeps its three zones; the old schedule is converted.
- **Board:** flashed, system test 123 pass, 0 fail, 3 skip; free heap 102 KB. Review: `docs/review_step_3.md`.
- **Not done:** `OutputBank` module, assessor and compliance review of the step, owner test.

## [02.11.2026] - Configuration export / import (FR-65, change C-9)

- **Who / when / why:** firmware session (Claude), owner decision 02.11.2026: keep using the board while the software
  changes. Own change before migration step 3 (order: this, step 3, step 2).
- **Where:** requirements baseline v2.7 (FR-65.1 ... FR-65.8, FR-58, FR-8.1, NFR-2.3, FR-11.3 name limits);
  `src/ConfigTransfer.cpp` (new), `WebApi.cpp` (`GET /api/config/export`, `POST /api/config/import`), `ThermalZoneManager`
  (`getLearnedDutyPct`, `setLearnedDutyPct`; `/api/zones` `learnedDuty` now one decimal), `web/index.html` (export /
  import buttons, import report, doc table), `docs/api_reference.md`, `docs/architecture.md`, `docs/compliance.md`.
  Tests TC29.1 ... TC29.33 written first, red on 2b7564f (33 of 33), commit e73fdb6.
- **Problems:** PB-55 (sensor table), PB-56 (unbounded HTTP body), PB-57 ... PB-61 from the safety review (heap growth of
  one import, valve-moving settings while valves are open, loop timing, frost on a disabled zone, tiny learned duty).
- **Board:** flashed ce8a58b (1.11.02.03); system test 112 PASS, 1 FAIL (TC29.2, zone target, PB-62), 2 SKIP (TC29.15,
  TC29.33: no learned duty above 0 % on the board); round trip export -> import -> export checked by hand: identical.
  Board check without error. Owner acceptance pending.

## [02.11.2026] - EU conformity: first full review logged

- **Who / when / why:** `sh-compliance-reviewer` (scope `all`), logged by the process chat with the owner's OK
  ("Da, trece-le"). Main claims checked against the code before logging.
- **Result:** 31 obligations: 12 partial, 19 missing, none covered. `docs/compliance.md` §3 statuses filled, §2
  GDPR row corrected (location and time requests leave the device), §4 WEEE and GPSR rows added.
- **Problems:** PB-43 ... PB-54 (4 safety: setup access point never closes, no authentication, relay test in any
  mode, BLE sensor table can lose the mapped sensors). **Requirements:** `docs/requirements_review.md` §10, 18
  rows (5 conflicts, 13 missing) and owner questions D40 ... D50.
- **Not done:** nothing fixed; the decisions are open. A duplicate of PB-43 ... PB-54 written by a re-run of the
  logging script was removed before this entry (never committed).

## [02.11.2026] - Process: step assessor; EU conformity supervisor

- **Who / when / why:** process chat (Claude), 02.11.2026. The chain checks itself, nobody checked its evidence;
  the owner wants to sell the device in the EU under their brand, without third-party certification.
- **Where:** `.claude/agents/sh-assessor.md` (evidence of a step, SUP.1); `docs/compliance.md` (EU acts,
  obligations `CMP-n` on the software, technical file - all `open`) and `.claude/agents/sh-compliance-reviewer.md`;
  `sh-change` runs both at the end of a step and the compliance check on security-related requirement changes;
  `sh-write-tests` commits the frozen tests before the code; `sh-review` args `assess`, `compliance`; AGENTS.md
  §2 (prepared for sale in the EU, no shop), §5; `docs/process.md` §1, §3.1, §9; `docs/plan.md` out of scope.
- **Not verified:** neither agent has run yet; `docs/compliance.md` is not legal advice and needs a consultant or
  lab before the first sale.

## [02.11.2026] - Process: short logs, short checker output

- **Who / when / why:** process chat (Claude), 02.11.2026, owner's choice "Jurnale scurte" and "Verificare pe
  scurt": every chat read long logs and a 126-line checker report, most of it about a system that no longer exists.
- **Where:** entries before 29.10.2026 moved unchanged to `docs/archive/changelog_until_2026-10-21.md` (changelog
  1743 -> 369 lines); decisions of 2026-10-06 ... 2026-10-22 to `docs/archive/decisions_until_2026-10-22.md` (539 ->
  392 lines); `activeContext.md` keeps only "Now" and "Next" (57 -> 21 lines), its old lines in
  `docs/archive/active_context_until_2026-11-02.md`; deleted with the owner's OK: `productContext.md`,
  `progress.md`, `projectbrief.md`, `systemPatterns.md`, `techContext.md` (pointers or stale, read by no tool).
  `tools/sh_process_check.py`: the default run prints a WARN count per section (126 -> 24 lines); a named check,
  `--full` and CI list every WARN. AGENTS.md §5, `docs/process.md` intro and §3, `sh-verify-firmware` step 2.
- **Verified:** checker PASS, same totals in short and full mode (0 errors, 1 known, 104 warnings, 1 waived).

## [02.11.2026] - Process: the owner's chain (`sh-change`)

- **Who / when / why:** process chat (Claude), 02.11.2026. The owner cannot review code and tests or run the
  process step by step; the tools existed, but nothing ran them in order.
- **Where:** `.claude/skills/sh-change` (log → requirement → tests first → code → gates → commit → board → owner
  test; the owner only tells, decides a requirement change, tests on the board); AGENTS.md §5 (chain rule, commit
  inside the chain without asking, "one chat, one zone" removed); `docs/process.md` §1, §5; `sh-verify-firmware`
  and `sh-write-tests` hand over to the chain; decision in `.agent/memory-bank/decisions.md`.
- **Not verified:** the chain has not run yet.

## [02.11.2026] - Process: tests first in a clean room, one reviewer per work product

- **Who / when / why:** process chat (Claude, branch `claude/vibrant-ride-95fick`), 02.11.2026, asked by the owner:
  tests must check the requirements, not adapt to the code; a failing test fixes the firmware, not the test.
- **Where:**
  - `.claude/agents/`: `sh-test-writer` and `sh-test-reviewer` (clean room: a hook blocks reads of `src/`,
    `include/`, `web/`, `lib/` and the other tools, and edits outside `tools/sh_test_system.py`),
    `sh-code-reviewer` (code against requirements, architecture, API reference, current step), `sh-req-reviewer`.
  - `.claude/hooks/sh_clean_room.py`: the clean-room hook (29 allowed / blocked cases checked by hand).
  - `.claude/skills/`: `sh-review` (reqs / tests / code / trace / all), `sh-write-tests` (write, review, red run,
    freeze); `sh-verify-firmware` steps 3-4 launch the new reviewers.
  - `docs/process.md` §1, §3.1 (tests first, red run, freeze; replaces "tests follow the code"), §3.2, §9;
    AGENTS.md §4 steps 3-4; decision in `.agent/memory-bank/decisions.md`.
- **Trial run** of `sh-test-reviewer` on all 82 tests: the hook blocked every read of the code (32 attempts),
  but also the agent's report hand-back - fixed (the hook now guards only the file and shell tools; 30 cases
  checked); the old test file name is allowed for `git diff` against main. Review result: 22 tests OK, 60 with
  findings (7 safety), 59 requirements untested, API reference lacks field names, states and fault injection.
  Logged in `docs/problems.md` as PB-25 ... PB-42 (owner: "Da, toate"; 9 marked safety), not fixed here.
- `tools/sh_process_check.py trace`: WARN on a parent requirement with its own `{LEVEL; METHOD}` (FR-3, FR-9,
  FR-40) - requirements §0 says a parent only groups, and such a requirement was never counted as untested.
- **Open:** `architecture.md` §18 still says "no separate test step first"; the `sh-test-writer`,
  `sh-code-reviewer` and `sh-req-reviewer` agents have not run yet.

## [01.11.2026] - Owner page test: engineering view, commissioning, states, checks, version (PB-17 - PB-21)

- **Who / when / why:** main chat (Claude), 01.11.2026, the owner's hands-on remarks on the page (step 1 board)
  and the request for an engineering view "like CANoe / Trace32".
- **Requirements (baseline v2.6, changes after closure C-3 - C-8, owner answers 01.11.2026):** FR-8.1 header in
  words with the next change, admin view = engineering view (every API value with its API name, explanation,
  requirement, change highlight, change trace with CSV export), commissioning in the page; new FR-8.4 zone states
  (`FROST`, `DISABLED`, `FAULT`, `ACQUIRING`, `WINDOW`, `HEATING`, `IDLE` with Romanian texts); FR-10.4 checks with
  rule, value and `OK` / `FAIL` / `INFO`, no health score; FR-11.3 relay used once and not by the pump, zone on
  only with a sensor, number of zones = zones switched on; FR-21 version `1.MM.DD.NN` from git. Architecture §7
  displayed state order and §10 health line follow.
- **Board:** zone data copied from the old software erased (owner decision "now"): Zone 1-3, no sensor, off, relays
  32 / 33 / 25. Heating off until commissioning; the old mapping is in the session scratchpad.
- **Firmware:** `ThermalZoneManager::getStateCode()` (FR-8.4, a switched-off zone is `DISABLED`);
  `DiagnosticEngine::runChecks()` / `checksJson()` replace the health score in `/api/diag/status`,
  `/api/boiler/status`, `/api/diag/selftest`, console `DIAG` and the boot banner (no more `PASS` without a check);
  `dtcCount` counts active DTCs; `/api/setup/map-zone` checks every field first (HTTP 400, nothing changed);
  enabling the pump is refused while a zone uses relay 4; `APP_VERSION` written by `tools/sh_git_version.py`;
  installation names removed from the relay comments in `Config.h` (FR-11.1).
- **Web:** family view shows switched-on zones with their state text and the schedule sentence; admin view has the
  checks, commissioning per zone (name, sensor, relay, on / off, offset), sensors, schedule, system, the signal
  watch of 8 GET endpoints (2 s) and the change trace (CSV).
- **Tests:** TC10.2 / TC10.3 without health score, new TC10.4 (FR-10.4), TC15.1 version from git (FR-21), TC19.6
  no assertion on the temperature of a zone without reading (no requirement), new TC19.8 (FR-8.4), TC19.9 (every
  API value explained, FR-8.1), TC19.10 (FR-11.3 rejections, pump on a zone relay).
- **Tools:** `sh_process_check.py` errors on a hard-coded `APP_VERSION`, release tag from the computed version;
  `sh_esp_cli.py diag` prints the checks.
- **Safety review:** BLOCKER B1 (relay moved while energised) and RISK R1 (zone off while its valve is the last open one), R2 (relay 4 polarity), R3 fixed before flashing; valve-open rule added to FR-11.3.
- **Found on the way:** PB-22 (room `+` lowers a target above 22.5 °C), PB-23 (FR-51 not built, safety), PB-24 (relay swap).

## [01.11.2026] - Page works behind a path-prefix proxy (PB-15)

- **Who / when / why:** main chat (Claude), 01.11.2026, owner report: through the home-server proxy
  (`/heating/`) the page showed only a dark skeleton with "HTTP 404".
- **Cause:** the page requested root-absolute URLs (`/api/...`, `/update`), which skip the proxy prefix.
- **Requirements:** FR-8.1 gains the proxy clause (change after closure C-2, baseline v2.5); owner approved, kept
  general - not tied to one server.
- **Web:** `web/index.html` requests every URL relative to the page address (`api/...`, `update`). No firmware
  logic changed.
- **Tests:** new TC19.7 (FR-8.1) fails on 7743c26 (5 root-absolute URLs), passes on the fixed page.
- **Checks:** `pio run` 1.14 MB; `sh_process_check.py` 0 errors, 1 known (PB-12). Safety review not needed (no
  `src/`, `include/`, `platformio.ini` change).
- **On the board:** the uncommitted build was flashed by mistake (`sh_ota_upload.py --help` uploads at once,
  PB-16); `/api/zones` 200 direct and through the proxy, TC19.7 PASS. Not a result of record until built from the
  commit.

## [01.11.2026] - Migration step 1: ParamStore (one parameter table, NVS in one place)

- **Who / when / why:** main chat (Claude), 01.11.2026, owner request "do it here" (architecture §18 step 1).
- **Firmware:** new `ParamStore` (`include/ParamStore.h`, `src/ParamStore.cpp`) is the only NVS user (R-A4): one
  table with the requirements §6 defaults and ranges (interlock, overrun, burner run / pause, hysteresis, sensor
  timeout, frost, window threshold, heartbeat, link heartbeat, open-loop bypass, actuator type, pump), zone records,
  learned duty (DTC-301 when discarded), state and location; range check on set and load, writes only on change.
  One-time copy of the old keys (schema 1), written only if every record was stored; old keys kept for a rollback.
  `ThermalZoneManager`, `BoilerRadioTrigger`, `TimeAndWeatherService` load from it; setters reject out-of-range
  values. `/api/config/system` POST validates all fields (exact step, §6 range) and rejects the whole request with
  HTTP 400; new fields `burnerMinRunSec`, `burnerMinPauseSec`, `hysteresis`, `windowThreshold`; removed
  `boilerDelaySec`, `turReturSensors`, `shortCyclingSec`, per-zone hysteresis / window threshold. Interlock and
  overrun persisted; one global hysteresis and window threshold; RF cooldown fixed 3 s (FR-17.2); reference MACs and
  zone names removed from firmware (FR-11.1); factory levels 23 / 21 / 18; location validated (FR-9.2).
- **Tests:** registry read from the requirements document (no copy); retired IDs remapped; TC07.2 interlock range,
  TC14.1 / TC14.4 sensor timeout range, TC02.2 location range, TC26.2 global window threshold, TC27.2 restores the
  board, new TC27.6 rejection below the §6 floors. Checker reads the safety rows of the ParamStore table.
- **Fixes:** PB-4, PB-5, PB-6, PB-8, PB-9, PB-11 (PB-13 with the flash). PB-14 stays open: the default is §6, the
  second DTC-101 path (1200 s in `DiagnosticEngine`) is step 6.
- **Checks:** `pio run` 1.19 MB; `sh_process_check.py` 0 errors, 1 known (PB-12); `sh-safety-reviewer` no BLOCKER,
  its three migration risks fixed before the commit (schema only after successful writes, migrate only without a
  schema, old keys kept).
- **On the board (01.11.2026):** flashed 413fa60, then 7743c26 (window threshold endpoint no longer needs a zone
  id; TC26.2 had left 1.5 C on the board because its restore was rejected). Migration log: every zone (name, MAC,
  pin, enabled, target, offset), schedule, levels 22 / 20 / 16, burner counters and location kept; burner 180 / 180,
  heartbeat 900 s, sensor timeout 600 s, frost 12 C carried over; Z2 / Z3 window 0.3 C dropped (global 1.2 C);
  `openLoopBypass` false (test leftover reset). Interlock 190 s, window 1.3 C, hysteresis 0.35 C survived an OTA
  reboot (PB-5), then restored. `sh_test_system.py` 81/81 PASS, coverage 57 / 91 requirements (the rest belong to
  later steps), tests leave the configuration unchanged; `sh_process_check.py board` PASS; free heap 112 KB.
  PB-4, PB-5, PB-6, PB-8, PB-9, PB-11, PB-13 set to fixed; closed after the owner's acceptance.

## [01.11.2026] - Requirements v2.4: watchdog on both tasks, safety reaction bound 10 s

- **Who / when / why:** requirements and architecture sessions, 01.11.2026, after two owner decisions
  (`decisions.md`): O-16 (no preference, recommended option) and the simplified NFR-1.8 (option B).
- **Requirements:** v2.3 — NFR-1.5 task watchdog covers `loop()` and the control task, a manual request counts from
  its acceptance; v2.4 — NFR-1.8 one bound of 10 s for every safety reaction, only exception a firmware update with
  the boiler OFF; D39 radio-cooldown clause superseded. Recorded as post-closure changes (`requirements_review.md` §9).
- **Architecture:** O-16 decided, 10 s bound everywhere, detail that existed only for 2 s removed; AR-21 (own control
  task) kept.
- **Checks:** `sh_process_check.py` 0 errors, 21 known, PASS. **Not changed:** firmware, tests.

## [01.11.2026] - Requirements v2.2 frozen; control tick in its own task (AR-21)

- **Who / when / why:** requirements and architecture sessions, 01.11.2026, after the owner's questionnaire
  (`decisions.md`, "Control in its own task…"). Verified and committed by the main chat.
- **Requirements v2.2:** O-12 (status reports longest control-tick delay and loop pass), O-13 (pump stop and forced
  manual OFF under NFR-1.8), O-14 (commissioning result is a document), O-15 (self-check may end a burner run early).
  Review closed by the owner; open action only: blueprint location and version (D37).
- **Architecture:** AR-21 rewritten — control tick in its own FreeRTOS task above `loop()`, command queue I15 and
  snapshot I16, both tasks on the watchdog, built in migration step 10; AR-3 / P6 amended; AR-22 approved; RAM budget
  +16 KB static against a 10 KB margin, covered by the `String` removal of step 11 and the planned log-ring saving
  (risk). New observation O-16 (watchdog of both tasks, NFR-1.8 trigger, OTA exception) for the owner.
- **Checks:** `sh_process_check.py` 0 errors, 21 known, PASS. **Not changed:** firmware, tests.

## [01.11.2026] - Requirements D37-D39 and architecture aligned to baseline v2.1 (wave 2)

- **Who / when / why:** requirements and architecture sessions started by the main chat, 01.11.2026; verified and
  committed by the main chat. Owner decisions D37 = B, D38 = A, D39 = A (`decisions.md`).
- **Requirements:** FR-60 skips the sensor check while no zone has a mapped sensor; NFR-1.8 bound applies to the
  decision and every relay, a radio frame may also wait for the FR-17.2 cooldown; blueprint location / version
  still to be given by the owner (D37). Review header still open (owner sign-off).
- **Architecture:** NFR-1.8 allocated and designed (bounded `WebServer` pass, ≤ 500 ms loop pass, relays before the
  radio frame, loop timing measured, OTA only behind `QuietGate`); boot OFF starts `burnerMinPause`; FR-60 per D38;
  glossary terms, I-3 with fault duty, only `SW` requirements allocated to modules, gaps G-n and steps cite `PB-n`,
  new gap G33. Proposed for the owner: AR-21, AR-22; observations O-12…O-15 for the requirements.
- **Checks:** `sh_process_check.py` 0 errors, 21 known, PASS. **Not changed:** firmware, tests.

## [01.11.2026] - Process review QA-1..QA-28 applied (wave 1)

- **Who / when / why:** three sessions started by the main chat, 01.11.2026, after the owner approved every row of
  `docs/process_review.md` with the recommended options. Verified and committed by the main chat.
- **Process session:** `docs/process.md` (test review gate §3.2 in the owner's order, waivers, tailoring, ASPICE
  labels, SUP.1 / MAN.3, owner flash decision), `AGENTS.md` (safety list by ID and parameter, FR-51, NFR-1.4; plan =
  architecture §18), `sh-safety-reviewer` and `sh-verify-firmware` read requirements and invariants at run time,
  `sh_process_check.py` (waivers, `{LEVEL; METHOD}` attribute, coverage per method, KNOWN errors via open `PB-n`,
  R-A1…R-A7 grep WARN), `sh_git_version.py` fixes, `problems.md` PB-1…PB-14, new columns in problems / acceptance.
- **Requirements session:** baseline v2.1 — `{LEVEL; METHOD}` on every active requirement, glossary, FR-2 / FR-4
  by parameter name, "flame lit" defined, FR-60 one mapped sensor, NFR-1.4 boot OFF starts `burnerMinPause`, new
  NFR-1.8 (safety reaction ≤ 2 s), phase tags mapped to migration steps, waiver table; review stays open for D37-D39
  and the owner's sign-off.
- **Records session:** `decisions.md` (D1-D36, O-1…O-11, AR-1…AR-20, quiz, work order, superseded decisions),
  `plan.md` as the migration plan with risks, memory-bank pointers to the architecture, hydraulics doc in English
  aligned with baseline v2.
- **Checks:** `sh_process_check.py` 0 errors, 21 known (PB-n), PASS; `pio run` clean. **Not changed:** firmware
  (`src/`, `include/`, `web/`), tests.

## [01.11.2026] - Process review QA-1..QA-28 (proposal)

- **Who / when / why:** main chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026, asked by the owner
  after the process, requirements and architecture chats were merged back: review everything except code and tests.
- **Where:** `docs/process_review.md` (new): 28 rows, 6 blocking (empty `problems.md` with known safety defects on
  the board, `AGENTS.md` safety list and `sh-safety-reviewer` against baseline v2 incl. FR-51, requirements review not
  closed and its exit criterion unreachable without waivers, decisions not in `decisions.md`, test/code order and no
  test review gate). Then requirements (SYS/SW level, verification method, FR-4 values, "flame lit", glossary, OTA
  validation, burner pause after reboot, scope vs plan), architecture (loop blocking vs FR-37, tailoring, RAM risk,
  I-3), ASPICE labels, traceability of acceptance / problems, CI, stale memory-bank and hydraulics docs.
- **Status:** awaiting owner approval row by row. **Not changed:** any reviewed file, firmware, tests. Nothing committed.

## [01.11.2026] - Process: one active chat owns every zone

- **Who / when / why:** process chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026, asked by the owner.
  The owner moves all work back to the main chat (parallel chats lost context); the "one chat, one zone" rule
  could have made that chat refuse process or requirement work.
- **Where:** `AGENTS.md` §5: "When only one chat is active, it owns every zone." The zone split still applies
  when parallel chats run.

## [01.11.2026] - Traceability: SWE.1 ↔ SWE.2 kept in one place and checked

- **Who / when / why:** `/simplify` chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026 02:10, asked by the
  owner to simplify SWE.1, SWE.2 and the trace between them. The requirement → module links were written twice by
  hand (`architecture.md` §3.2 column and §15 matrix), disagreed on 37 requirements and no tool read them.
- **Where:**
  - `docs/architecture.md`: §3.2 Requirements column is the only allocation (union of both old tables, no link
    dropped; FR-64 → `SafetySupervisor` taken from §6.1 S4; new "all modules" row for cross-cutting NFRs; DTC IDs
    removed, §10 is the DTC → module table, now one row per code); §15 matrix replaced by a pointer; the FR-51
    sensor-loss rule stated once in §7.1 (S4 had dropped "outdoor unknown" and the disabled zone); §9.2 cites FR-58
    instead of copying its list.
  - `docs/requirements_and_features.md`: ID lists removed from section headings (they had drifted: FR-39, FR-72
    missing, FR-44 misplaced); §0 says IDs are cited in full, never as ranges; §7 names the hand-kept links.
    No requirement text changed.
  - `tools/sh_process_check.py trace`: reads the §3.2 allocation (ERROR on a retired / unknown / parent ID, WARN on an
    active requirement without a module), derives parent IDs from children instead of headings, matrix gets a
    Modules column, `--matrix` works without a path. Output otherwise identical (16 errors, 84 warnings, as before).
  - `docs/process.md`: SWE.2 row, rule R1 (no ID ranges), §7 gap partly closed.
- **Owner review:** the 37 merged rows in §3.2. **Not changed:** firmware, tests. Committed locally with the
  first commit of `tools/sh_process_check.py`, `tools/sh_git_version.py` and `docs/process.md` (owner OK); not pushed.

## [01.11.2026] - Requirements: architecture observations O-1..O-11 applied

- **Who / when / why:** requirements chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026 01:29.
  The architecture chat handed observations O-1..O-11 (`docs/architecture.md` §16) to the requirements chat;
  the owner decided the open ones by questionnaire.
- **Where:** `docs/requirements_and_features.md` (FR-3, FR-12.2, FR-47, FR-51, FR-54, FR-58, FR-62, FR-64, FR-69,
  FR-71, NFR-1.4, NFR-3.2, §5 new DTC-208, §6 new parameters) and `docs/requirements_review.md` §6 (decision log).
- **Safety:** the boiler never burns with all zone valves closed (O-1); a zone with a lost sensor keeps frost safety
  in global OFF below `faultFrostOutdoor` = 5 °C outdoor (O-2, owner chose 5 °C); the flow-limit OFF is forced (O-5).
- **Not changed:** firmware, tests, `docs/architecture.md`. Nothing committed.

## [01.11.2026] - Architecture: SWE.2 for baseline v2 approved

- **Who / when / why:** architecture chat (Claude, branch `claude/vibrant-ride-95fick`), 01.11.2026, entry written
  01.11.2026 00:50. Asked through the session that coordinates the chats: write the software architecture (ASPICE
  SWE.2) for requirements baseline v2, because process §7 listed "SWE.2 architecture not defined" as open and the
  current code mixes control and safety in one class. Every decision row was approved by the owner.
- **Where:** `docs/architecture.md` (new): 23 modules in 6 layers, `SafetySupervisor` as the only writer of outputs,
  interfaces, 1 s control tick, state machines (zone, valve, boiler, boiler link, mode, season, boot / OTA), validity
  `Ok` / `Held` / `Lost` on every input with a fallback matrix, `ParamStore`, DTC and event store, fault injection,
  output / boiler-link / input abstractions, phase-4 advisor contract, flash / RAM / timing / wear budget, trace of
  all 103 active requirement IDs to modules, gap list G1-G32 and 14 migration steps. `AGENTS.md` §5: new zone
  *architecture* (`docs/architecture.md`), added with the owner's OK.
- **Owner decisions (AR-1..AR-20):** last zone valve stays open while the burner is still ON or its OFF is deferred,
  then for `overrun`; learned TPI duty per zone and outdoor band of 5 °C; weather fetch in a background task;
  history in LittleFS; flash margin ≥ 40 KB; season safety net only in season mode AUTO. Later the same day: in
  global OFF a zone with a lost sensor runs `faultDuty` when it is cold outside (O-2, limit set by the requirements
  chat); a flow-temperature limit stops the boiler at once (O-5, only with the optional probes, none installed);
  G2 fixed in migration steps 4 and 9, no separate hotfix. Work order (owner): requirements → architecture → code,
  each migration step changes the code and its tests together (no separate test step first).
- **Aligned 01.11.2026 01:45** with the requirements commit 925ea91: `faultFrostOutdoor` (5 °C) in global OFF and
  for disabled zones, DTC-208 for a failed output expansion, §6 parameter names, `AUTH` list = FR-58, §16 marked
  applied, §18 work order.
- **Not changed:** firmware, tests, requirements. Open: observations O-1..O-11 (`docs/architecture.md` §16) for the
  requirements chat (O-1 and O-2 are safety); dependency checks R-A1..R-A7 for the process chat.

## [01.11.2026] - Requirements: review approved, baseline v2

- **Who / when / why:** requirements chat (Claude, branch `claude/vibrant-ride-95fick`), 31.10.2026–01.11.2026,
  entry written 01.11.2026 00:02. Asked by the owner: ASPICE SYS.2 / SWE.1 review of the v1 specification
  (Romanian, partly obsolete, values contradicting each other and AGENTS.md), then a new baseline. Every row was
  approved by the owner.
- **Where:** `docs/requirements_review.md` (new): findings per requirement, domain gaps G-01..G-22, owner's
  blueprint comparison B-01..B-21, decisions D1-D36, fallback matrix. `docs/requirements_and_features.md`
  rewritten in English as baseline v2 (IDs kept where the meaning did not change, retired IDs struck through,
  new FR-52..FR-72 and NFR-5.x, DTC table, parameter table).
- **Key decisions:** interlock 180 s; overrun keeps the last valve open without a pump too; burner min run / pause
  180 s; sensor timeout 20 min, then the zone runs its learned duty (20 % if nothing learned); frost protection
  also on disabled zones; boost upward only (+0.5-2 °C, ends after 3 h or at the next slot); levels 23 / 21 / 18;
  one global window threshold, smart window detection in phase 4; automatic heating season; 1-32 zones;
  fail-safe boiler link (heartbeat, can be disabled); API authentication; OTA rollback and manual full self-test;
  alarms page-only; every feature has a fallback that the occupants do not notice (NFR-5).
- **Not changed:** firmware, tests. Open: `tools/sh_test_system.py` still cites retired FR-13, FR-24..27, FR-33,
  FR-38 (`sh_process_check.py trace`: 16 errors); new DTCs are not raised by the firmware yet.

## [31.10.2026] - Process: chat zones, problems log, acceptance, release gate

- **Process:** AGENTS.md §5 "One chat, one zone" - process / requirements / firmware chats change only their
  files; shared logs are append-only.
- **Process:** new `docs/problems.md` (`PB-n`, symptom, board commit, safety, cause, fix, status) and
  `docs/acceptance.md` (owner's hands-on test: board commit, what was checked, OK/NOK). process.md §5 now
  covers commit -> flash -> accept -> release gate -> PR -> merge + tag `vAPP_VERSION`; §8 problem flow.
- **Tools:** `sh_process_check.py process` checks the problems log and `PB-n` citations in commits;
  new `release` mode: OK acceptance for a build with HEAD's firmware, no open safety problem,
  `APP_VERSION` not already tagged with other firmware.

## [31.10.2026] - Process: requirement rules, board check, review / decision / commit gates

- **Process:** new [`docs/process.md`](process.md) - work products, requirement writing rules (R1-R6),
  requirements review steps and exit criteria, decision recording, commit/push gate, board check,
  known process gaps. Referenced from AGENTS.md §4.
- **Tools:** `sh_process_check.py reqs` (also in the default run and CI, WARN only) - one value per safety
  quantity, vague wording, one meaning per DTC (two names, unnamed citations, defined but never raised).
- **Tools:** `sh_process_check.py board [--ip]` - live `/api/config/system` values against the safety limits
  (ERROR), commit the board runs vs HEAD (WARN). Added to AGENTS.md §4 and the `sh-verify-firmware` skill
  (before and after flash).
- **Build identity:** new pre-build script `tools/sh_git_version.py` writes `include/git_version.h`
  (gitignored) with `APP_GIT_COMMIT` = short HEAD, `-dirty` on uncommitted firmware inputs. `Version.h`
  includes it instead of the hard-coded `8065534`. `sh_process_check.py process` errors on a hard-coded
  commit or a missing pre-script.
- **Open (process gaps, see process.md §7):** burner min pause not exposed by the API.

## [31.10.2026] - Tool naming convention

- **Process:** new project tools/scripts start with `sh_` (AGENTS.md §3). `tools/process_check.py` renamed to
  `tools/sh_process_check.py`; CI, `verify-firmware` skill and `safety-reviewer` agent updated.
- **Process:** existing tools renamed too: `sh_esp_cli.py`, `sh_test_system.py`, `sh_build_web.py`
  (`platformio.ini` extra_scripts), `sh_ota_upload.py`, `sh_dev_server.py`. Agent `sh-safety-reviewer` and skill
  `sh-verify-firmware` use `sh-` (names allow only hyphens). README, AGENTS.md, docs and memory bank updated;
  older changelog entries keep the old names.

## [30.10.2026] - Agent workflow: safety reviewer and verify skill

- **Tools:** `.claude/agents/safety-reviewer.md` - read-only agent that reviews the firmware diff against the
  AGENTS.md safety list (interlock, overrun, anti-short-cycling, frost, DTC-101 / safe state) and resource rules;
  runs `process_check.py process` first and reports BLOCKER / RISK / NOTE.
- **Tools:** `.claude/skills/verify-firmware/SKILL.md` - the "verification before flashing" pipeline as one
  command: `pio run` -> `process_check.py` -> safety review -> OTA flash (asks first) -> `test_system.py`.

## [30.10.2026] - Traceability and process check

- **Tools:** new `tools/process_check.py` (stdlib only, no board). `trace`: requirements in
  `docs/requirements_and_features.md` vs test docstrings / `SWE1_REQUIREMENTS` vs `// FR-x` tags in code,
  optional Markdown matrix (`--matrix`). `process`: safety defaults (interlock, overrun, anti-short-cycling,
  frost, sensor timeout), changelog + activeContext updated with code, commit message format, debug leftovers,
  tracked `secrets.h`, library changes, firmware < 1.25 MB. Runs in GitHub Actions after `pio run`.
- **Safety fix:** `Config.h` defaults were test-bench values (interlock 5 s, overrun 10 s, boiler off 15 s)
  and are not stored in NVS, so every boot ran with them. Now 120 s / 180 s / 180 s (FR-2, FR-3, FR-4).
  The check also covers the real anti-short-cycling defaults in `BoilerRadioTrigger` (`min_pause`, `min_run`).
- **Checker review fixes:** requirement IDs must match exactly (`FR-10.9` no longer passes via `FR-10`);
  coverage counts only tests citing the leaf's own ID, parent-only coverage is a separate warning (30/88, was
  reported as 75/88); duplicate requirement definitions are errors; API clamps and the frost NVS default are
  checked next to the compile-time defaults; git failures abort with an error instead of passing; merge
  commits skipped; paths with spaces/diacritics parsed correctly; CI on `main` diffs against the previous push.
- **Open findings (not fixed):** API clamps allow interlock/overrun from 1 s, short-cycling from 10 s, frost
  from 5 °C; code tag `FR-17.7` not in the requirements doc; FR-41..FR-45 used by tests but missing from the requirements doc; FR-36
  (Home Assistant) still in the doc; commit `abb8dec` subject `test,cli:` not in the allowed types;
  `/api/config/system` accepts interlock/overrun down to 1 s; `_minBoilerOffMs` is never read.

## [30.10.2026] - First verification on the board

- `pio run` clean (flash 1.19 MB), flashed with `tools/ota_upload.py`, `tools/test_system.py` 78/78 PASS.
- **Tests:** dropped `FR-36` (Home Assistant, removed in phase 1) from the requirement registry; new
  `SWE.5-TC16.5` covers `FR-44` global away (toggle, every zone's `effectiveTarget` = away level, restore).
  `activeLevel` stays the slot level while away is on — by design, the UI checks `away` first.

## [29.10.2026] - Level schedule, new UI, TPI control

- **Schedule:** bio-schedule, presets, anchors and named blocks replaced by day/night/away levels per zone,
  one weekday and one weekend slot list for all zones, global away mode. New API `/api/schedule/level`,
  `/api/schedule/slots`, `away` on `/api/schedule/enable`; console `SCHEDULE LEVEL`, `SCHEDULE SLOTS`, `AWAY`.
  NVS `sched/cfg2`; legacy schedule keeps only its on/off flags, legacy keys and `presets` namespace cleared.
- **UI:** `web/index.html` rewritten (4886 → 222 lines): room cards, timed boost (16–22.5 °C, 0.5 °C), Away,
  alerts, `#admin` view. `web/setup.html` and `/setup` removed. Checked in headless Chromium against a mock API.
- **Control:** TPI (60 min cycle, min 10 min, band 1.5 °C, learned base duty 0–60 %, hysteresis = overshoot cut),
  weather-dependent pre-heat lead (45 min–4 h), per-zone sensor offset (`OFFSET`, map-zone `offset`).
- **Tools:** `ota_upload.py` reads OTA credentials from `secrets.h` and refuses to flash without it;
  `esp_cli.py schedule` forwards to the console; test suite group 16 rewritten for the level schedule.
- Docs: README and API reference rewritten for the current firmware.

## [29.10.2026] - Lean phase 2: firmware removals

- **Safety:** removed Service Mode and forced boiler START (both bypassed the anti-short-cycling guard). Forced STOP kept.
- Removed PWA (manifest, service worker, icons), web console page (`/console`; Telnet and `/api/console/*` kept), unused `PubSubClient`.
- Replaced `DevicePassport` with a plain weather location setting (`/api/setup/location`, console `LOCATION`), migrated from the old NVS namespace.
- Added GitHub Actions firmware build (`.github/workflows/build.yml`).

## [29.10.2026] - Lean phase 1: repository cleanup

- Removed docs outside current scope: market/ROI, sales plan, multi-tenant SaaS architecture, remote fleet, session summary, generated test report.
- Removed Home Assistant integration (`homeassistant/`) and its tests (TC22.x), nginx/portal/docker tooling, browser review scripts.
- Removed unused web pages `web/index_v2.html`, `web/public_landing.html` (not compiled into firmware).
- Rewrote `AGENTS.md` and `docs/plan.md` as short, phase-based documents.
- Firmware (`src/`, `include/`) untouched.

## [02.11.2026] - Process: fewer agent runs per step

- **Who / when / why:** process chat (Claude), 02.11.2026; token use of the export / import session.
- **Where:** `sh-change` (`sh-req-reviewer` on changed IDs only), `sh-write-tests` (one test review, hash of the test
  file in the step record, area for the writer), `sh-verify-firmware` steps 3-4 (no second test review on the same
  hash; `sh-safety-reviewer` only for the safety list; reviewers see what changed since their last run).
- **Also:** every agent's report is capped (40 lines, passed checks as one line, no restating).
- **Also:** every agent's report is capped (40 lines, passed checks as one line, no restating).
- **Not done:** splitting `tools/sh_test_system.py` by area (the firmware session is editing it).

## [02.11.2026] - Process: light workflow by default, full on request

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner reached 45 % of the weekly limit on a Monday.
- **Where:** `sh-change` (modes), `sh-verify-firmware` (`light`), AGENTS.md §4 and §5, `docs/process.md` §3.1.
  Light: the chat does everything, one safety review only for the safety list. Full: all agents, when the owner asks
  and before a release.
- **Not verified:** neither mode has run; the export / import session still runs its step in the full chain.

## [02.11.2026] - Process: guard hook for the order tests before code

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner: tests must never be written after the code just
  to pass; a bad test or requirement goes to them.
- **Where:** `.claude/hooks/sh_chain_guard.py` + `.claude/settings.json` (PreToolUse on Edit / Write), `tools/sh_chain.py`
  (start, freeze, unfreeze, done), `check_chain` in `tools/sh_process_check.py`, `.gitignore` (`.claude/state/`),
  `sh-change`, AGENTS.md §5, `docs/process.md` §3.1.
- **Verified:** the hook exits 2 / 0 as designed in both phases and without a marker (Windows paths); the checker errors
  on a changed frozen test file. **Not verified:** a real chain run; new sessions load the hook, running ones do not.

## [02.11.2026] - Process: system tests split by area

- **Who / when / why:** process chat (Claude), 02.11.2026; the owner: the test file is far too long (4,511 lines, one
  agent run read it whole for ~96k tokens).
- **Where:** `tools/sh_test_system.py` is now the runner (list of tests, traceability, report, `main`);
  `tools/sh_tests/base.py` (HTTP helpers, requirement registry, `TestResult`, `SuiteBase`) and seven area modules
  `t_health`, `t_hydraulics`, `t_control`, `t_ui_history`, `t_safety_features`, `t_export_import`, `t_zones`, each one a
  mixin class. Tests only moved: all 164 methods and class constants are identical (AST comparison with the old file),
  129 tests listed. Adapted: `tools/sh_process_check.py` (`test_files()`, tests parsed per file, `file:line` refs,
  frozen-hash check over all files), `tools/sh_chain.py` (hash over all test files, new command `hash`),
  `sh_chain_guard.py` and `sh_clean_room.py` (the whole `tools/sh_tests/` folder), agents `sh-test-writer`,
  `sh-test-reviewer`, `sh-assessor`, skills `sh-write-tests`, `sh-verify-firmware`, `docs/process.md` §2 R7, §3.1, SWE.6 row.
- **Verified:** AST identical for every method; `trace` output equal to the old one apart from `file:line`; checker
  PASS (0 errors); guard and clean-room hook exit codes for the folder in both phases; a changed test file after
  `freeze` gives the ERROR, restored gives PASS. **Not verified:** a run against the board (the suite itself was not
  started; the firmware session owns the board).
