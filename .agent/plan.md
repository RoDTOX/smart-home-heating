# Implementation Plan: 1h Resolution, Real-Time Ongoing Slot Telemetry & Multi-Day Gap Integrity

## Original request (verbatim)
"ba,... ce dracu e cu graficul asta ma ? partea aia de acum 7min pana in prezent, apare ca e cerere pe camera adultilor, dar centrala e oprita... :/ cel putin pe grafic, ca in realitate ea e pornita.... ce cacat se intampla ? a, hai ca in a 2-a poza, pare ca s-a desenat un putoi d-ala rectangular. dar tot nu e ce trebuie.. pentru testare si acuratete, rog sa faci graficul si pe 1h resolution... 

inca o chestie... graficul pe 3 zile pare dubios.. nu prea pupa cu datele din ultimele 12/24h, ori e prea mare rangeul inte valori.. vezi ultimele 2 poze, poate iti dai seama"

## Stage tracker
- [x] Stage 1: Real-Time Current Slot Tracking in Firmware (`src/TelemetryHistoryManager.cpp` - update ongoing in-memory slot in RAM every 10s with live boiler/valve/temp status so chart reflects reality up to the current minute, commit to LittleFS on 5m slot boundary)
- [x] Stage 2: 1-Hour Chart Resolution (`web/index.html` - add "1 Oră" range button [1h, 12h, 24h, 3d, 5d], 12 slots @ 5min, 62px per slot, time marks every 10 min)
- [x] Stage 3: Multi-Day Gap & Integrity Fix (`web/index.html` - break curves cleanly on large gaps (>3h) instead of stretching misleading dotted lines across 48h voids; sync 24h into 5d buffer in `TelemetryHistoryManager.cpp`)
- [x] Stage 4: ASPICE SWE.5 Tests (`tools/sh_test_system.py` - TC20.1-4, TC19.16 PASS on live board)
- [x] Stage 5: Build, Process Check, OTA Flash & Hardware Verification (v1.11.03.25 commit `6d084ff` online on `192.168.1.20`)

## Verification steps
1. Test switching to "1 Oră" in UI: verified 12 high-resolution 5-minute slots across the last 60 minutes with 10-minute time marks. -> PASS
2. Verify live ongoing slot: when boiler turns ON, the chart's "Acum" slot updates in RAM immediately via `updateLiveSample()` without waiting for the 5-minute flash boundary. -> PASS
3. Test 3-Day and 5-Day views: curves break cleanly on multi-hour downtime gaps (`maxBridgeGap`) rather than drawing misleading long interpolations across weekend power-off voids. -> PASS
4. Pass `pio run` and `tools/sh_process_check.py` with 0 errors. -> PASS
5. Flash board via OTA and verify live on `192.168.1.20`. -> PASS (`v1.11.03.25` running)
