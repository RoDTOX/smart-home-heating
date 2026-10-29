# ASPICE 4.0 Capability Level 1 Documentation Restructuring & Code Traceability Plan

## Original request (verbatim)
> "sa nu uitam sa facem curat in documentatie.. am creat foarte multe schimbari in acest proiect, am facut ai slop, am lucrat sigur pe niste fisiere si acum ai lucrat si tu pe altele, nu sunt multumit nici de ale mele , iar de ale tale si agentilor nu am idee ce e acolo... deci trebuie o echipa serioasa de analiza, critica, research , ISO si ASPICE compliant .. sa faca update si remove. CLEANUP."
> "sablonul de fisiere as vrea sa aiba in nume si procesul eg. sys2_requirements.md , daca e posibil si se poate urmari peste toate.. dac anu nu."
> "Eu sunt de acord si aprob planul, dar momentan ruleaza victory auditor si trebuie sa validez si eu rezultatele modificarilor de azi. Apoi cred ca implementarea planului curent de curatare si conformitate ASPICE 4, implica modificari la nivel de fisiere sursa , care sa includa trasabilitate in cod , deci , asteptam putin."
> "trecem la etapa urmatoare, da !"

---

## Executive Architecture & Strategy
This plan executes the complete migration of all engineering documentation to **ASPICE 4.0 PAM Capability Level 1 (CL1)**.
It strictly enforces the Independent Lead Auditor's findings and conditions:
- **[🔴CRT-01] Dual-Hardware Specification**: Preserves live ESP32-Relay X4 V1.1 bench target while documenting future ESP32-S3 turnkey board.
- **[🔴CRT-02] Formal Absorption of Orphaned Documents**: DTC fault dictionary (`DTC-101`..`DTC-402`), multi-tenant SaaS cloud topology, 7 manifold loops / towel radiator bypass, prototype BOM and field installer sniffer.
- **[🔴CRT-03] SWE.3 Detailed Unit Design**: Full detailed design of software units (FSM stateflow, circular FIFO buffer, numerical derivative estimators) plus external REST/LittleFS contracts.
- **[🔴CRT-04] AGENTS.md Protocol Preservation**: Atomic update and pointer maintenance for `docs/plan.md`.
- **[🟠MAJ-01] Unified Traceability Standard**: `[SYS-REQ-xxx]`, `[SWE-REQ-xxx]`, `[TC-SYS-xxx]`, `[TC-SWE-xxx]`.
- **[🟠MAJ-02] Adversarial Test Evidence**: Integration of both nominal (7/7) and adversarial (8/8) test suites into SWE.4 verification evidence.
- **Source Code Traceability**: Direct inline requirement citations (`// [SWE-REQ-xxx]`) across firmware units.
- **Zero Data Loss Invariant**: Deletions occur ONLY after full migration and verification.

---

## Stage Tracker

### Stage 1: Directory Setup & System Engineering (SYS.2 - SYS.5)
- [x] Create `docs/sys/` directory.
- [x] Create `docs/sys/sys2_system_requirements.md` (ODD, hydraulic parameters, 433MHz RF & NimBLE radio environment, `[SYS-REQ-001]`..`[SYS-REQ-020]`).
- [x] Create `docs/sys/sys3_system_architecture.md` (System topology, cloud outbound reverse tunnel, external interfaces from `architecture_multitenant_iot_saas.md`).
- [x] Create `docs/sys/sys3_hardware_specification.md` (Dual Variant: Part A Bench ESP32-Relay X4 V1.1 + Part B Commercial ESP32-S3 8Z BOM).
- [x] Create `docs/sys/sys3_hydraulics_and_safety.md` (7 manifold loops / 3 zones, bathroom radiator bypass, wax actuator interlock 120s, pump overrun 180s, short-cycling 60s).
- [x] Create `docs/sys/sys4_system_integration_test_spec.md` (Tier 1-4 test architecture from `TEST_INFRA.md`, pairwise interaction matrix `TC-X01`..`TC-X10`).
- [x] Create `docs/sys/sys5_system_qualification_report.md` (Live HIL qualification report against ESP32 at `192.168.1.20`, 95 tests).

