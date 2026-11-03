# Implementation Plan: 24h/Multi-Day Telemetry Range, Hairline Cursor & TPI Algorithm Inspector

## Original request (verbatim)
"arata bine graficul, ar mai fi fost util o linie care sa urmareasca cursorul
posobilitatea sa afisam graficul pe 12/24 ore, 1,3,7 zile 
se afiseaza de 2x legenda, o data in partea de sus si o data in zona laterala, dar nue  clar ce inseamna cele 4 bare . nu avem date despre algoritmul TPI, sau nu il vad eu."

## Stage tracker
- [x] Stage 1: Cursor Tracking Hairline (SVG vertical dashed guide snapping to cursor X coordinate across entire chart height)
- [x] Stage 2: Time Range Selector (12h, 24h, 3d, 5d) with chronological rolling buffer reordering (eliminates artificial yesterday-today jump/crossover)
- [x] Stage 3: Clarify Legend & Activity Tracks (remove redundant top labels, add explicit headers explaining colored bars = open valve / active flame, separate temperature curve legend)
- [x] Stage 4: TPI Algorithm Breakdown Table & Explanations (show P deficit, outdoor weather compensation, learned NVS inertia I, total duty %, on-minutes per hour, and current cycle state per zone)
- [x] Stage 5: ASPICE Requirements & SWE.5 Tests Update (`SWE.5-TC19.16` updated / `TC19.17` added in `t_ui_history.py`)
- [x] Stage 6: Build, Flash OTA, Hardware Verification & Process Check

## Verification steps
1. Check hairline cursor line follows mouse over SVG chart and snaps to 5-minute slots.
2. Test switching between 12 Ore, 24 Ore, 3 Zile, 5 Zile:
   - 12h: last 12 hours rolling ending at "acum"
   - 24h: last 24 hours rolling ending at "acum" with zero crossover lines
   - 3d & 5d: multi-day history from `api/history?range=5d`
3. Verify legend clarity:
   - Clear title: "Activitate Relee & Cazan (Bare colorate = circuit deschis / flacără activă)"
   - Temperature legend only above temperature graph.
4. Verify TPI Algorithm Inspector table:
   - Displays real metrics for each zone: target, current, P deficit, weather compensation %, learned duty %, total duty %, cycle minutes per hour.
5. All tests PASS on live board (`192.168.1.20`) and `sh_process_check.py` 0 errors.
