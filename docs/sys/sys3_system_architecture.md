# [SYS.3] System Architectural Design

> **Standard:** ASPICE 4.0 PAM — Process SYS.3 (System Architectural Design)  
> **Status:** RELEASED / BASELINED  
> **Traceability Upward:** Fulfills `[SYS-REQ-001]` through `[SYS-REQ-020]`

---

## 1. System Physical Context & Topology

The system comprises three physical layers: Sensing, Edge Control, and Actuation/Cloud Telemetry.

```
                           +----------------------------------------+
                           |       Cloud Platform / Multi-Tenant    |
                           |   mysmarthome.cc / app.mysmarthome.cc   |
                           +-------------------+--------------------+
                                               ^
                                               | Outbound Reverse Tunnel (No port-forward)
                                               v
+-----------------------+          +-----------+------------+          +------------------------+
|  Xiaomi BLE Sensors   |  2.4GHz  |   ESP32 Smart Heating  |  433MHz  |  Gas Boiler Receiver   |
|  - Kids Room          | -------> |   Controller (Core)    | -------> |  - Flame On/Off        |
|  - Matrimonial Room   | (Passive |   IP: 192.168.1.20     |   OOK    |  - Bistable contact    |
|  - Living & Kitchen   |  BTHome) |   - FreeRTOS Dual-Core |  Burst   +------------------------+
+-----------------------+          |   - 1.25MB OTA + LittleFS      |
                                   +-----------+------------+
                                               | 230V AC
                                               v GPIO Switch
                                   +-----------+------------+
                                   | Electrothermal Actuator|
                                   | 230V NC M30x1.5 Wax    |
                                   | - Living Loops (1,2,3) |
                                   | - Kids Loops (6,7)     |
                                   | - Matrimonial (4,5)    |
                                   +------------------------+
```

---

## 2. Hardware Subsystems Breakdown

### 2.1 Edge Microcontroller Core
- **Target Microcontroller:** Espressif ESP32 dual-core Xtensa LX6 @ 240 MHz (ESP32-WROOM-32E / ESP32-S3).
- **Storage & Partitions:** 4 MB / 8 MB Flash partitioned into:
  - `app0` (1.25 MB): Primary active firmware slot.
  - `app1` (1.25 MB): OTA staging update slot.
  - `spiffs` / `littlefs` (1.375 MB): File system for web UI (`index.html.gz`), historical telemetry ring buffers (`events.bin`, `history24h.bin`), and configuration passports.
  - `nvs` (20 KB): Non-volatile parameter storage (setpoints, calibration coefficients, Wi-Fi credentials).

### 2.2 Wireless Sensor Ingestion Subsystem (NimBLE)
- **Mechanism:** Passive BLE observation (zero connection establishment, zero peripheral battery drain).
- **Ingestion Format:** BTHome v2 unencrypted advertisement payloads from Xiaomi Mijia LYWSD03MMC sensors.
- **Acquisition Grace Period:** At system boot, a $300\text{ s}$ warmup window is initiated (`ZONE_WARMUP`). Zones transition immediately to active control as soon as all assigned sensors report.

### 2.3 RF Boiler Trigger Subsystem
- **Transmitter Hardware:** SYN115 433.92 MHz ASK/OOK PLL-stabilized transmitter connected to dedicated GPIO (GPIO19 on prototype).
- **Modulation Standard:** Precise bit-banging OOK pulse train:
  - 65 ms preamble burst.
  - 409 discrete pulse transitions (`ondurations` / `offdurations`).
  - Total frame duration: ~131 ms.
  - WDT Feeding: `esp_task_wdt_reset()` fed before transmission to prevent watchdog starvation during bit-banging.

### 2.4 Hydronic Actuator Driver Subsystem
- **Actuator Type:** Normally Closed (NC) 230V AC thermal wax actuators with M30x1.5 thread.
- **Relay Driving:** Optoisolated mechanical relays (ESP32-Relay X4) or Zero-Cross Triac Solid State Relays (MOC3043M + BTA16-600B on custom commercial board).
- **Polarity Invariant:** Active state energizes the wax actuator; de-energization defaults to spring-loaded closed state.

---

## 3. External Cloud & Reverse Tunnel Architecture

Fulfilling **[SYS-REQ-020]**, the controller communicates with the multi-tenant SaaS cloud platform (`mysmarthome.cc`) without requiring client-side router port forwarding or static public IPs.

### 3.1 Network Topology & Cloudflare / Tailscale Routing

