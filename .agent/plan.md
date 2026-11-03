# Implementation Plan: Gas Meter Index Tracking & Boiler Flow Temperature Reference (FR-35.1)

## Original request (verbatim)
"vreau sa tinem log si al consumului centralei, in indexul de pe contor si al temperaturii setate la centrala.

acum am 2265.304m3 si centrala e setata la 40*C, dar vad ca baga cu 43*C."

## Stage tracker
- [ ] Stage 1: Requirements & Parameter Specifications (`docs/requirements_and_features.md` §3.5 FR-35.1 and §6 configuration parameters)
- [ ] Stage 2: ASPICE Verification Tests First (`tools/sh_tests/t_ui_history.py` - SWE.5-TC21.3 for gas meter tracking & boiler setpoint)
- [ ] Stage 3: ParamStore & State Extensions (`include/ParamStore.h`, `src/ParamStore.cpp` - `boilerSetTemp`, `boilerFlowObserved`, `gasFlameRate`, `GasMeterBaseIndex`, `GasMeterBaseSec`, `GasMeterBaseEpoch`)
- [ ] Stage 4: WebApi Endpoints (`src/WebApi.cpp` - expose in `/api/boiler/status`, add POST `/api/boiler/gas-meter` to calibrate/update meter index)
- [ ] Stage 5: Web UI Gas Meter & Boiler Flow Widget (`web/index.html` - real-time calculated meter index, consumption today/yesterday/month in m³, quick index calibration input, boiler dial setpoint 40°C vs 43°C observed)
- [ ] Stage 6: Process Checks, Build, Tests & Flashing (`pio run`, `tools/sh_process_check.py`, `tools/sh_ota_upload.py`, live board verification)

## Verification steps
1. Run `python tools/sh_process_check.py` to ensure requirements §6 and `ParamStore.cpp` are 100% in sync with 0 errors.
2. Build firmware clean with `pio run`.
3. Flash to board via `python tools/sh_ota_upload.py`.
4. Verify `/api/boiler/status` returns `gasMeter` schema with estimated index `2265.304 m³`, `boilerSetTemp: 40`, `boilerFlowObserved: 43`.
5. Test updating index via `POST /api/boiler/gas-meter` and verify NVS persistence.
6. Verify live Web UI card displays current meter index, gas consumed today, and boiler flow reference.
