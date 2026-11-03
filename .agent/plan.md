# Agent Plan Reference

Authoritative plan: [`docs/plan.md`](../docs/plan.md) (named by AGENTS.md §2). Current phase status is tracked there.
This file only points to it. On 03.11.2026 another AI tool (Google AGY) overwrote it with a work plan of its own for
one task; that text is kept below unchanged as history (process review, `docs/review_agy_03_11.md`). Its stages are
done on the board (a9c5175) outside the chain; they are not migration steps.

Details: [`hardware_esp32_relay.md`](../docs/hardware_esp32_relay.md),
[`underfloor_heating_hydraulics_and_failsafes.md`](../docs/underfloor_heating_hydraulics_and_failsafes.md).

---

## History: AGY work plan of 03.11.2026

## Original request (verbatim)
"perfect, planuiesti sa facem aici si un / sh process check , ca as vrea sa il bag si pe claude la inaintare, de pe celalalt GUI al Anthropic, Opus 5.5/Sonnet 5.5

sper ca pe grafic pui sidetectia de geam deschis... urmeaza sa se detecteze pe camera adultilor.. am dat drumul la geam acum.. sper sa mearga. 

markul ala de RESTART arata ca naiba, putea sa fie mai mic si mai elegant, un fulger ceva,

iar in zona de instrumente inginerie si debug, era bun un log window/console... care sa printeze si sa imi arate.. ce mai face si pe unde se invarte softu`."

## Stage tracker
- [ ] Stage 1: Elegant Minimalist Restart Marker (`web/index.html` - replace bulky 66px orange rectangular badge with a discreet 13px circular pin containing `⚡`, subtle 1px dashed line, clear hover tooltip with reset reason)
- [ ] Stage 2: Open Window Detection on History Chart & Room Cards (`include/TelemetryHistoryManager.h`, `src/TelemetryHistoryManager.cpp`, `web/index.html` - track active window pause per zone in telemetry flags/JSON, display cyan aeration indicator on zone activity row, room card badge with countdown and instant cancel button)
- [ ] Stage 3: Sliding Baseline Window Detection Optimization (`src/ThermalZoneManager.cpp` - track peak temperature reference over rolling window so that gradual or mid-cycle temperature drops aren't reset prematurely by fixed 5-minute boundaries)
- [ ] Stage 4: Live Console & Log Window in Engineering/Debug (`web/index.html` - add interactive live console streaming `/api/console/logs?since=...`, auto-scroll toggle, clear button, command input using `POST /api/console/cmd`)
- [ ] Stage 5: Build, Verify with `tools/sh_process_check.py`, OTA Flash & Live Board Testing
- [ ] Stage 6: Update Documentation & Handoff Summary for Claude

## Verification steps
1. Verify restart marker is visually lightweight (small circular `⚡` pin, no bulky overlapping orange box).
2. Verify open window detection is displayed clearly on room card (cyan border, countdown, cancel button) and on telemetry chart lane.
3. Verify live console stream in "Instrumente Inginerie & Debug" receives and appends logs, scrolls automatically, and allows command submission.
4. Verify `pio run` builds clean (<1.25 MB flash limit).
5. Verify `python tools/sh_process_check.py` passes with 0 errors.
6. Verify live board responses after OTA flashing on `192.168.1.20`.
