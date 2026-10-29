# Project Context Router: Smart Home Heating System

> **Repository:** `smart-home-heating`  
> **Target Hardware:** ESP32-WROOM-32E (Bench: `192.168.1.20`) / ESP32-S3 (Production 8Z PCBA)  
> **Compliance Framework:** ASPICE 4.0 Capability Level 1 (CL1)  
> **Active Baseline:** `BL_2026_v1.10.28.1` | Firmware: `v1.10.28.1`  
> **Role:** Master Index & Context Router for AI Coding Agents

---

## 🧭 Canonical ASPICE 4.0 Process Directory

For all software development, architecture review, and verification tasks, consult the canonical process documents below:

### 1. System Engineering Domain (`docs/sys/`)
- [`sys2_system_requirements.md`](file:///d:/_git/smart-home-heating/docs/sys/sys2_system_requirements.md): System Requirements Specification (`[SYS-REQ-001]` to `[SYS-REQ-020]`), Operational Design Domain (ODD), hydraulic limits, and radio environments.
- [`sys3_system_architecture.md`](file:///d:/_git/smart-home-heating/docs/sys/sys3_system_architecture.md): System Architecture, element decomposition, outbound cloud reverse tunnel, and external interfaces.
- [`sys3_hardware_specification.md`](file:///d:/_git/smart-home-heating/docs/sys/sys3_hardware_specification.md): Dual-Variant Hardware Spec (Part A: Bench ESP32-Relay X4 V1.1; Part B: Turnkey Commercial ESP32-S3 8-Zone PCBA with zero-cross SSRs & OpenTherm).
- [`sys3_hydraulics_and_safety.md`](file:///d:/_git/smart-home-heating/docs/sys/sys3_hydraulics_and_safety.md): Hydronic Physics, 7-loop manifold mapping to 3 zones, natural towel radiator bypass, actuator opening delays, and pump overrun.
- [`sys4_system_integration_test_spec.md`](file:///d:/_git/smart-home-heating/docs/sys/sys4_system_integration_test_spec.md): 4-Tier Test Architecture, boundary limits, and Pairwise Cross-Feature Interaction Matrix (`TC-X01` to `TC-X10`).
- [`sys5_system_qualification_report.md`](file:///d:/_git/smart-home-heating/docs/sys/sys5_system_qualification_report.md): Formal Hardware-in-the-Loop (HIL) Qualification Report (95/95 PASS on live ESP32 at `192.168.1.20`).

### 2. Software Engineering Domain (`docs/swe/`)
- [`swe1_software_requirements.md`](file:///d:/_git/smart-home-heating/docs/swe/swe1_software_requirements.md): Software Requirements (`[SWE-REQ-001]` to `[SWE-REQ-021]`), NFRs, DTC fault register (`DTC-101` to `DTC-402`), and 2R2C formulas.
- [`swe2_software_architecture.md`](file:///d:/_git/smart-home-heating/docs/swe/swe2_software_architecture.md): C++ Module Decomposition, FreeRTOS dual-core tasks, memory budgets (1.25MB OTA app slot, 1.375MB LittleFS partition).
- [`swe3_detailed_design_and_api.md`](file:///d:/_git/smart-home-heating/docs/swe/swe3_detailed_design_and_api.md): Detailed Unit Design (ThermalZoneManager FSM, 12-sample circular buffer, numerical derivative filters) & complete REST/JSON API contract.
- [`swe4_model_unit_verification.md`](file:///d:/_git/smart-home-heating/docs/swe/swe4_model_unit_verification.md): 2R2C Unit Verification (7/7 Nominal Tests + 8/8 Adversarial Tests, 100% PASS).
- [`swe5_software_regression_report.md`](file:///d:/_git/smart-home-heating/docs/swe/swe5_software_regression_report.md): Bidirectional Traceability Matrix linking all `[SWE-REQ-xxx]` requirements to automated test cases.

### 3. Supporting & Management Processes (`docs/sup/`, `docs/biz/`)
- [`sup1_quality_assurance_audit.md`](file:///d:/_git/smart-home-heating/docs/sup/sup1_quality_assurance_audit.md): Independent Lead Assessor Quality Audit Report (Verdict: APPROVED UNCONDITIONAL).
- [`sup8_baseline_BL_2026_v1.10.28.yaml`](file:///d:/_git/smart-home-heating/docs/sup/sup8_baseline_BL_2026_v1.10.28.yaml): Cryptographic Baseline Configuration Ledger.
- [`biz1_market_and_cost_strategy.md`](file:///d:/_git/smart-home-heating/docs/biz/biz1_market_and_cost_strategy.md): Commercial Strategy, BOM itemization ($75.80 USD at MOQ 1000), competitor benchmark, and field installer sniffer tool.
- [`docs/changelog.md`](file:///d:/_git/smart-home-heating/docs/changelog.md): Official revision history.

---

## 🛠️ Engineering Rules for AI Coding Agents
1. **Zero AI Slop:** Never create unreviewed root markdown summaries or speculative abstractions.
2. **Ladder of Laziness:** Always inspect existing C++ modules before writing new routines.
3. **Partition Budget:** The compiled binary must strictly fit inside the $1.25\text{ MB}$ ($1,310,720\text{ B}$) OTA application slot with $\ge 15\text{ KB}$ free headroom.
4. **Hydronic Failsafes:** Never bypass actuator interlock delay ($120\text{ s}$), pump overrun ($180\text{ s}$), or burner anti-short-cycling guard ($60\text{ s}$).
5. **NVS Wear Protection:** Commit to flash strictly when value delta exceeds $\pm 2\%$.
