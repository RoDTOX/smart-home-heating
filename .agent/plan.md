# Step 11 — History in LittleFS + StatusModel static buffer

## Original request (verbatim)

Step 7 am verificat vizual cat se poate. Step 10 nu o sa verific ce poti verifica tu, astea sunt verificabile
automat prin teste, nu au nevoie de testare manuala. fa ce faci si apoi trecem la step 11.

## Goal (architecture §18, step 11)

Close G28 (telemetry rings from RAM to LittleFS) and G29 (JSON built with `String` → `StatusModel` static buffer).
Done when: heap ≥ 100 KB with 32 zones configured on the bench.

## What is already done (partial acceptance 04.11.2026)

- Streamed JSON for `/api/history` (AR-15) — chunked response, no full String in RAM
- Web page served from LittleFS (FR-8.5)
- Serial buffer reduced to 1 KB
- `/api/diag/mem` endpoint (FR-10.6)
- Weather task stack 6 KB → 4 KB

## What remains

### Part A — G28: Telemetry rings from RAM to LittleFS (AR-15)

Today: `_ring24h[288]` (2,880 B) and `_ring5d[240]` (2,400 B) live entirely in RAM = **5,280 B**.
Target: only the current record in RAM (~10 B); rings read from/written to LittleFS files.

Design:
1. Keep the current slot in RAM (one `TelemetryPoint` per ring = 20 B total).
2. On a 5-min boundary, write the completed slot to the LittleFS file at the correct offset (append/seek).
3. `streamHistoryJson()` already streams — change it to read from the file sequentially instead of the RAM array.
4. `boilerOnSeconds()`, `gasFlowMeasured()`, runtime counters: read from file, not RAM array.
5. Remove the two arrays from the class → saves **5,260 B heap**.
6. `loadFromFlash()` / `saveToFlash()`: no longer bulk-copy arrays; file IS the persistence.

### Part B — G29: StatusModel static buffer

Today: every API handler in WebApi.cpp builds a `String` (106 uses) → heap fragmentation.
Target: a 4 KB static `char[]` buffer; JSON written with `snprintf` / ArduinoJson `serializeJson` into it.

Design:
1. Create `StatusModel.h` / `StatusModel.cpp` with a static 4 KB buffer.
2. The control tick (step 6 of the snapshot I16) populates the snapshot; `StatusModel` formats it.
3. Migrate the biggest endpoints first: `/api/status`, `/api/zones` (each ~1.5 KB JSON).
4. Then smaller endpoints: `/api/schedule`, `/api/config/*`, `/api/diag/*`.
5. `server.send()` with `const char*` instead of `String`.

### Part C — Verification

1. `pio run` clean
2. `python tools/sh_process_check.py` 0 ERROR
3. Free heap ≥ 100 KB (NFR-2.2) — measure with `/api/diag/mem`
4. Flash size ≤ 1,306,624 B with margin ≥ 20 KB (NFR-2.8)
5. System tests pass on board

## Stage tracker

- [x] A1: Design ring-on-disk file layout (seek offsets, header)
- [x] A2: Implement file-based write (updateSlots writes to LittleFS at offset)
- [x] A3: Implement file-based read for streamHistoryJson
- [x] A4: Implement file-based read for boilerOnSeconds, runtime counters
- [x] A5: Remove RAM arrays, keep only current slot
- [x] A6: Test: build clean, flash, verify history still works (TC37.1..TC37.2 100% PASS)
- [x] B1: Create StatusModel.h/cpp with static buffer
- [x] B2: Migrate /api/status to static buffer
- [x] B3: Migrate /api/zones to static buffer
- [x] B4: Migrate remaining endpoints
- [x] B5: Remove String usage from migrated handlers
- [x] C1: Full verification (build, process check, heap, flash, board test)
