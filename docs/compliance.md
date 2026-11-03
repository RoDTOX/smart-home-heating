# EU conformity (CE) - software

> Work product for selling the controller in the EU under the owner's brand (owner, 02.11.2026). The owner is the
> manufacturer in law for the whole device; the hardware is made to order by a factory. This file lists what EU
> law asks of the **software**, maps it to requirements and records the evidence. Checked by
> `sh-compliance-reviewer` (`docs/process.md` §1).
>
> Not legal advice. Before the first sale a conformity consultant or test lab confirms §2, the product category
> and the software class (CMP-40). Dates and acts: verify in EUR-Lex when a decision depends on them.

## 1. Position

| Item | Value |
|---|---|
| Manufacturer | the owner, own brand (02.11.2026) - responsible for hardware and software |
| Hardware | made to order by a factory; its lab reports are evidence for the hardware rows of §4 |
| Conformity route | self-declaration (internal production control, no notified body) where harmonised standards are applied in full - to confirm per act |
| CRA category | assumed *default* (a heating controller is not in the CRA lists of important or critical products) - to confirm |
| Market | EU |
| Status | not on the market; no unit sold |

## 2. EU acts

| Act | Covers | Applies | Evidence from |
|---|---|---|---|
| Radio Equipment Directive 2014/53/EU, Art. 3.1(a), 3.1(b), 3.2 | electrical safety, EMC, use of the radio spectrum (Wi-Fi, BLE, 433.92 MHz) | now | test lab on the hardware; software: CMP-50, CMP-51 |
| RED delegated regulation (EU) 2022/30, Art. 3.3(d)(e)(f), standards EN 18031-1/-2/-3 | network protection, personal data, fraud (not applicable: no payments) | units placed on the market 01.08.2025 - 10.12.2027; repealed when the CRA applies | software: §3.1 |
| Cyber Resilience Act (EU) 2024/2847 | product security, vulnerability handling, reporting | reporting (Art. 14) from 11.09.2026 for products on the market; essential requirements and CE from 11.12.2027 | software §3.1, §3.2; owner |
| RoHS 2011/65/EU | hazardous substances | now | factory declaration |
| WEEE 2012/19/EU | take-back, bin symbol, producer registration per country | now | owner, factory |
| General Product Safety Regulation (EU) 2023/988 | safety aspects no specific act covers, traceability, contact | now | owner |
| GDPR (EU) 2016/679 | personal data (room temperatures and presence patterns, Wi-Fi credentials) | now | software: CMP-8 - the location goes to the weather service, time from public servers, readings open on the LAN (PB-54, D48, D49) |

Indicative harmonised standards (the lab confirms the list): EN 60730-1 with EN 60730-2-9 (automatic electrical
controls, temperature sensing; software in Annex H), EN 301 489-1/-3/-17 (EMC of radio equipment), EN 300 328
(2.4 GHz), EN 300 220-1/-2 (433 MHz short-range devices), EN 62479 (RF exposure), EN 18031-1/-2 (cybersecurity
until 10.12.2027), the CRA standards in preparation.

## 3. Obligations on the software

Obligations are paraphrased with their source; the law's text is in EUR-Lex. `Requirements` cites the IDs of
`docs/requirements_and_features.md` that meet the obligation. Status: `open` (not assessed), `covered`
(a requirement asks for it and a test or a record verifies it), `partial`, `missing` (no requirement),
`n/a` (with the reason).

### 3.1 Product security (CRA Annex I part I; until 10.12.2027 RED 2022/30 via EN 18031)

