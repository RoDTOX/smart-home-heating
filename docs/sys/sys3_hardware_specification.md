# [SYS.3] Hardware Engineering Specification (Dual-Platform Architecture)

> **Standard:** ASPICE 4.0 PAM — Process SYS.3 (System Architectural Design / Hardware Element Specification)  
> **Status:** RELEASED / BASELINED  
> **Traceability:** Fulfills `[SYS-REQ-001]`, `[SYS-REQ-002]`, `[SYS-REQ-014]`, `[SYS-REQ-015]`  
> **Condition Adherence:** Complies with Independent Quality Audit Condition **[🔴CRT-01]**

---

## Executive Summary: Dual-Platform Strategy
This specification governs two distinct hardware tiers:
- **Part A: Operational Test-Bench Platform (ESP32-Relay X4 V1.1)** — The active physical board deployed on the hardware test bench and live in the test apartment, responding at `192.168.1.20`.
- **Part B: Commercial Turnkey Platform (ESP32-S3 8-Zone PCBA)** — The modular, mass-production hardware specification designed for factory fabrication, zero-cross silent switching, OpenTherm bus control, and Faraday-cage signal bypass.

---

# PART A: Operational Test-Bench Target (ESP32-Relay X4 V1.1)

### A.1 Physical Board Identity & Specifications
- **Module Identity:** DOIT WiFi MODEL ESP32-32E N4 (ESP32-WROOM-32E / ESP32-D0WD-V3).
- **CPU:** Xtensa® Dual-Core 32-bit LX6 @ 240 MHz.
- **Embedded Flash:** 4 MB SPI Flash.
- **SRAM:** 520 KB SRAM (320 KB usable heap).
- **Hardware MAC Address:** `20:50:0d:1a:72:14`.
- **Device Serial Number:** `SH-14721A0D5020`.
- **Power Supply Input:** Dual-mode AC 90–250V AC or DC 7–30V / 5V DC via terminal block.

### A.2 Complete Physical Pinout & GPIO Mapping

| Function | ESP32 GPIO | Electrical Level | Description / Circuit Assignment |
|:---|:---:|:---:|:---|
| **Relay 1** | `GPIO32` | Active HIGH | Zone 1: Living & Kitchen (Manifold loops 1, 2, 3 in parallel) |
| **Relay 2** | `GPIO33` | Active HIGH | Zone 2: Kids Room (Manifold loops 6, 7 in parallel) |
| **Relay 3** | `GPIO25` | Active HIGH | Zone 3: Master Bedroom (Manifold loops 4, 5 in parallel) |
| **Relay 4** | `GPIO26` | Active HIGH | Secondary circulation pump or unassigned spare |
| **RF 433.92 MHz DATA** | `GPIO19` | 3.3V Logic | Connected directly to DATA pin of SYN115 transmitter |
| **Status LED** | `GPIO23` | Active LOW | System heartbeat and BLE acquisition indicator |
| **UART0 TX** | `GPIO1` | 3.3V TTL | Serial CLI & boot logs (115200 baud) |
| **UART0 RX** | `GPIO3` | 3.3V TTL | Serial command ingestion |
| **Boot Mode Select** | `GPIO0` | Pull-up | Tactile push button on PCB for flash download mode |

### A.3 Flashing Procedure via USB-UART Bridge
When flashing firmware via physical USB cable:
1. Connect a NodeMCU / CH340 USB-UART bridge to the programming header (`5V`, `GND`, `TX`, `RX`).
2. Bridge `RST` to `GND` on the NodeMCU to put the bridge's own ESP into reset.
3. Hold `IO0` button on ESP32-Relay X4 board, press `RST`, release `IO0` to enter ROM bootloader download mode.
4. Execute PlatformIO flash: `pio run -e esp32dev --target upload --upload-port COM6` (baud: 460800).

