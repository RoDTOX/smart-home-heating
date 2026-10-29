# [SWE.2] Software Architectural Design

> **Standard:** ASPICE 4.0 PAM — Process SWE.2 (Software Architectural Design)  
> **Status:** RELEASED / BASELINED  
> **Traceability Upward:** Fulfills `[SWE-REQ-001]` through `[SWE-REQ-021]` and `[SWE-NFR-001]` through `[SWE-NFR-006]`

---

## 1. Software Module Decomposition & Dependencies

The firmware architecture is structured as a collection of decoupled, statically allocated C++ classes executed deterministically within the ESP32 Arduino / FreeRTOS environment.

```
                                  +-----------------------+
                                  |       main.cpp        |
                                  |  - WebServer (Port 80)|
                                  |  - REST Dispatcher    |
                                  |  - TWDT Watchdog Feed |
                                  +-----------+-----------+
                                              |
     +-----------------+----------------------+---------------------+-----------------+
     |                 |                      |                     |                 |
     v                 v                      v                     v                 v
+------------+  +--------------+     +------------------+    +-------------+   +---------------+
| BleSensor  |  | TimeAnd      |     | ThermalZone      |    | BoilerRadio |   | Diagnostic    |
| Scanner    |  | WeatherSvc   |     | Manager          |    | Trigger     |   | Engine        |
| - NimBLE   |  | - NTP Client |     | - FSM States     |    | - 433.92MHz |   | - DTC 101-402 |
| - BTHome v2|  | - Open-Meteo |     | - Hysteresis Loop|    | - OOK Train |   | - Self-Test   |
+------------+  +--------------+     +--------+---------+    | - Cooldown  |   +---------------+
                                              |              +-------------+
                                              v
                                     +------------------+
                                     | Thermodynamic    |
                                     | Engine           |
                                     | - 2R2C Model     |
                                     | - Night Calib    |
                                     | - Window Detect  |
                                     +------------------+
```

### 1.1 Module Roles & Responsibilities

| Module Name | Source Files | Primary Role & Interfaces |
|:---|:---|:---|
| **`ThermalZoneManager`** | `ThermalZoneManager.h/.cpp` | Manages hydronic zones, state transitions, relay outputs, Bio-Schedule anchor evaluation, and manual boost holds. |
| **`ThermodynamicEngine`** | `ThermodynamicEngine.h/.cpp` | Computes 2R2C discrete model matrices, night cooling calibration ($k_{\text{loss}}$), Away step response ($t_{\text{dead}}$, slope, overshoot), predictive preheat lead time, and numerical 1st/2nd derivative open-window detection. |
| **`BoilerRadioTrigger`** | `BoilerRadioTrigger.h/.cpp` | Generates 409-transition OOK pulse trains on 433.92 MHz, enforces 3s RF cooldown, anti-short-cycling 60s/60s guards, and power-up failsafe OFF bursts. |
| **`BleSensorScanner`** | `BleSensorScanner.h/.cpp` | Passive NimBLE observer extracting temperature, humidity, and battery data from Xiaomi BTHome v2 packets without connection overhead. |
| **`DiagnosticEngine`** | `DiagnosticEngine.h/.cpp` | Tracks diagnostic trouble codes (`DTC-101` to `DTC-402`), health scores, self-test verification, and service mode execution. |
| **`TimeAndWeatherService`**| `TimeAndWeatherService.h/.cpp` | Synchronizes NTP time (Europe/Bucharest), polls Open-Meteo REST API, and determines seasonal heating boundaries. |
| **`DevicePassport`** | `DevicePassport.h/.cpp` | Stores digital device passport in NVS (serial number, commissioning date, installer notes). |
| **`TelemetryHistoryManager`**| `TelemetryHistoryManager.h/.cpp` | Manages LittleFS historical ring buffers (`events.bin`, `history24h.bin`) for high-speed local charting. |
| **`NetworkConsole`** | `NetworkConsole.h/.cpp` | 2-Way bidirectional diagnostic console on Telnet port 23 and `/console` HTTP endpoint. |

---

## 2. Memory Architecture & Partition Layout

Fulfilling **[SWE-NFR-001]**, **[SWE-NFR-002]**, and **[SYS-REQ-015]**:

```
+-----------------------------------------------------------------------------------------+
|                                4 MB SPI FLASH PARTITION MAP                              |
+-------------+-------------+-----------------------------+-------------------------------+
|  nvs (20KB) | otadata(8KB)|   app0 (OTA 0 - 1.25 MB)    |    app1 (OTA 1 - 1.25 MB)     |
|   0x9000    |   0xe000    |          0x10000            |           0x150000            |
| Config/Keys | OTA Boot Reg| Active Firmware (1,295,040 B| Staging Slot (1,310,720 B max)|
+-------------+-------------+-----------------------------+-------------------------------+
                            |                                                             |
                            +-------------------------------------------------------------+
                            |               littlefs Partition (1.375 MB)                 |
                            |                          0x290000                           |
                            | - web/index.html.gz (53 KB)                                 |
                            | - telemetry /events.bin & /history24h.bin                   |
                            | - device assets & icons                                     |
                            +-------------------------------------------------------------+
```

### 2.1 Memory Budgets & Real-Time Invariants
- **Flash OTA App Slot:** Maximum $1,310,720\text{ bytes}$ ($1.25\text{ MB}$). Current firmware build: **$1,295,040\text{ bytes}$** ($15,680\text{ bytes}$ free headroom).
- **RAM Heap Floor:** Minimum $80\text{ KB}$ Free Heap. Runtime average: **$89\text{–}91\text{ KB}$** with NimBLE and WebServer active.
- **Stack Protection:** Main `loop()` task runs on FreeRTOS Core 1 with 8192-byte stack; NimBLE background host runs on Core 0.

---

## 3. Concurrency & Task Topography

```
                    +------------------------------------+
                    |        ESP32 DUAL-CORE SOC         |
                    +-----------------+------------------+
                                      |
         +----------------------------+----------------------------+
         |                                                         |
         v Core 0 (System Core)                                    v Core 1 (App Core)
+------------------------------------+                    +------------------------------------+
| FreeRTOS Host Tasks                |                    | Arduino loop() & HTTP Engine       |
| - Wi-Fi Protocol Stack (lwIP)      |                    | - ThermalZoneManager::update()     |
| - NimBLE Bluetooth Controller      |                    | - ThermodynamicEngine::update()    |
| - Network Packet Dispatcher        |                    | - BoilerRadioTrigger::update()     |
+------------------------------------+                    | - WebServer::handleClient()        |
                                                          | - esp_task_wdt_reset()             |
                                                          +------------------------------------+
```

### 3.1 Watchdog & Determinism Guarantees
- The application core executes a strictly non-blocking cooperative event loop.
- **Hardware Watchdog:** ESP32 Task Watchdog Timer (TWDT) is initialized to $30\text{ s}$. `esp_task_wdt_reset()` is called at the head of every `loop()` iteration.
- Network socket operations (HTTP client queries, weather fetch) have a hard timeout clamp of $\le 1.5\text{ s}$ to guarantee `loop()` responsiveness.
