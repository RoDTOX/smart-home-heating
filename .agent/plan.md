# Implementation Plan: TPI Context, Room Card Subtitle Optimization & 24h Telemetry Chart

## Original request (verbatim)
"1. Ce înseamnă procentul de ciclu (TPI) și cum îl punem în context: am inteles, e un dutycyle per ora. ca sa iti raspund, imi place optiunea B dar si cu procentul de la optiunea A :)
2. Optimizarea cardurilor din partea de sus: 1.ok; 2/3💧 60% RH e ok.
3. Grafic pe 24h (Centrală, Zone 1/2/3, TPI și Temperaturi): centrala e ok cu flacara, dar zonele nu e ok sa le pui iconite, trebuie sa ia numele din configuratie, ca zonele vor diferi de la user la user.. unii poate au holuri, hale, etc.
graficul trebuie sa fie maricel, pe toata orizontala, eventual cu categoria lui, probabil sub inginerie & debug, sau Diagnoză sau standalone."

## Stage tracker
- [x] Stage 1: Card Subtitle & TPI Status Optimization (remove target duplicate, add `💧 X% RH`, format TPI as `Încălzire TPI · X min/h (Y%)` and pause as `Pauză ciclu (menține din șapă)`)
- [x] Stage 2: 24h History Chart Implementation in `web/index.html` (SVG timeline for Centrală and dynamic zone names from configuration, plus 24h temperature curves)
- [x] Stage 3: Requirements & ASPICE SWE.5 Test Suite Updates (`FR-34`, `FR-47`, `t_ui_history.py` new test `test_tc19_16_history_24h_chart_and_card_subtitles`)
- [x] Stage 4: Verification on Board (`pio run`, `sh_build_web.py`, OTA upload, test execution, `sh_process_check.py`)
- [x] Stage 5: Documentation & Git Commit

## Verification steps
1. Check room cards on `http://192.168.1.20/`:
   - Left subtitle shows humidity clearly: `💧 60% RH` (no redundant `țintă 23.5°`).
   - If manual hold active: `💧 60% RH · Boost manual`.
   - Heating state shows: `Încălzire TPI · 17 min/h (28%)` or `Încălzire continuă (100%)`.
   - Idle state in pause shows: `Pauză ciclu (menține din șapă)` or `Temperatură atinsă`.
2. Expand `Istoric & Grafic 24h`:
   - Binary activity lanes for 🔥 Centrală and each zone by its configured name (`Living`, `KidsDorm`, `AdultsDorm`).
   - Temperature chart showing 24h curves (00:00 to 24:00) with outdoor and zone temperatures.
   - Clean, full-width responsive SVG rendering without external libraries.
3. Run `sh_test_system.py`, verify SWE.5 test passes.
4. Run `sh_process_check.py` and `sh_process_check.py board`.
