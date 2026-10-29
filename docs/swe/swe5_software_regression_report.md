# [SWE.5] Software Regression & Traceability Report

> **Standard:** ASPICE 4.0 PAM — Process SWE.5 (Software Regression Testing and Verification)  
> **Status:** RELEASED / PASSED (100.0%)  
> **Traceability:** Bidirectional Matrix linking `[SWE-REQ-xxx]` to Automated Tests

---

## 1. Bidirectional Requirements Traceability Matrix

Every software requirement defined in `[SWE.1]` is directly traceable to an automated test case in `tools/test_system.py`, `tools/test_thermodynamics.py`, or `tools/test_thermodynamics_adversarial.py`:

| Software Requirement ID | Requirement Title | Verifying Test Suite | Test Case Identifier | Verdict |
|:---|:---|:---|:---|:---:|
| **[SWE-REQ-001]** | Multi-Zone FSM State Machine | `test_system.py` | `GRP-02-TC01` .. `TC04` | **PASS** |
| **[SWE-REQ-002]** | Actuator Polarity (NC/NO) Configuration | `test_system.py` | `GRP-25-TC01` | **PASS** |
| **[SWE-REQ-003]** | Hysteresis Band Evaluation | `test_system.py` | `GRP-03-TC01` .. `TC04` | **PASS** |
| **[SWE-REQ-004]** | Actuator Opening Interlock Delay | `test_system.py` | `GRP-04-TC01` .. `TC03` | **PASS** |
| **[SWE-REQ-005]** | Circulation Pump Overrun Timer | `test_system.py` | `GRP-05-TC01` .. `TC03` | **PASS** |
| **[SWE-REQ-006]** | RF 433.92 MHz OOK Pulse Train | `test_system.py` | `GRP-01-TC01` .. `TC03` | **PASS** |
| **[SWE-REQ-007]** | Idempotency Filter (Zero Redundant RF) | `test_system.py` | `GRP-01-TC04` | **PASS** |
| **[SWE-REQ-008]** | Transmitter 3s Cooldown Guard | `test_system.py` | `GRP-07-TC01` | **PASS** |
| **[SWE-REQ-009]** | Anti-Short-Cycling Burner Guard (60s/60s) | `test_system.py` | `GRP-06-TC01` .. `TC04` | **PASS** |
| **[SWE-REQ-010]** | Power-Up Failsafe Flame OFF Burst | `test_system.py` | `GRP-01-TC05` | **PASS** |
| **[SWE-REQ-011]** | NimBLE Passive Observation (BTHome v2) | `test_system.py` | `GRP-09-TC01` .. `TC04` | **PASS** |
| **[SWE-REQ-012]** | Boot Warmup Grace Period (300s) | `test_system.py` | `GRP-09-TC03` | **PASS** |
| **[SWE-REQ-013]** | Sensor Loss Failsafe Isolation (DTC-101) | `test_system.py` | `GRP-10-TC01` .. `TC03` | **PASS** |
| **[SWE-REQ-014]** | 2R2C State-Space Model Implementation | `test_thermodynamics.py` | `SWE.4-TH01` | **PASS** |
| **[SWE-REQ-015]** | Autonomous Night-Time Calibration ($k_{\text{loss}}$)| `test_thermodynamics.py` | `SWE.4-TH01` | **PASS** |
| **[SWE-REQ-016]** | Step-Response Parameter Identification | `test_thermodynamics.py` | `SWE.4-TH02` | **PASS** |
| **[SWE-REQ-017]** | Dynamic Pre-Heat Lead Time Accuracy | `test_thermodynamics.py` | `SWE.4-TH03` | **PASS** |
| **[SWE-REQ-018]** | 12-Sample Circular FIFO Buffer | `test_thermodynamics.py` | `SWE.4-TH04` | **PASS** |
| **[SWE-REQ-019]** | Numerical 1st & 2nd Derivative Estimators | `test_thermodynamics.py` | `SWE.4-TH04`, `TH05` | **PASS** |
| **[SWE-REQ-020]** | Rapid Open-Window Detection Threshold | `test_thermodynamics.py` | `SWE.4-TH04`, `TH05` | **PASS** |
| **[SWE-REQ-021]** | Window Auto-Recovery on Rebound | `test_thermodynamics.py` | `SWE.4-TH07` | **PASS** |
| **[SWE-NFR-001]** | OTA Binary Slot Budget $\le 1.25\text{ MB}$ | PlatformIO Build Log | `firmware.bin` size check | **PASS** |
| **[SWE-NFR-002]** | Free Heap RAM $\ge 80\text{ KB}$ | `test_system.py` | `GRP-21-TC02` | **PASS** |
| **[SWE-NFR-003]** | REST API Response Latency $< 100\text{ ms}$ | `test_system.py` | All REST queries | **PASS** |
| **[SWE-NFR-004]** | NVS Delta-Write Flash Wear Protection | `test_system.py` | `GRP-16-TC01` | **PASS** |
| **[SWE-NFR-005]** | Static Allocation Invariant (No heap leaks) | PlatformIO Inspect | Symbol size analysis | **PASS** |
| **[SWE-NFR-006]** | Hardware Task Watchdog (TWDT 30s) Guard | `test_system.py` | `GRP-20-TC01` | **PASS** |

---

## 2. Traceability Metrics & Coverage Summary

- **Total Software Functional Requirements:** 21 / 21 covered (**100.0% coverage**).
- **Total Non-Functional Requirements:** 6 / 6 verified (**100.0% coverage**).
- **Orphaned Requirements:** 0.
- **Untraced Code Artifacts:** 0.
- **Regression Status:** Clean 0 defects across all execution pipelines.
