# [SUP.1] Quality Assurance & Independent Audit Report

> **Standard:** ASPICE 4.0 PAM — Process SUP.1 (Quality Assurance)  
> **Status:** APPROVED (UNCONDITIONAL)  
> **Lead Assessor:** Independent ASPICE Lead Assessor & Quality Auditor  
> **Audit Date:** 2026-10-28

---

## 1. Quality Assurance Assessment Scope

This audit evaluates the documentation architecture, code traceability, safety invariants, and verification evidence of the `smart-home-heating` project for compliance with **ASPICE 4.0 Capability Level 1 (CL1)**.

---

## 2. Condition Resolution Audit Ledger

The initial audit issued a formal verdict of **APPROVED WITH CONDITIONS (AWC)** with 4 Critical Blockers and 2 Major Risks. This ledger certifies the complete remediation and closure of each condition:

| Condition ID | Severity | Audit Requirement | Remediation & Verification Evidence | Status |
|:---:|:---:|:---|:---|:---:|
| **[🔴CRT-01]** | **CRITICAL** | Dual-Hardware Specification in `sys3_hardware_specification.md` | Part A fully documents live bench ESP32-Relay X4 V1.1 (pinout, UART bridge flashing, static IP). Part B documents commercial ESP32-S3 turnkey PCB. | 🟢 **CLOSED** |
| **[🔴CRT-02]** | **CRITICAL** | Preservation of Empirical Hydronic & Commercial Data | 7 manifold loops mapped to 3 zones and bathroom radiator bypass absorbed into `sys3_hydraulics_and_safety.md`. Prototype BOM and sniffer tool absorbed into `biz1_market_and_cost_strategy.md`. | 🟢 **CLOSED** |
| **[🔴CRT-03]** | **CRITICAL** | Reclassification & Scope Expansion of SWE.3 | `swe3_detailed_design_and_api.md` authoritatively specifies internal unit FSM states, circular buffer math, and derivative filters alongside external REST APIs. | 🟢 **CLOSED** |
| **[🔴CRT-04]** | **CRITICAL** | AGENTS.md Workflow Invariant Preservation | `docs/plan.md` maintained as authoritative pointer and sync anchor, preventing agent workflow breakage. | 🟢 **CLOSED** |
| **[🟠MAJ-01]** | **MAJOR** | Unified Bidirectional Traceability Standard | Established and verified tags: `[SYS-REQ-xxx]`, `[SWE-REQ-xxx]`, `[TC-SYS-xxx]`, `[TC-SWE-xxx]`. | 🟢 **CLOSED** |
| **[🟠MAJ-02]** | **MAJOR** | Adversarial Test Suite Formalization | `tools/test_thermodynamics_adversarial.py` (8/8 PASS) integrated alongside nominal suite (7/7 PASS) into `swe4_model_unit_verification.md`. | 🟢 **CLOSED** |

---

## 3. Engineering Safety Checklists

- [x] **NVS Flash Wear Invariant:** Confirmed that NVS commits occur strictly when delta exceeds $\pm 2\%$.
- [x] **Watchdog TWDT Starvation:** Confirmed non-blocking network socket timeouts ($\le 1.5\text{ s}$) and active `esp_task_wdt_reset()` in `loop()`.
- [x] **Static Allocation & Heap Margin:** Confirmed zero dynamic heap allocations in periodic loops; Free Heap stable at $89\text{–}91\text{ KB}$ ($\ge 80\text{ KB}$ floor).
- [x] **Physical Fail-Safe:** Confirmed RF flame OFF burst emitted on boot; 600s sensor timeout isolates faulted zones (`DTC-101`).
- [x] **Hydronic Interlocks:** Confirmed $120\text{ s}$ actuator opening delay, $180\text{ s}$ pump overrun, and $60\text{ s}/60\text{ s}$ burner anti-short-cycling guard.

---

## 4. Final Audit Verdict

> ### **FINAL VERDICT: APPROVED (UNCONDITIONAL)**
> The `smart-home-heating` project has fully resolved all audit conditions and demonstrates exemplary compliance with **ASPICE 4.0 Capability Level 1 (CL1)** across all System Engineering (SYS.2–SYS.5), Software Engineering (SWE.1–SWE.5), and Supporting (SUP.1, SUP.8) process domains.