| ID | Obligation | Source | Requirements | Status | Evidence |
|---|---|---|---|---|---|
| CMP-1 | A documented cybersecurity risk assessment, kept up to date; it decides how each row below applies | CRA Art. 13(2), (3) | — | missing | none; D41 and D49 wait for it |
| CMP-2 | No known exploitable vulnerability at release, in own code and in components (libraries, Arduino core, ESP-IDF) | CRA Annex I.1 (2)(a) | — | missing | no vulnerability check; `NimBLE-Arduino` is the unreleased 3.0.0-dev |
| CMP-3 | Secure by default configuration, with a reset to the original state; no universal default password | CRA Annex I.1 (2)(b); EN 18031-1 | FR-59 (conflict, D40), FR-66 | partial | PB-53 |
| CMP-4 | Security updates can be delivered; where applicable automatic by default with an opt-out, and the user is told | CRA Annex I.1 (2)(c) | FR-8.3 | partial | D43 |
| CMP-5 | Protection from unauthorised access (authentication, access control); possible unauthorised access is reported | CRA Annex I.1 (2)(d) | FR-58, FR-59, FR-65.3 | partial | FR-58 not built (step 9); PB-44, PB-45, PB-46; D49 (also the export, FR-65.1); configuration import / export without authentication until step 9 (PB-44) |
| CMP-6 | Confidentiality of stored and transmitted data (credentials, settings, traffic) | CRA Annex I.1 (2)(e) | — | missing | D41 |
| CMP-7 | Integrity of data, commands, firmware and configuration; corruption is reported | CRA Annex I.1 (2)(f) | FR-42, FR-53, FR-60, NFR-5.3, FR-65.3, FR-65.5, FR-65.7 | partial | PB-48, PB-52; D45 |
| CMP-8 | Only the data the function needs (data minimisation) | CRA Annex I.1 (2)(g) | — | missing | PB-54; D48 |
| CMP-9 | Essential functions stay available under attack or overload (heating continues) | CRA Annex I.1 (2)(h) | NFR-1.1, NFR-1.5, NFR-1.8, FR-41, FR-65.7 (import only) | partial | PB-47, PB-48, PB-49, PB-50, PB-56; control task not built (step 10) |
| CMP-10 | Minimal negative impact on other devices and on the network | CRA Annex I.1 (2)(i) | FR-17.2, FR-17.7, FR-9 | partial | PB-47 |
| CMP-11 | Limited attack surface, external interfaces included (ports, services, setup access point) | CRA Annex I.1 (2)(j) | — (FR-7.1, FR-45, FR-50 widen the surface) | missing | PB-43; D42 |
| CMP-12 | Exploitation mitigation: an incident has limited impact | CRA Annex I.1 (2)(k) | FR-42, FR-55, FR-58, NFR-5.1 | partial | PB-1 |
| CMP-13 | Security-relevant events (access, changes) are logged and can be monitored, with an opt-out | CRA Annex I.1 (2)(l) | — (FR-62: safety events only) | missing | PB-51 |
| CMP-14 | All data and settings can be removed securely and easily (factory reset) | CRA Annex I.1 (2)(m) | FR-66 (conflict, D44) | partial | PB-51 |

### 3.2 Vulnerability handling and manufacturer duties (CRA Annex I part II, Art. 13, 14, Annex II, VII)

| ID | Obligation | Source | Requirements | Status | Evidence |
|---|---|---|---|---|---|
| CMP-20 | Software bill of materials (SBOM), machine-readable, at least the top-level components | CRA Annex I.2 (1) | — | missing | none |
| CMP-21 | Vulnerabilities fixed without delay; security fixes separate from feature changes where possible | CRA Annex I.2 (2) | — | missing | D50 |
| CMP-22 | Regular security tests and reviews | CRA Annex I.2 (3) | — | missing | none |
| CMP-23 | Fixed vulnerabilities are disclosed once an update exists | CRA Annex I.2 (4) | — | missing | owner, before the first sale |
| CMP-24 | A coordinated vulnerability disclosure policy | CRA Annex I.2 (5) | — | missing | owner, before the first sale |
| CMP-25 | A contact address for reporting vulnerabilities | CRA Annex I.2 (6) | — | missing | owner, before the first sale |
| CMP-26 | Updates are distributed securely | CRA Annex I.2 (7) | FR-8.3, FR-60 | partial | PB-52; D43 |
| CMP-27 | Security updates free of charge, with advice to the user | CRA Annex I.2 (8) | — | missing | owner, before the first sale |
| CMP-28 | A declared support period, by default at least five years, shown to the buyer | CRA Art. 13(8) | — | missing | D46 |
| CMP-29 | Actively exploited vulnerabilities and severe incidents reported through the ENISA platform (early warning, notification, final report) | CRA Art. 14 | — | missing | due at the first sale |
| CMP-30 | Technical documentation and EU declaration of conformity, kept for the period the law sets | CRA Art. 13, Annex VII | — | missing | owner, before the first sale |
| CMP-31 | User information: manufacturer and contact, intended use, end of support, how to install updates and how to reset before disposal | CRA Annex II | — | missing | owner, before the first sale |

### 3.3 Safety of the control software (RED Art. 3.1(a); EN 60730-1 Annex H)

