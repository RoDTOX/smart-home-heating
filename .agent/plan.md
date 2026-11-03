# Implementation Plan: Gas Calibration Log, Generic Boiler Model & Restart Timeline Markers

## Original request (verbatim)
"nu pot sa modific calibrarea sau sa bag index, ca iar, cand dau pe Calibreaza Index/Setari.. imi dispare campul "Actualizare Index Fizic de pe Contor & Referință.." . nici nu inteleg cum functioneaza cand bag valoare noua in index. sper doar sa se adauge in log. 

ce nu imi place in schimb e ca ai bagat date despre centrala mea, ceea ce nu e ok, pentru ca ar trebui sa mi le ceri, nu sa le "suppose", si sa le scrii, alti useri pot avea alte centrale cu alte consumuri la alte temperaturi.

nu se vede log cand am bagat datele despre centrala, si ce valori. dar intre valori, cu cat ai mai multe date in timp ale indexului si temperatura setata, iti poti da seama aproximativ(eliminand gazul ars pe dusuri si apa calda de robinet), de consum si poti aproxima mai corect consumul, mai ales calcurand in pe valori de noapte, cand nu se consuma apa calda (asta poate fi un tips catre user, sa adauge consumul seara inainte de culcare si dimineata la trezire, inainte de dusuri). astfel datele pot fi algoritmizate corect, in baza temperaturii exterioare, cererii de caldura si a functionarii centralei. daca ajuta, se mai poate adauga si capacitatea in kw a centralei.

vezi ca acel tooltip cu "● Sincronizat cu arderea flăcării" cam sare in ochi aiurea. poate mai bine faci un grafic separat pentru datele astea de montiorizare consum gaz , pe langa acele chenare cu info.

pe graficul de istoric telemetrie, TPI, activitate , temepraturi, ar trebui puse pe grafic asa, ca idee si referinta, zonele unde a fost placa in flash/restartata/ din diverse motive (firmware update, testing etc)"

## Stage tracker
- [ ] Stage 1: Fix UI Calibration Form Persistence (`web/index.html` - stop destructive `box.innerHTML` re-render in `renderGasMeter()`; update individual DOM fields by ID so the calibration form stays open, keeps user inputs and doesn't flicker/collapse on poll)
- [ ] Stage 2: Generic Boiler Parameters & Capacity (`include/ParamStore.h`, `src/ParamStore.cpp`, `src/WebApi.cpp` - add configurable `boilerPowerKw`, remove any hardcoded brand/model assumptions; make all labels and tips 100% generic)
- [ ] Stage 3: Gas Calibration Event Log (`include/ParamStore.h`, `src/ParamStore.cpp`, `src/WebApi.cpp`, `web/index.html` - store history of index calibration events with timestamp, entered index, delta m³, heating burn hours, and empirical flow rate; display history log table in UI with night calibration advice)
- [ ] Stage 4: Subdued Badge & Gas Consumption Chart (`web/index.html` - remove jarring green badge, add dedicated gas consumption bar chart/breakdown)
- [ ] Stage 5: System Restart / Flash Markers on Telemetry Chart (`src/TelemetryHistoryManager.h`, `src/TelemetryHistoryManager.cpp`, `web/index.html` - track boot/restart events in firmware with reset reason, expose via `/api/history`, render vertical dashed markers with `⚡ Restart / Flash` badges on the timeline)
- [ ] Stage 6: Build, Process Check, OTA Flash & Hardware Verification

## Verification steps
1. Verify clicking "Calibrează Index / Setări" keeps the form open indefinitely without disappearing during automatic polling.
2. Verify saving a new index appends an entry to the calibration log with exact timestamp, heating burn time, and empirical flow rate.
3. Verify boiler capacity in kW is configurable and no hardcoded boiler model names exist in UI or code.
4. Verify the green badge is replaced with a discreet indicator and gas consumption is clearly visualised.
5. Verify reboot/flash events appear as vertical markers on the 1h/12h/24h/multi-day telemetry charts.
6. Pass `pio run` and `tools/sh_process_check.py` with 0 errors.
7. Flash board via OTA and test on `192.168.1.20`.