### A.4 Network Configuration & Web OTA Endpoint
- **Local Static Lease:** `192.168.1.20` assigned via MAC `20:50:0d:1a:72:14` on residential ZTE gateway (`192.168.1.1`).
- **Web OTA Endpoint:** `POST http://192.168.1.20/update`
  - Authentication: HTTP Basic Auth (`admin` / `21051990`).
  - Automated Paced Script: [`tools/ota_upload.py`](file:///d:/_git/smart-home-heating/tools/ota_upload.py).
  - Maximum OTA Application Slot: **1,310,720 bytes (1.25 MB)**.

---

# PART B: Modular Commercial Platform (ESP32-S3 8-Zone PCBA)

### B.1 Core Architecture & Silicon
- **SoC:** Espressif ESP32-S3-WROOM-1-N8R2 (Xtensa® Dual-Core LX7 @ 240 MHz, Vector AI instructions).
- **Memory Footprint:** 8 MB Quad-SPI Flash + 2 MB Octal PSRAM.
- **Power Ingress:** Integrated 230V AC to 5V/3.3V DC SMPS (isolated Mornsun LS05-13B05R3 or Hi-Link HLK-5M05).

### B.2 Solid State Zero-Cross Switching (8 Zones)
- **Driver Circuit:** 8 independent channels utilizing `MOC3043M` zero-cross phototriac optocouplers driving `BTA16-600B` snubberless triacs (rated 16A, 600V).
- **Protection Circuit:** Each channel includes an RC snubber ($39\,\Omega + 10\,\text{nF}$ 630V) and a $14\text{D}471\text{K}$ Metal Oxide Varistor (MOV) for spike suppression.
- **Acoustic Signature:** $0\text{ dB}$ (silent solid-state operation inside residential bedroom manifolds).

### B.3 Boiler Communication Interfaces
1. **Wireless 433.92 MHz Transmitter:** SYN115 PLL-stabilized chip with 50-ohm trace to SMA bulkhead.
2. **Galvanically Isolated OpenTherm Master (EN 50559):** Active current loop interface ($I_{\text{high}} = 17\text{–}23\text{ mA}$, $I_{\text{low}} = 5\text{–}9\text{ mA}$) with dual optocouplers (PC817 / 6N137). Enables modulating water temperature ($35^\circ\text{C}\text{–}45^\circ\text{C}$) to maximize condensing efficiency.
3. **Auxiliary Dry-Contact SPDT Relay:** Songle 10A 250V AC relay for legacy non-RF boilers.

### B.4 Faraday Cage Signal Bypass (Antenna Bracket Solution)
- Underfloor heating manifolds are enclosed in $1.0\text{ mm}$ thick sheet-metal cabinets, causing $-32\text{ dB}$ attenuation on internal PCB antennas.
- **Solution:** Mainboard provides an IPEX/U.FL connector routed via a $30\text{ cm}$ RG178 pigtail to an external SMA female bulkhead bracket mounted on the outside of the metal cabinet.
- **Antenna:** External 5 dBi omnidirectional dipole antenna, yielding $+32\text{ dB}$ link margin improvement.

### B.5 Turnkey Bill of Materials (BOM) Economics

| Item Category | Component / Module | Unit Cost (MOQ 1000) |
|:---|:---|:---:|
| **Processing & RF** | ESP32-S3-WROOM-1-N8R2 (8MB/2MB) + SYN115 + RF circuitry | $3.45 USD |
| **Power Stage** | AC-DC 230V to 5V SMPS + LDO 3.3V + MOV/Fuse | $2.60 USD |
| **Actuation Stage** | 8x MOC3043M + 8x BTA16-600B + Snubbers + SPDT Relay | $6.20 USD |
| **OpenTherm Bus** | Isolated OpenTherm Master circuit (6N137, transistors, passives) | $1.85 USD |
| **PCB & Connectors** | 4-Layer FR4 ENIG PCB + 5.08mm Screw Terminals + Enclosure | $5.90 USD |
| **SMT Assembly** | PCBA manufacturing, inspection, and functional test | $2.80 USD |
| **Antenna Kit** | 30cm RG178 IPEX-SMA pigtail + 5dBi external antenna | $1.80 USD |
| **Actuators (8x)** | 8x 230V NC M30x1.5 electrothermal wax actuators | $25.60 USD ($3.20/ea) |
| **Sensors (8x)** | 8x Xiaomi BLE 5.0 wireless temperature/humidity monitors | $25.60 USD ($3.20/ea) |
| **TOTAL TURNKEY KIT** | **Complete 8-Zone Smart Heating Package** | **$75.80 USD (~348.7 RON)** |

*Cost Target:* Target BOM was $< \$85.00\text{ USD}$. Achieved cost is **$75.80 USD**, beating target by $10.8\%$.