| ID | Obligation | Source | Requirements | Status | Evidence |
|---|---|---|---|---|---|
| CMP-40 | The software class (A, B or C) of each safety function is decided and recorded - to confirm with the lab; working assumption: class B for the hydraulic safety functions (AGENTS.md §3) | EN 60730-1 Annex H | — | missing | working assumption only; D47 |
| CMP-41 | For class B: faults of CPU, program flow, interrupts, clock, program memory, data memory, internal and external communication and I/O that could cause an unsafe state are detected, and the controller goes to its safe state | EN 60730-1 Annex H | NFR-1.4, NFR-1.5, FR-16.2, FR-53, FR-55, FR-60, NFR-5.3 | partial | watchdog covers `loop()` only; no memory or clock self-test; D47 |
| CMP-42 | A documented development process (specification, design, verification, traceability) - `docs/process.md` is the candidate evidence | EN 60730-1 Annex H | — (`docs/process.md`) | partial | no step record yet; unit verification (SWE.4) not applied |

### 3.4 Radio (RED Art. 3.2)

| ID | Obligation | Source | Requirements | Status | Evidence |
|---|---|---|---|---|---|
| CMP-50 | The 433.92 MHz transmitter stays within the duty-cycle and power limits of its short-range-device band; the software decides how often frames are sent (commands, heartbeat, repeats) | RED Art. 3.2; EN 300 220 | FR-5.1, FR-17.2, FR-17.6, FR-17.7 | partial | PB-47; no on-air analysis |
| CMP-51 | Wi-Fi and BLE stay within the settings of the certified radio module (EU country, power, channels) | RED Art. 3.2; EN 300 328 | — | missing | Wi-Fi country and power not set; BLE observer only, never transmits |

## 4. Technical file

| Item | From | Status |
|---|---|---|
| Cybersecurity risk assessment (CMP-1) | software | missing |
| SBOM (CMP-20) | software | missing |
| Security assessment against EN 18031-1/-2, later the CRA (§3.1) | software | missing |
| Software class and Annex H measures (CMP-40, CMP-41) | software, lab | missing |
| Radio, EMC and electrical safety test reports (RED Art. 3.1, 3.2) | lab via factory | missing |
| RoHS declaration | factory | missing |
| Vulnerability disclosure policy and contact (CMP-24, CMP-25) | owner | missing |
| User manual in the language of each country, with the CRA Annex II information | owner | missing |
| EU declaration of conformity, CE marking, product label | owner | missing |
| WEEE producer registration in each country of sale | owner | missing |
| Traceability (type, batch or serial) and manufacturer contact on the product (GPSR) | owner, factory | missing |

## 5. Rules

- An obligation becomes a requirement only through the owner's chain (`sh-change` step 2): the owner decides, the
  requirement is written by `docs/process.md` §2, and this file then cites its ID. Values stay in requirements §6.
- A status changes only with evidence named in its row.
- A change of the law, a date that passed or a confirmation by the lab is a row in §6.

## 6. Changes

| Date | Change | By |
|---|---|---|
| 02.11.2026 | Created: the owner sells the device in the EU under their brand | owner (decision), Claude |
| 02.11.2026 | First full review: statuses of §3, GDPR row of §2, WEEE and GPSR rows of §4; findings in `requirements_review.md` §10 (D40 ... D50) and PB-43 ... PB-54 | `sh-compliance-reviewer`, owner (log) |
| 02.11.2026 | FR-65 (configuration export / import) checked before it was written (C-9): import joins FR-58, credentials never in the file, size bound, JSON only, no cross-origin header; CMP-5, CMP-7, CMP-9 cite it; HTTP body gap PB-56. For the risk assessment (CMP-1): export = household profile (location, room names, sensor IDs, schedule), import = full reconfiguration. For the user manual (CMP-31): the export file holds no password but should be kept private | `sh-compliance-reviewer`, Claude (chain) |
| 03.11.2026 | Console login (C-30, FR-75 ... FR-77): the development build (`SH_DEV_OPEN`) keeps console, Telnet and `/update` open (owner, no USB cable in the development phase). **Release blocker (owner): a build with `SH_DEV_OPEN` can never be sold**; `python tools/sh_process_check.py release` fails while the release profile or any header defines it (FR-75.6, PB-104). CMP-3, CMP-5, CMP-11 stay `partial` until a release build with the login is tested | owner (decision), Claude |
