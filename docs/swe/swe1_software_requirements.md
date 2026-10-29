# [SWE.1] Software Requirements Specification

> **Standard:** ASPICE 4.0 PAM — Process SWE.1 (Software Requirements Analysis)  
> **Status:** RELEASED / BASELINED  
> **Traceability Root:** `[SWE-REQ-xxx]`, `[SWE-NFR-xxx]`  
> **Traceability Upward:** Derived from `[SYS-REQ-001]` through `[SYS-REQ-020]`

---

## 1. Functional Requirements Register (FR-01 to FR-45)

### 1.1 Zone & Relay Actuation Layer
- **[SWE-REQ-001] (Zone Count & State):** The software shall maintain state for at least 3 discrete thermal zones (`ZONE_IDLE`, `ZONE_WARMUP`, `ZONE_HEATING`, `ZONE_WINDOW_PAUSE`, `ZONE_FAULT`). *(Derived from [SYS-REQ-001])*
- **[SWE-REQ-002] (Polarity Configuration):** Actuator active drive level shall be software configurable between Normally Closed (NC, active HIGH) and Normally Open (NO, active LOW) persisted in NVS. *(Derived from [SYS-REQ-001])*
- **[SWE-REQ-003] (Hysteresis Evaluation):** A zone shall request heat when $T_{\text{ambient}} < T_{\text{target}} - \text{Hysteresis}$ (default $0.30^\circ\text{C}$), and extinguish demand when $T_{\text{ambient}} \ge T_{\text{target}} + \text{Hysteresis}$. *(Derived from [SYS-REQ-001])*
- **[SWE-REQ-004] (Opening Interlock Timer):** Following demand assertion, the software shall start an interlock timer ($120\text{ s} - 180\text{ s}$), inhibiting boiler flame commands until the timer expires. *(Derived from [SYS-REQ-003])*
- **[SWE-REQ-005] (Pump Overrun Timer):** Upon extinguishing burner demand, the software shall run the secondary circulation pump for $180\text{ s} \pm 2\text{ s}$ before de-energizing Relay 4. *(Derived from [SYS-REQ-004])*

### 1.2 Boiler Radio Trigger Layer
- **[SWE-REQ-006] (RF OOK Modulation):** The software shall generate an accurate 409-transition OOK bit-banged pulse train with a $65\text{ ms}$ preamble on the configured RF GPIO. *(Derived from [SYS-REQ-002])*
- **[SWE-REQ-007] (Idempotency Filter):** Re-asserting an active state (ON when already ON, OFF when already OFF) shall produce zero RF bursts and return immediate HTTP 200 with `redundant: true`. *(Derived from [SYS-REQ-002])*
- **[SWE-REQ-008] (Transmitter Cooldown):** Consecutive RF transmissions shall enforce a minimum spacing of $3.0\text{ s}$ to prevent radio spectrum saturation. *(Derived from [SYS-REQ-002])*
- **[SWE-REQ-009] (Anti-Short-Cycling Burner Guard):** The software shall enforce $60\text{ s}$ minimum burner run time and $60\text{ s}$ minimum burner rest time. Conflicting commands return HTTP 429 with `pendingAction` and `waitSec`. *(Derived from [SYS-REQ-005])*
- **[SWE-REQ-010] (Power-Up Failsafe Burst):** At initial boot, `BoilerRadioTrigger::begin()` shall immediately issue an RF flame OFF burst and save state `OFF` to NVS. *(Derived from [SYS-REQ-017])*

### 1.3 Sensor Ingestion & Timeout Failsafe Layer
- **[SWE-REQ-011] (NimBLE Passive Observation):** The BLE scanner shall ingest BTHome v2 unencrypted temperature, humidity, and battery broadcasts without establishing Bluetooth connections. *(Derived from [SYS-REQ-001])*
- **[SWE-REQ-012] (Boot Warmup Grace Period):** For the first $300\text{ s}$ after boot, missing sensor telemetry shall maintain state `ZONE_WARMUP` rather than immediately faulting. Transition to active control occurs immediately upon receiving packets for all assigned sensors. *(Derived from [SYS-REQ-007])*
- **[SWE-REQ-013] (Sensor Timeout Failsafe DTC-101):** If no valid BLE packet is received for $> 600\text{ s}$, the software shall set `sensorFault = true`, de-energize the actuator relay, cancel boiler demand, and log `DTC-101`. Control auto-recovers on next valid packet. *(Derived from [SYS-REQ-007])*

### 1.4 Thermodynamic Predictive Engine (2R2C Model)
- **[SWE-REQ-014] (Mathematical Formulation):** The thermodynamic engine shall model indoor temperature $T_a$ and screed core temperature $T_s$ using discrete state-space formulation:
  $$\frac{dT_s}{dt} = \frac{Q_{\text{in}}}{C_s} - \frac{T_s - T_a}{R_s C_s}, \quad \frac{dT_a}{dt} = \frac{T_s - T_a}{R_s C_a} - \frac{T_a - T_{\text{ext}}}{R_e C_a}$$
  *(Derived from [SYS-REQ-009])*
- **[SWE-REQ-015] (Passive Night-Time Calibration):** Between 01:00 and 05:00, when all zones are in idle cooling, the engine shall compute the building loss rate:
  $$k_{\text{loss}} = \frac{T(t_1) - T(t_2)}{(t_2 - t_1) \cdot (\bar{T}_{\text{indoor}} - \bar{T}_{\text{ext}})}$$
  and update the running exponential moving average (EMA $\alpha = 0.2$). *(Derived from [SYS-REQ-009])*
