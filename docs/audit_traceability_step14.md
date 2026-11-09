# ASPICE Traceability & Coverage Audit — Migration Phase 4 / Step 14

> Audit report for ASPICE SWE.1 (Software Requirements) ↔ SWE.2 (Software Architecture) ↔ SWE.3 (Software Detail Design & Unit Construction) ↔ SWE.5 (Software Integration & Verification).
> Generated on 09.11.2026. Evaluates baseline v2.10 requirements against test suite and codebase.

---

## 1. Executive Summary

| Metric | Measured Value | Standard Target | Status |
|---|---|---|---|
| **Total Requirement IDs** | 149 (125 leaf requirements) | 125 | Complete specification |
| **Atomic SWE.5 Verification Tests** | 265 automated test cases | — | Comprehensive test coverage |
| **Requirements with Dedicated Tests** | **83 / 112 testable leaves (74.1%)** | ≥ 90% | Action needed: 29 requirement gaps |
| **Code Implementation Tags (SWE.3)** | **100 / 125 leaf requirements (80.0%)** | ≥ 85% | 25 requirements lack explicit traceability tags |
| **Module Architectural Allocation (SWE.2)** | **129 / 131 requirements (98.5%)** | 100% | High architectural compliance |
| **Process Checker Verdict** | 0 errors, 1 known, 63 warnings | 0 errors | PASS |

---

## 2. Requirement Coverage Analysis & Classification

### 2.1 Untested Requirements (11 Leaf Requirements)

The audit identified 11 leaf requirements that currently have no dedicated automated test case:

1. **Physical & Deferred Hardware**:
   - `FR-50` (Setup Access Point): Triggered only when configured WiFi is inaccessible.
   - `FR-54` (Flow Temperature Limit): Deferred by owner (no physical flow sensors installed).
   - `FR-72` (Additional BLE Receivers): Tagged `[later]`, multi-receiver arbitration pending hardware.
2. **Maintenance & Device Reset**:
   - `FR-49` (Sensor Offset): Configurable in API and UI, but missing dedicated assertion test suite.
   - `FR-61` (Operation Without Valid NTP Time): Fallback behavior present in code, untested in SWE.5.
   - `FR-66` (Factory Reset): Tagged `[later]`, hardware button reset workflow pending.
3. **Security & Authentication (CRA / RED)**:
   - `FR-75.3` (Credential Storage in NVS): Password hash/salt persistence test missing.
   - `FR-75.4` (Brute Force Lockout): 5 consecutive invalid logins resulting in client IP lockout.
4. **Non-Functional Requirements (NFR)**:
   - `NFR-2.3` (API Response Time): API endpoints respond in < 200 ms under nominal load.
   - `NFR-2.6` (No Heap Drift): Heap memory stability across 24h continuous operation.
   - `NFR-3.1` (Bilingual UI Support): Localization strings for Romanian and English.
   - `NFR-4.1` (Comfort Temperature Accuracy): ±0.3 °C steady-state comfort tracking.

---

## 3. Test Quality Audit: "Hollow" Test Findings

The static trace analyzer flagged 27 test cases as "hollow" (checking HTTP status code without validating payload values or checking values stated in test titles):

- **Command & Auth Tests (`t_commands_auth.py`)**: `TC41.2`, `TC41.5` assert only HTTP status without inspecting response JSON; `TC41.6` (stack headroom 4 KB) and `TC41.7` (heap thresholds 60 KB / 75 KB) do not assert numerical values.
- **Hydraulics & Control Tests (`t_hydraulics.py`, `t_control.py`)**: `TC07.3`, `TC10.1`, `TC03.4` lack payload assertions; `TC07.4` promises 12°C frost threshold check but only asserts response format.
- **Modes & Safety Features (`t_modes.py`, `t_safety_features.py`)**: `TC24.1`, `TC36.1`, `TC36.4`, `TC36.5` have title-to-assertion value discrepancies.

---

## 4. Architectural Rules Compliance (SWE.2 R-A1 .. R-A7)

- **R-A7 Violations (Dynamic `String` in L2–L4 control loop)**:
  - `ThermalZoneManager.cpp`, `PlantMonitor.cpp`, and `TimeAndWeatherService.cpp` contain dynamic `String` operations.
  - *Risk*: Heap fragmentation during continuous long-term execution.
  - *Mitigation Plan*: Refactor hot-path logging and status strings to fixed buffers or `printf`-style streams in Phase 5.1.2.

---

## 5. Remediation Plan

1. **Step 1 (Immediate)**: Strengthen `TC41.2` assertions and implement dedicated `SWE.5-TC49.1` test for `FR-49` (Sensor Offset calibration).
2. **Step 2 (Security & Lockout)**: Implement automated test for `FR-75.4` (Brute force lockout).
3. **Step 3 (Heap Hardening)**: Replace dynamic `String` allocations in `PlantMonitor` and `ThermalZoneManager` with bounded character arrays.