### Stage 2: Software Engineering (SWE.1 - SWE.5)
- [x] Create `docs/swe/` directory.
- [x] Create `docs/swe/swe1_software_requirements.md` (Functional `[SWE-REQ-001]`..`[SWE-REQ-045]`, NFRs, DTC fault register `DTC-101`..`DTC-402`).
- [x] Create `docs/swe/swe2_software_architecture.md` (C++ module decomposition, FreeRTOS tasks, memory budgets: 1.25MB OTA app slot, LittleFS partition).
- [x] Create `docs/swe/swe3_detailed_design_and_api.md` (FSM state diagrams, 12-sample circular buffer, numerical derivative filter, REST/JSON API contract).
- [x] Create `docs/swe/swe4_model_unit_verification.md` (2R2C Unit tests: 7/7 nominal + 8/8 adversarial test evidence).
- [x] Create `docs/swe/swe5_software_regression_report.md` (Bidirectional traceability matrix SWE.1 -> SWE.5).

### Stage 3: Supporting Processes & Commercial Strategy (SUP.1, SUP.8, BIZ.1)
- [x] Create `docs/sup/` directory.
- [x] Create `docs/sup/sup1_quality_assurance_audit.md` (Formal Independent Stage 2 Audit report & safety checklists).
- [x] Create `docs/sup/sup8_baseline_BL_2026_v1.10.28.yaml` (Cryptographic baseline ledger: Git commit, binary size, LittleFS image).
- [x] Create `docs/biz/` directory.
- [x] Create `docs/biz/biz1_market_and_cost_strategy.md` (BOM economics, competitor benchmark, field installer sniffer tool).

### Stage 4: Root Router & Protocol Alignment
- [x] Update root `PROJECT.md` as lightweight context router & ASPICE process index.
- [x] Update root `PLAN.md` (MAN.3 Project Management & Release Roadmap).
- [x] Update `docs/plan.md` pointer.
- [x] Update `docs/changelog.md`.

### Stage 5: Code-Level Traceability Tagging
- [x] Add `// [SWE-REQ-xxx]` traceability tags to `include/ThermodynamicEngine.h` & `src/ThermodynamicEngine.cpp`.
- [x] Add `// [SWE-REQ-xxx]` traceability tags to `include/ThermalZoneManager.h` & `src/ThermalZoneManager.cpp`.
- [x] Add `// [SWE-REQ-xxx]` traceability tags to `include/BoilerRadioTrigger.h` & `src/BoilerRadioTrigger.cpp`.
- [x] Add `// [SWE-REQ-xxx]` traceability tags to `include/DiagnosticEngine.h` & `src/DiagnosticEngine.cpp`.
- [x] Verify PlatformIO build: `pio run` (SUCCESS).
- [x] Verify test suite: `python -X utf8 tools/test_thermodynamics.py` (7/7 PASS).

### Stage 6: Safe Deletion of Redundant Slop Files (Confirmation Boundary)
- [x] Verify all critical hardware, manifold, and DTC data is 100% captured in canonical docs.
- [x] Delete obsolete files:
  - `TEST_INFRA.md`, `TEST_READY.md`
  - `docs/session_summary_2026-10-06.md`
  - `docs/bom_hardware_costs.md`
  - `docs/hardware_esp32_relay.md`
  - `docs/research_thermal_inertia_and_adaptive_preheat.md`
  - `docs/remote_fleet_and_service_mode.md`
  - `docs/architecture_multitenant_iot_saas.md`
  - `docs/hardware_china_8zone.md`
  - `docs/market_analysis_and_roi.md`, `docs/plan_sales.md`
  - `.agent/coordination/` and `.agents/` temporary directories.
- [x] Refresh `.agent/memory-bank/` (`activeContext.md`, `progress.md`, `decisions.md`).
