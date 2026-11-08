# Step 14 Review — Phase 4 Optimization Advisors & Thermal Plant Telemetry

> ASPICE MAN.3 / SWE.1–SWE.6 step review record for Migration Step 14 (Phase 4).
> In accordance with [`process.md`](process.md) §9 and [`architecture.md`](architecture.md) §13, §18.

Status: **Accepted on Board** (owner confirmed 07.11.2026 / 08.11.2026).

---

## 1. Summary of Scope & Built Modules

Step 14 introduces the optional, non-safety phase-4 optimization advisors operating under the strict contract of `architecture.md` §13 (bounded advice, time-boxed ≤ 2 ms, static memory ≤ 2 KB RAM, safe fallback to baseline):

1. **`PlantMonitor` (FR-56, FR-57, PB-125)**:
   - Detects abnormal plant behavior: valve stuck open (`DTC-203` `ERR_ZONE_OVERHEAT`), zone heating without effect (`DTC-202` `ERR_ZONE_NO_EFFECT`), and boiler not heating (`DTC-204`).
   - Differentiated thermal inertia: 3.5 h threshold (`12600000 ms`) for underfloor heating (`floor`) vs 45 min for radiators (`radiators`).
   - Cross-zone heat misdirection detection: raises `DTC-202` if a zone has demand for ≥ 3.5 h without temperature rise while an uncommanded, closed zone overheats by ≥ +0.5 °C.
   - Idempotent single transport re-send of ON command (`BoilerLink::resend`) after 15 min before escalating.
2. **`HeatUpModel` (FR-67)**:
   - Learns thermal inertia and heating lead time (°C/h vs outdoor temperature).
   - Pre-heat lead time dynamically bounded in [45..240] min; safe fallback to 90 min on lost weather data.
3. **`SmartWindow` (FR-40.2)**:
   - Evaluates multi-variate sliding window: indoor temperature slope, absolute humidity drop, indoor-outdoor $\Delta T$.
   - Suspends detection if indoor-outdoor difference < 5 °C; falls back seamlessly to FR-40 baseline.
4. **`CycleAligner` (FR-63)**:
   - Aligns TPI cycle phases across zones to enforce minimum concurrent thermal load (`minLoops` parameter).
5. **`SolarThrottle` (FR-70)**:
   - Adjusts effective TPI duty based on forecasted solar irradiance, preventing floor overheating on solar gain zones.
6. **`HumidityComp` (FR-71)**:
   - Compensates comfort target downward by up to −0.5 °C when relative humidity exceeds 60% RH.
7. **Plant Telemetry & Gas Accounting Enhancements (FR-35.1, PB-126)**:
   - Exclusive night-interval calibration (20:00–02:00 to 06:00–11:00) isolating underfloor heating runtime (zero domestic hot water).
   - Real-time Domestic Hot Water (ACM) consumption estimation on meter index input: $\Delta\text{Index} - (\text{Runtime} \times \text{Calibrated\_Rate})$.
   - Gas meter reading management endpoints (`/api/boiler/gas-meter/delete`, `/api/boiler/gas-meter/update`).
   - Section 5.2 compact Hardware Health dashboard (CPU freq, core temp, OTA slot, I/O pin levels).

---

## 2. ASPICE Verification Evidence

| Item | Result / Verification Evidence |
|---|---|
| **Requirements** | FR-35.1, FR-40.2, FR-56, FR-57, FR-63, FR-67, FR-69, FR-70, FR-71, FR-10.6, NFR-5.3 in [`requirements_and_features.md`](requirements_and_features.md). |
| **Architecture Contract** | Modules implemented per `architecture.md` §13 & §18. No advisor resides in safety loop; all Clamp/Fallbacks active. |
| **Atomic SWE.5 Tests** | 42/42 PASS on live board (`192.168.1.20`):<br>• `SWE.5-TC21.1..17` (Gas telemetry, night calibration, ACM): 17/17 PASS<br>• `SWE.5-TC46.1..5` (`PlantMonitor` & Cross-Zone misdirection): 5/5 PASS<br>• `SWE.5-TC47.1..4` (`HeatUpModel`): 4/4 PASS<br>• `SWE.5-TC48.1..4` (`SolarThrottle`): 4/4 PASS<br>• `SWE.5-TC49.1..4` (`CycleAligner`): 4/4 PASS<br>• `SWE.5-TC50.1..4` (`HumidityComp`): 4/4 PASS<br>• `SWE.5-TC51.1..4` (`SmartWindow`): 4/4 PASS |
| **Process Check** | `python tools/sh_process_check.py`: **0 errors, PASS**.<br>`python tools/sh_process_check.py board`: **0 errors, PASS**. |
| **Hardware Resources** | RAM: internal free heap 88–106 KB (within NFR-2.2 bounds).<br>Flash: 1 299 257 B (OTA slot margin 7.3 KB under limit 1 306 624 B). |
| **Owner Hands-on Test** | Night calibration verified live (`0.132 m³/h`, 0.85 m³ total ACM tracked). All 3 zones stable at 23.5 °C. 0 DTCs. |

---

## 3. Residual Risks & Future Work

- **R-5 (Recorded window traces)**: `SmartWindow` parameters (`windowK`, `windowAhDrop`) will continue to be refined over the heating season using historical telemetry logs.
- **Backlog Triage**: Defect [PB-116](problems.md) resolved and closed (commit `0d86a27`). Residual minor items (e.g. PB-119 1-minute chart resolution) remain tracked for commercial release.

---

## 4. Sign-off

- **Architecture Migration Step 14**: **ACCEPTED & CLOSED**
- **Date**: 08.11.2026