- **[SWE-REQ-016] (Step-Response Calibration):** During Away mode step tests, the engine shall identify dead time ($t_{\text{dead}}$), rising slope ($dT/dt$), and thermal overshoot ($\Delta T_{\text{overshoot}}$). *(Derived from [SYS-REQ-009])*
- **[SWE-REQ-017] (Predictive Pre-Heat Calculation):** The software shall calculate dynamic lead time $t_{\text{lead}}$ so that room arrives at target setpoint within $\pm 0.3^\circ\text{C}$ of schedule anchor:
  $$t_{\text{lead}} = t_{\text{dead}} + \frac{T_{\text{target}} - T_{\text{current}}}{\text{Slope}} \times 60 - \Delta T_{\text{overshoot}} \times K_p$$
  *(Derived from [SYS-REQ-010])*

### 1.5 Rapid Derivative Open-Window Detection Engine
- **[SWE-REQ-018] (Circular Buffer):** The software shall maintain a rolling 12-sample circular buffer ($60\text{ min}$ history at $5\text{ min}$ intervals) per zone. *(Derived from [SYS-REQ-011])*
- **[SWE-REQ-019] (Numerical Derivative Filters):** The software shall compute central-difference first derivative ($dT/dt$) and acceleration second derivative ($d^2T/dt^2$):
  $$\frac{dT}{dt} = \frac{T_k - T_{k-2}}{2 \cdot \Delta t}, \quad \frac{d^2T}{dt^2} = \frac{T_k - 2T_{k-1} + T_{k-2}}{\Delta t^2}$$
  *(Derived from [SYS-REQ-011])*
- **[SWE-REQ-020] (Trigger Threshold):** An open window shall be declared if:
  $$\left|\frac{dT}{dt}\right| > 2.5 \times \text{Rate}_{\text{natural}}(T_{\text{ext}})$$
  yielding detection in $\le 10\text{ min}$ at $T_{\text{ext}} = 15^\circ\text{C}$ and $\le 5\text{ min}$ at $T_{\text{ext}} = -5^\circ\text{C}$. *(Derived from [SYS-REQ-011])*
- **[SWE-REQ-021] (Auto-Recovery & Safety Timeout):** Heating shall pause, de-energizing the relay. The pause automatically clears when $dT/dt \ge 0$ for two consecutive samples, upon manual UI tap, or after a maximum safety timeout of $30\text{ min}$. *(Derived from [SYS-REQ-011])*

---

## 2. Diagnostic Trouble Code (DTC) Register

Fulfilling **[SYS-REQ-019]**, the software embeds an automotive-grade diagnostic dictionary:

| DTC Code | Severity | Trigger Condition | Automated Mitigation Action |
|:---:|:---:|:---|:---|
| **`DTC-101`** | **CRITICAL** | Assigned BLE room sensor offline $> 600\text{ s}$ | Zone relay de-energized; boiler demand isolated |
| **`DTC-102`** | **WARNING** | BLE sensor battery level $< 15\%$ | Telemetry flagged; low-battery push alert |
| **`DTC-201`** | **WARNING** | Command rate limit exceeded (spam threshold) | Command rejected with HTTP 429 |
| **`DTC-202`** | **CRITICAL** | Boiler RF transmission failure / WDT violation | Transmitter reset; safe state restored |
| **`DTC-301`** | **WARNING** | Rapid temperature drop detected (Window Open) | Zone heating paused; actuator closed |
| **`DTC-302`** | **CRITICAL** | Emergency frost threshold reached ($< 12.0^\circ\text{C}$) | Forced zone and boiler activation |
| **`DTC-401`** | **WARNING** | Wi-Fi connection lost $> 300\text{ s}$ | Non-blocking reconnect; local control active |
| **`DTC-402`** | **WARNING** | Open-Meteo weather API timeout $> 1800\text{ s}$ | Fallback to Romanian seasonal defaults |

---

## 3. Non-Functional Requirements (NFR)

- **[SWE-NFR-001] (OTA Binary Budget):** Total compiled binary shall be $\le 1,310,720\text{ bytes}$ ($1.25\text{ MB}$). *(Derived from [SYS-REQ-015])*
- **[SWE-NFR-002] (RAM Stability):** Free Heap RAM shall remain $\ge 80\text{ KB}$ under full BLE scan load and active WebServer connections. *(Derived from [SYS-REQ-015])*
- **[SWE-NFR-003] (REST Response Time):** 95th percentile HTTP response latency shall be $< 100\text{ ms}$. *(Derived from [SYS-REQ-013])*
- **[SWE-NFR-004] (NVS Wear Protection):** NVS parameter commits shall execute strictly when value delta exceeds $\pm 2\%$. *(Derived from [SYS-REQ-014])*
- **[SWE-NFR-005] (Static Memory Invariant):** Zero dynamic heap allocation (`malloc`, `new`, unbounded `String`) in periodic `loop()` execution paths. *(Derived from [SYS-REQ-015])*
- **[SWE-NFR-006] (Watchdog Protection):** `loop()` iteration latency shall never exceed $500\text{ ms}$ under any condition, feeding ESP32 TWDT (30s timeout). *(Derived from [SYS-REQ-018])*