```
                       [ INTERNET / CLOUDFLARE EDGE ]
                                     |
         +---------------------------+---------------------------+
         |                                                       |
         v                                                       v
[ mysmarthome.cc ]                                      [ app.mysmarthome.cc ]
Public Showcase & Marketing                             Tenant Web Portal (Next.js/PWA)
                                                                 |
                                                                 v
                                                        [ Ingestion Gateway ]
                                                        api.mysmarthome.cc
                                                                 ^
                                                                 | Outbound WebSocket /
                                                                 | Reverse HTTP Tunnel
                                                                 |
                                                    +------------+------------+
                                                    | Residential Client LAN  |
                                                    | ESP32 Heating Unit      |
                                                    | IP: 192.168.1.20        |
                                                    +-------------------------+
```

### 3.2 Tenant Isolation & Fleet Security
1. **Outbound-Only Tunnel:** The ESP32 initiates an outbound TLS connection to `api.mysmarthome.cc`. Incoming commands (setpoints, Away mode) flow over this established bi-directional channel.
2. **Device Passport Token:** Authentication is performed via unique Device Serial (`SH-14721A0D5020`) and cryptographically signed bearer tokens stored in ESP32 NVS flash.
3. **Telemetry Push Cadence:** High-priority events (flame toggles, DTC fault codes, window open alerts) are pushed instantly; regular climatic telemetry is pushed every 60s.

---

## 4. Architectural Safety Invariants & Interlocks

| Hazard Code | Failure Scenario | Architectural Mitigation | Verification Standard |
|:---|:---|:---|:---|
| **HAZ-01** | Boiler deadheading against closed valves | Actuator 120–180s interlock delay before RF flame ignition command | `[SYS-REQ-003]` / SWE.5 HIL |
| **HAZ-02** | Residual heat exchanger boil-over | 180s circulation pump overrun through open bathroom towel radiators | `[SYS-REQ-004]` / SWE.5 HIL |
| **HAZ-03** | Rapid burner relay chat / short cycling | Enforced 60s minimum burn / 60s minimum pause guard | `[SYS-REQ-005]` / SWE.5 HIL |
| **HAZ-04** | Room sensor battery death / signal loss | 10-minute timeout isolation (`DTC-101`), auto-recovery on reception | `[SYS-REQ-007]` / SWE.5 HIL |
| **HAZ-05** | Power blackout recovery | RF flame OFF signal emitted at boot; NVS preset restoration | `[SYS-REQ-017]` / SWE.5 HIL |
| **HAZ-06** | Microcontroller FreeRTOS task deadlock | ESP32 Hardware Task Watchdog (TWDT) set to 30s timeout | `[SYS-REQ-018]` / SWE.5 HIL |

---

## 5. Architectural Allocation Matrix

| Architectural Module | Implemented Files | Allocated Requirements |
|:---|:---|:---|
| **Hydronic Zone Controller** | `include/ThermalZoneManager.h`, `src/ThermalZoneManager.cpp` | `[SYS-REQ-001]`, `[SYS-REQ-003]`, `[SYS-REQ-004]`, `[SYS-REQ-006]`, `[SYS-REQ-008]` |
| **Boiler Radio Transmitter** | `include/BoilerRadioTrigger.h`, `src/BoilerRadioTrigger.cpp` | `[SYS-REQ-002]`, `[SYS-REQ-005]`, `[SYS-REQ-017]` |
| **Thermodynamic AI Engine** | `include/ThermodynamicEngine.h`, `src/ThermodynamicEngine.cpp` | `[SYS-REQ-009]`, `[SYS-REQ-010]`, `[SYS-REQ-011]` |
| **BLE Sensor Scanner** | `include/BleSensorScanner.h`, `src/BleSensorScanner.cpp` | `[SYS-REQ-001]`, `[SYS-REQ-007]` |
| **Diagnostic & Watchdog** | `include/DiagnosticEngine.h`, `src/DiagnosticEngine.cpp` | `[SYS-REQ-007]`, `[SYS-REQ-018]`, `[SYS-REQ-019]` |
| **LittleFS Web & REST API** | `src/main.cpp`, `web/index.html` | `[SYS-REQ-012]`, `[SYS-REQ-013]`, `[SYS-REQ-015]`, `[SYS-REQ-016]` |
| **Cloud & Reverse Tunnel** | `include/DevicePassport.h`, `src/DevicePassport.cpp` | `[SYS-REQ-020]` |
