# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- Live gas meter index tracking, calibration history, generic boiler model, and refined telemetry & debug tools:
  - Base index calibrated: `2265.304 m³` (NVS key `mindex`).
  - Boiler dial setpoint: `40°C` (NVS `btset`), observed delivery: `43°C` (NVS `btflow`), calibrated flame rate: `0.60 m³/h` (NVS `grate`).
  - Boiler capacity: `24 kW` (NVS `bkw`), gas price: `3.30 Lei/m³` (NVS `gprice`).
  - Calibration history log (`/gas_calib.json`) stores up to 15 calibration events with empirical burn rate calculation and night calibration guidance.
  - Telemetry chart features minimalist circular `⚡` restart pins with fine 1px dashed amber lines and hover tooltip integration.
  - Open window detection visualized with cyan blocks and `🪟` icons on zone activity rows, hover tooltip status, and room card countdown badge with instant cancel button (`/api/zones/window/clear`).
  - Sliding peak baseline evaluation in `ThermalZoneManager.cpp` ensures real-world room cooling trends are not truncated prematurely by 5-minute fixed clock boundaries.
  - Live Console & Log Stream embedded in "Instrumente Inginerie & Debug" streaming real-time firmware activity from `/api/console/logs` with command execution.
  - Board flashed via OTA with commit `a9c5175` (v1.11.03.27); SWE.5-TC20, TC21, TC26, TC19.16 PASS; `tools/sh_process_check.py` 0 errors.
- Canonical plan is `docs/plan.md`; `.agent/plan.md` only points to it (AGY text kept below it as history). The 43 commits 31b8496..6170f16 were built outside the chain: reviewed on 03.11.2026, requirements rows C-14 ... C-20 written after the fact.
- Migration of `docs/architecture.md` §18, status in [`docs/plan.md`](../../docs/plan.md). Step 1 (`ParamStore`)
  committed (413fa60, fix 7743c26) and flashed; the owner's page test fixes committed (43ffe48 PB-15, 2b7564f
  PB-17 ... PB-21). Not pushed.
- Zones erased on the board by the owner (01.11.2026): heating off until the owner commissions them from the page.
- Old NVS keys are kept for a rollback of step 1; remove them in a later step, after acceptance.
- Process (02.11.2026): what the owner tells or asks starts skill `sh-change` (requirement down, tests first in a clean
  room, commit on the work branch after the gates, push / `main` on request). The chain has not run yet. Old log
  entries are in `docs/archive/`; the checker's default run counts WARN per section. The process changes of
  01.11 - 02.11 are not committed.

- The device is prepared for sale in the EU under the owner's brand (02.11.2026): EU obligations of the software in
  `docs/compliance.md`. First full review 02.11.2026: 12 partial, 19 missing; PB-43 ... PB-54 (4 safety),
  `requirements_review.md` §10 with owner questions D40 ... D50 (open). `sh-assessor` has not run yet.

- Model per work (02.11.2026): AGENTS.md §5 table; agents carry `model` / `effort` in their frontmatter (Opus only
  for the safety reviewer and safety tests).

- Firmware session (03.11.2026): C-14 / PB-66 / PB-67 (thermal deficit 100% duty on error >= 0.4 C, weather compensation, UI 2-in-1 sensor toggle, dirty protection with undo, accurate status labels). C-15 / PB-68 resolved: Honeywell multi-moment schedule matrix (P1-P4 per day/group), 4 modes (5+2, constant, unified, 7day), configurable away setpoints, clean 24h UI, SWE.5-TC16.6 PASS. Fix d366a91 applied (mode & tab switching). UI cleanup f7ad961 applied. Fix & feat 04a2a84 applied: restored closing brace in syncSchedInput, implemented always-visible vertical setpoint stepper on all zone cards, widened MAX_T to 30 C. Feat 1f162c5 and fix f5ccb5d (PB-70) applied: aligned PARAM_LIMITS with requirements §6 (rfSafetyHeartbeatSec up to 900 s, overrun 180 s floor), updated FR-8.1 (baseline v2.11), and added SWE.5-TC29.38 and SWE.5-TC19.11 tests for pre-import diff preview and vertical card steppers. Feat 7eae5ee and fix e2536da applied: replaced raw 34-key import dump with clean status card, cleared obsolete report on re-import/export, and disambiguated schedule levels in diff table (Program Zi vs Mod Plecat). Schedule active moment highlighting made day-group aware (no false weekend highlight on weekdays, Constant 24/7 badge updated), FR-10.4 check conditions clarified, operational checks for boiler/relays added, and dedicated active DTC panel with cause explanations implemented (SWE.5-TC19.12 & TC19.13 PASS). Admin panel reorganized into 4 collapsible groups (Confort & Programare default open, Sănătate, Configurare, Debug), Clear DTC button with session history added, and polling throttled when debug is collapsed (SWE.5-TC19.14 PASS). Implemented zone heating runtime telemetry (runtimeTodaySec, runtimeYesterdaySec in /api/zones and UI zone card badges) and closed all 4 admin sections by default (SWE.5-TC19.15 PASS). Optimized room card subtitles (removed duplicate target, added 💧 X% RH and Boost manual), clarified TPI cycle formatting (Încălzire TPI · X min/h, Pauză ciclu menține din șapă), and implemented 24h LittleFS telemetry chart in dedicated collapsible section with dynamic zone names and interactive hover inspection (SWE.5-TC19.16 PASS). Added multi-range selector (12h, 24h, 3d, 5d), hairline cursor guide, chronological rolling buffer reordering, clarified activity track legends, and real-time TPI algorithm inspector (SWE.5-TC19.16 updated PASS). Ensured runtime badge is unconditionally rendered across all room cards (even at 0m today) to maintain visual card symmetry. Removed door emoji ('caramizi') from zone labels, aligned 5d LittleFS buffer with absolute deterministic slot indexing, and corrected multi-day SVG time scaling so 5d correctly distinguishes from 3d. Anchored zone heating runtime counters to the 24h LittleFS buffer as single source of truth across reboots, improved X-axis labels to day+clock time, added gap detection with dotted interpolation, thinned lines with sample dots, and made hover tooltip persistent. `OutputBank` (step 3) committed. Board settings saved before
  the first flash: `side-session-notes/.../board_settings_2b7564f_2026-11-02.json`.

