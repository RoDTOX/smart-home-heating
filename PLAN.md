# [MAN.3] Project Management & Release Roadmap

> **Standard:** ASPICE 4.0 PAM — Process MAN.3 (Project Management)  
> **Status:** ACTIVE / BASELINE BL_2026_v1.10.28.1  
> **Authoritative Release Roadmap:** Master tracking document for milestones, capability gates, and operational sprints.

---

## 1. Executive Release Milestones

| Milestone ID | Target Scope & Capability Gate | Status | Verification Gate |
|:---:|:---|:---:|:---:|
| **M1** | **Core Hydronics & 433MHz Boiler Trigger** <br>• ESP32-Relay X4 hardware bringing up, OOK radio trigger, 3-zone thermal control, failsafe interlocks. | **DONE** | 100% PASS on HIL suite (v1.2.0) |
| **M2** | **Circadian Bio-Schedule & Web Kiosk UX** <br>• 4-anchor dynamic schedule, custom presets, PWA dashboard for Galaxy Tab A9, LittleFS asset serving. | **DONE** | DevTools WCAG AA & CDP live tests (v1.10.27.4) |
| **M3** | **Thermodynamic AI & Rapid Window Engine** <br>• 2R2C state-space model, autonomous night cooling calibration ($k_{\text{loss}}$), Away step test, rapid derivative open-window detection. | **DONE** | 7/7 Nominal + 8/8 Adversarial PASS (v1.10.28.1) |
| **M4** | **Turnkey Hardware Specification & Production BOM** <br>• Custom ESP32-S3 8-zone PCBA with zero-cross SSRs, isolated OpenTherm master, external SMA antenna, BOM $< \$85\text{ USD}$ ($75.80 USD achieved). | **DONE** | Independent Victory Audit APPROVED |
| **M5** | **ASPICE 4.0 Capability Level 1 (CL1) Restructuring** <br>• Complete purge of AI slop, canonical `sys/`, `swe/`, `sup/`, `biz/` structure, bidirectional code traceability, cryptographic baseline ledger. | **ACTIVE** | Stage 2 Independent Audit APPROVED |
| **M6** | **Hardware PCBA Prototype Fabrication & Field Pilot** <br>• Gerber generation, JLCPCB SMT pilot run (10 units), OpenTherm physical testing with Viessmann/Bosch boilers. | **PLANNED** | Pilot deployment & field telemetry |

---

## 2. Active Sprint Focus: ASPICE 4.0 Restructuring & Traceability

- **Objective:** Finalize documentation consolidation, inline code requirement tags, and purge obsolete temporary slop files.
- **Verification Invariant:** Clean build (`pio run`), 100% test pass on simulation harnesses (`test_thermodynamics.py`), zero data loss from historical field notes.
