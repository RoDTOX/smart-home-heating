# [BIZ.1] Commercial Market Strategy & Scalable BOM Economics

> **Document Type:** Commercial Positioning, Pricing Benchmark & Manufacturing Economics  
> **Status:** RELEASED / BASELINED  
> **Traceability:** Links to `[SYS.3]` Hardware Element Specification  
> **Condition Adherence:** Complies with Independent Quality Audit Condition **[🔴CRT-02]**

---

## 1. Market Opportunity & Competitive Landscape

Underfloor hydronic heating is the dominant heating distribution system in new residential developments across Central and Eastern Europe. However, existing multi-zone control solutions suffer from high commercial markups, closed proprietary protocols, or clunky cloud dependencies:

| Solution Provider | Architecture Type | Typical Cost (3–4 Zones) | Key Bottlenecks & Limitations |
|:---|:---|:---:|:---|
| **Salus IT600** | Proprietary Zigbee Hub + Hardwired Base | $350 – $450 USD | Proprietary ecosystem; expensive expansion modules; closed API. |
| **Tado Starter + TRVs** | Cloud-dependent Bridge | $300 – $400 USD | Inflexible for underfloor manifolds; subscription required for open-window detection; high cloud latency. |
| **Honeywell Evohome** | Proprietary RAMSES-II 868MHz | $500 – $750 USD | Prohibitive retail price; complex commissioning; vendor lock-in. |
| **Tuya Zigbee Kit** | Generic Cloud Tuya Gateway | $180 – $250 USD | Dependent on Chinese cloud servers; relay chatter (no zero-cross); poor hydronic failsafes. |
| **MySmartHome Heating** | **Autonomous Edge ESP32 + Web/REST** | **$75.80 USD (BOM) / $149 USD (Retail)** | **100% local autonomy; 0dB silent SSRs; 2R2C predictive calibration; sub-100ms API; zero cloud lock-in.** |

---

## 2. Turnkey Manufacturing Economics (Custom 8-Zone PCBA)

The custom production board integrates an ESP32-S3 microcontroller, 8 solid-state zero-cross relays, an isolated OpenTherm master interface, an auxiliary dry-contact SPDT relay, and an external bulkhead SMA antenna:

| Subsystem Component | Technical Specification | Unit Cost @ MOQ 1000 |
|:---|:---|:---:|
| **Main Processing & RF** | ESP32-S3-WROOM-1-N8R2 (8MB Flash, 2MB PSRAM) + SYN115 433.92MHz | $3.45 USD |
| **AC-DC Isolated Power** | 230V AC to 5V DC SMPS module + 3.3V LDO + MOV & fuse | $2.60 USD |
| **8x Solid State Switching** | 8x MOC3043M zero-cross optocouplers + 8x BTA16-600B Triacs + RC snubbers | $6.20 USD |
| **OpenTherm Master Interface** | EN 50559 compliant isolated circuit (6N137 optocouplers, transistors, passives) | $1.85 USD |
| **PCB & Connectors** | 4-layer FR4 ENIG PCB + 5.08mm industrial pluggable terminal blocks + DIN enclosure | $5.90 USD |
| **SMT Pick-and-Place & Test**| Automated assembly, AOI, automated flashing and functional test fixture | $2.80 USD |
| **External Antenna Solution** | 30cm RG178 IPEX-to-SMA pigtail + external 5dBi rubber duck antenna | $1.80 USD |
| **8x Thermal Actuators** | 8x 230V NC M30x1.5 electrothermal wax actuators (OEM factory pricing) | $25.60 USD ($3.20/ea) |
| **8x Wireless Room Sensors** | 8x Xiaomi Mijia BLE 5.0 LCD digital thermometers (BTHome v2) | $25.60 USD ($3.20/ea) |
| **TOTAL TURNKEY KIT BOM** | **Complete 8-Zone Smart Heating Package** | **$75.80 USD (~348.7 RON)** |

*Target Compliance:* The strict $< \$85.00\text{ USD}$ engineering constraint is fulfilled with a **10.8% cost margin**.

---

## 3. Prototype Bench BOM (Local Romanian Sourcing Baseline)

For bench prototyping and field pilots, the system was originally fabricated using locally sourced off-the-shelf components:

| Prototype Component | Sourcing Channel | Cost (RON) | Purpose in System |
|:---|:---|:---:|:---|
| **ESP32-Relay X4 V1.1** | Optimus Digital / eMAG | ~85 RON | Primary test bench controller (`192.168.1.20`) |
| **SYN115 + SYN480R Set** | Optimus Digital | ~25 RON | 433.92 MHz radio transmitter & receiver module |
| **3x Xiaomi LYWSD03MMC** | AliExpress / eMAG | ~90 RON (30 RON/ea) | Digital wireless room sensors (Kids, Bed, Living) |
| **3x NC M30x1.5 Actuators** | eMAG (Beok / Salus / Watts) | ~180 RON (60 RON/ea)| 230V electrothermal wax actuators on manifold loops |
| **Connectors & Wires** | WAGO 221-413 / 221-415 | ~40 RON | Safe 230V AC distribution in manifold cabinet |
| **Wall Tablet Kiosk** | OLX (Samsung Galaxy Tab A9 LTE)| 450 RON | Dedicated zero-scroll wall kiosk display |
| **TOTAL PROTOTYPE COST** | **Local Retail Procurement** | **~870 RON (~$190 USD)** | Rapid bench verification without custom tooling |

---

## 4. Field Installation Kits & Installer Tooling

To ensure zero-friction customer installation, two dedicated companion hardware tools are designed:

### 4.1 Universal Boiler Receiver Module (Add-On Kit)
- **Target Application:** Gas boilers that lack built-in 433.92 MHz wireless receivers and only accept dry-contact thermostat inputs.
- **Hardware:** Ultra-compact ESP8266 / ESP32-C3 module powered from boiler 230V terminal block with a 10A dry-contact relay.
- **Protocol:** Encrypted ESP-NOW or 433.92 MHz pairing with main heating controller. Eliminates wiring between manifold and boiler.

### 4.2 Portable "Heating Sniffer" 433MHz Field Tool
- **Target Application:** Field tool for HVAC technicians and installers to capture customer RF thermostat timings on-site.
- **Hardware:** Handheld pocket tool comprising an ESP32, SYN480R superheterodyne receiver, and 0.96" OLED display (~45 RON total parts).
- **Operation:** When the customer presses their existing thermostat, the sniffer instantly records the pulse transitions (`ondurations`/`offdurations`) and uploads the configuration to the controller.

---

## 5. Monetization & SaaS Business Model

1. **Hardware Margin:** Turnkey 8-zone kit manufactured at **$75.80 USD** and retailed at **$149.00 USD (~685 RON)**, generating a healthy **49.1% gross hardware margin**.
2. **Installation Service:** Turnkey installation, sensor mapping, and screed calibration provided by certified local HVAC partners for ~350–500 RON per apartment.
3. **Optional SaaS Cloud Tier (`mysmarthome.cc`):**
   - *Free Tier:* 100% full local control, local Web UI, LAN REST API, and local telemetry.
   - *Premium Cloud Tier (5 EUR/month):* Multi-tenant remote access over Cloudflare/Tailscale tunnel, unlimited telemetry history, push notifications on phone, and AI thermal efficiency reports.
