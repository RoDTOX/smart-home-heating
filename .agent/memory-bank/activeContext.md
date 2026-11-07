# Active Context

Only what holds now and what comes next. A line that no longer holds is replaced, not kept: history is in
[`docs/changelog.md`](../../docs/changelog.md) and git log; the lines of this file until 02.11.2026 are in
[`docs/archive/active_context_until_2026-11-02.md`](../../docs/archive/active_context_until_2026-11-02.md).

## Now
- **Night-Only Calibration & DHW Estimation (`PB-126`, `FR-35.1`)**:
  - Boiler burn rate is exclusively calibrated during night intervals (20:00-02:00 to 06:00-11:00) where DHW (apă caldă menajeră) is zero, isolating underfloor heating. Measured live rate: `0.132 m³/h`.
  - Domestic Hot Water (ACM) is estimated upon entering meter readings: `ΔIndex - (Runtime × Calibrated_Flow)`. Live total ACM identified: `0.85 m³`.
  - UI Card 3.3 displays real-time breakdown: Today's gas broken down into `♨️ Încălzire` (from runtime) and `🚿 ACM` (from index confirmation).
- **Operational Defect Fixes & Telemetry Enhancements (PB-125, PB-126)**:
  - **Relay Correction (PB-125)**: Living on Relay 1 (GPIO32), Kids on Relay 3 (GPIO25), Adults on Relay 2 (GPIO33). Verified live.
  - **Per-Zone Heating Type (FR-69, PB-125)**: Added `heatingType` (Pardoseală vs Calorifere) per zone in `ThermalZoneConfig`, `ParamStore` NVS migration, Web API, and UI Section 4.1. Underfloor zones enforce a 3.5h thermal lag threshold before triggering stagnation / heat misdirection DTCs.
  - **Cross-Zone Misdirection Detection**: Integrated into `PlantMonitor`, triggering alert if a demanding zone has no rise after 3.5h while an uncommanded closed zone overheats $\ge +0.5^\circ\text{C}$.
  - **Gas Meter Deletion, Editing & Daily Consumption Recalculation (PB-126)**: Added endpoints `/api/boiler/gas-meter/delete` and `/api/boiler/gas-meter/update`.
  - **UI Inspector & Footer Fixes**:
    - Fixed TPI duty display in Section 3.2 where 100% duty was erroneously rendered as 1%.
    - Standardized footer: `Smart Heating · FW v1.11.05.24 (f3e6287) · UI v1.11.07.01 (LittleFS)`.
    - Added instant auto-save on setup toggles (solar orientation, heating type, offset).
  - **Live Verification**: `pio run` clean, `sh_ota_upload.py` and `assets` flashed to `192.168.1.20`. 22/22 SWE.5-TC21 & TC46 atomic tests PASS (100%). `sh_process_check.py board` PASS (0 errors).
- **Cost, Token & Runaway Guard Hook (C-44, D-61)**: Added deterministic lifecycle hooks (`tools/sh_cost_guard.py`) in Claude Code (`.claude/settings.json`) and Antigravity (`.agent/hooks.json`). Enforces mandatory micro-plan + owner approval before non-trivial execution and blocks runaway execution loops exceeding 10 consecutive tool calls or 10 minutes.
- **Accepted Steps (1–12 & 14)**: All validated on board. Step 13 (probes) deferred with owner confirmation. Step 14 (all 6 advisors + PB-125 underfloor lag & PB-126 night gas/DHW telemetry) accepted and fully aligned with SW/System Requirements (`docs/requirements_and_features.md`) and Architecture (`docs/architecture.md`).
- **Live Board Status (`192.168.1.20`)**: FW `v1.11.05.24`, LittleFS UI `v1.11.07.01`. All zones nominal at 23.5°C target, 0 DTCs active, 42/42 regression tests (TC21, TC46–TC51) PASS. `sh_process_check.py` 0 errors.

## Next
1. **Repository Synchronization & Push**: Display git diff status and await explicit owner confirmation for `git push origin <branch>` per invariant.
2. **Night Observation**: Monitor the first autonomous overnight night-only gas calibration run on the board.