- Tests (02.11.2026): the system tests are split by area: `tools/sh_tests/t_<area>.py` (one mixin class each) and
  `base.py`; `tools/sh_test_system.py` is the runner. Checker, guard, chain hash and clean-room hook cover the
  whole set. Largest module: `t_export_import.py` (1,700 lines), split further when it grows.

- Workflow modes (02.11.2026): light by default (the chat does everything; one safety review for the safety list), full
  (all agents) on the owner's request and before a release - AGENTS.md §5.

## Next
- Step 2 (`Diagnostics`) built 04.11.2026, board test and owner OK pending; PB-116 open. Owner confirms `GET /api/diag/events` (FR-62).
- Findings of the C-30 final review and the test run on 1.11.03.37 are logged as PB-107 ... PB-115 (03.11.2026); PB-110 needs an owner decision. Board has rotated sensor MACs (PB-114): the owner fixes them. Firmware 1.23 MB of 1.25 MB: tight for the next steps (PB-115).
- Step 3 accepted by the owner 03.11.2026 (zones, sensors, `OutputBank`); the gas meter and the open window are accepted too. Step 2 (`Diagnostics`) starts in a new session from the main chat.
- C-29 flashed 1.11.03.35: measured flow 0.177 m3/h from the history, gas today 2.41 m3. Owner: look at the gas card, OK / NOK. Open: PB-103.
- C-26 (flow measured, flow and boiler kW not settable) flashed 1.11.03.34: log holds the owner's readings 2265.304 (15:39) and 2265.996 (22:19), both without runtime; measured flow starts with two more readings that have one. C-25 (gas meter readings as data) flashed 1.11.03.32, log holds the owner's 15:39 reading 2265.304; owner types the current reading in the page, then OK / NOK. Open: PB-100, PB-101. Rounds left: PB-75 day/night levels, console login (C-21).
- C-30 console login (FR-75..77) built with two profiles: board runs the dev build (open console, `profile` dev-open); release build not flashed, release gate fails while `SH_DEV_OPEN` can reach it (PB-104). Owner test pending. Open: PB-105, PB-106.
- PB-72 closed (owner OK). PB-80 fixed by df5fd70 (AGY, read and kept). PB-97 (stale WINDOW state, window button) fixed, flashed, owner test pending. Rounds left: PB-75 day/night levels, console login (C-21).
- Out-of-chain work kept (owner, 03.11.2026): folded into steps 7, 8, 11, tests first.
- Process review of 03.11.2026 (`docs/review_agy_03_11.md`): PB-71 ... PB-94 open (PB-71 and PB-74 safety). Owner has not tested 1.11.03.x; no acceptance row for it, and the OK for `OutputBank` (step 3) is still open.
- Owner decisions: console authentication (FR-58, D42, D49), wording of FR-34 / FR-40 / FR-35 / FR-31, the unit of the 1.25 MB limit.
- Owner proposal (03.11.2026): Decouple web UI assets to LittleFS with dual versioning (target migration step 12).
- Owner: hands-on acceptance of step 1 and the page (row in `docs/acceptance.md`), then the step review record and
  step 2 (`Diagnostics`).
- Test findings of the trial review, PB-25 ... PB-42 (9 safety): fix through `sh-change`, tests by `sh-test-writer`.
- `docs/architecture.md` §18 still says "no separate test step first" - align it with `docs/process.md` §3.1.
- PB-43 first (setup access point never closes, open to anyone in range): through `sh-change`, FR-50 needs a
  closing rule (owner decision).
- Owner decisions D40 ... D50 (EU rules), one at a time through the chain; most land in migration steps 9 and 12.
