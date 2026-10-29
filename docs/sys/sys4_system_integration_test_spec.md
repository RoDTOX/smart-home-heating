# [SYS.4] System Integration & Verification Specification

> **Standard:** ASPICE 4.0 PAM — Process SYS.4 (System Integration and Integration Verification)  
> **Status:** RELEASED / BASELINED  
> **Traceability Root:** `[TC-SYS-xxx]`  
> **Verification Upward:** Validates `[SYS-REQ-001]` through `[SYS-REQ-020]`

---

## 1. Test Architecture & Hierarchy

The integration test framework is structured into a 4-Tier verification hierarchy designed to stress physical hardware, hydraulic interlocks, and concurrent communication channels.

```
       +-------------------------------------------------------+
       |   Tier 4: Multi-Day Real-World Chrono Scenarios       |
       +---------------------------+---------------------------+
                                   ^
       +---------------------------+---------------------------+
       |   Tier 3: Pairwise Cross-Feature Interactions (TC-X)  |
       +---------------------------+---------------------------+
                                   ^
       +---------------------------+---------------------------+
       |   Tier 2: Boundary, Limit & Safety Threshold Stress   |
       +---------------------------+---------------------------+
                                   ^
       +---------------------------+---------------------------+
       |   Tier 1: Atomic Functional Component Verification    |
       +-------------------------------------------------------+
```

---

## 2. Tier 1: Functional Integration Tests

| Test Case ID | Target Requirement | Integration Test Description | Expected Result |
|:---|:---|:---|:---|
| **[TC-SYS-001]** | `[SYS-REQ-001]` | Verify individual zone relay engagement on setpoint increase above ambient + hysteresis. | Relay pin goes HIGH; actuator energizes; zone state transitions to `ZONE_WARMUP`. |
| **[TC-SYS-002]** | `[SYS-REQ-002]` | Verify RF 433.92 MHz pulse train generation on boiler command. | Transmitter produces 65ms preamble + 409 transitions; boiler flame ignites. |
| **[TC-SYS-003]** | `[SYS-REQ-003]` | Verify $120\text{ s}$ actuator delay before RF boiler trigger. | Timer verifies flame command is emitted strictly after $t \ge 120\text{ s}$. |
| **[TC-SYS-004]** | `[SYS-REQ-004]` | Verify $180\text{ s}$ secondary pump overrun upon burner cutoff. | Pump remains energized for $180\text{ s} \pm 2\text{ s}$ post burner flame extinction. |
| **[TC-SYS-005]** | `[SYS-REQ-005]` | Verify burner anti-short-cycling guard (60s ON / 60s OFF). | Toggles within 60s return `HTTP 429` with `pendingAction` and countdown. |
| **[TC-SYS-006]** | `[SYS-REQ-006]` | Verify emergency freeze protection triggers at $T < 12.0^\circ\text{C}$. | Boiler and zone relay force ON regardless of schedule mode. |
| **[TC-SYS-007]** | `[SYS-REQ-007]` | Verify sensor timeout isolation after 600s loss of signal. | Actuator relay de-energizes; `DTC-101` raised in diagnostic register. |
| **[TC-SYS-008]** | `[SYS-REQ-008]` | Verify weekly 20s anti-seize exercise cycle. | Relay and pump cycle for 20s; NVS timestamp updated. |
| **[TC-SYS-012]** | `[SYS-REQ-012]` | Verify wall tablet zero-scroll rendering ($1280 \times 800$). | DOM rendered height $\le 530\text{px}$; no vertical overflow scrollbars. |
| **[TC-SYS-013]** | `[SYS-REQ-013]` | Verify REST JSON API response latency under local LAN. | 95% of API queries respond in $< 100\text{ ms}$. |

---

## 3. Tier 3: Pairwise Cross-Feature Interaction Matrix (`TC-X01` to `TC-X10`)

Cross-feature coupling represents the primary source of real-world edge defects. The pairwise matrix verifies safety invariants under conflicting concurrent events:

| Case ID | Feature A (State) | Feature B (Event) | Conflict & Expected Resolution | Verification Verdict |
|:---|:---|:---|:---|:---:|
| **[TC-X01]** | **Burner Active in Min-Burn (60s)** | **Open-Window Declared ($dT/dt \le -2.5\times$)** | Safety priority: Zone relay opens immediately (0V). Boiler burner cancels demand once min-burn 60s elapses to protect heat exchanger. | **PASS** |
| **[TC-X02]** | **Sensor Offline (`DTC-101`)** | **User Requests Manual Boost (+2.0°C)** | Rejected: Manual boost is prohibited while zone is in sensor failsafe isolation. API returns error status. | **PASS** |
| **[TC-X03]** | **Predictive Pre-Heat Active** | **User Engages Away Mode (`/api/schedule/away`)** | Immediate preemption: Pre-heat is cancelled, zone drops to Eco target ($18.0^\circ\text{C}$), boiler turns OFF. | **PASS** |
| **[TC-X04]** | **Active Heating Demand** | **Abrupt Electrical Power Loss & Recovery** | Fail-safe boot: ESP32 boots up, sends RF flame OFF burst, restores setpoints from NVS, syncs NTP clock. | **PASS** |
| **[TC-X05]** | **All 3 Zones Idle (Temp OK)** | **Simultaneous Setpoint Step Change on all 3 Zones** | Synchronized interlock: All 3 relays engage; single $120\text{ s}$ delay is shared; single RF flame ON burst. | **PASS** |
| **[TC-X06]** | **RF Cooldown Guard Active (3s)** | **Rapid Consecutive HTTP Flame Commands** | Spam protection: Rapid redundant commands return `HTTP 429`; zero unnecessary RF bursts emitted. | **PASS** |
| **[TC-X07]** | **Open-Meteo Weather Polling** | **Internet WAN Outage / Wi-Fi Drop** | Graceful degradation: Last valid weather reading cached for 30 min; fallback to seasonal Romanian defaults. | **PASS** |
| **[TC-X08]** | **Heating Active (NC Actuator)** | **Actuator Polarity Inverted in NVS (NO Mode)** | Safety interlock: Relay logic flips safely without causing permanent actuator valve closure. | **PASS** |
| **[TC-X09]** | **Away Mode Active ($18.0^\circ\text{C}$)** | **Severe Ambient Temperature Plunge ($< 12.0^\circ\text{C}$)** | Override invariant: Freeze protection supersedes Away mode; boiler turns ON until room reaches $14.0^\circ\text{C}$. | **PASS** |
| **[TC-X10]** | **Anti-Seize Exercise Triggered** | **Active Heating Demand in Progress** | Arbitration: Anti-seize exercise is skipped because valves/pump are already actively operating. | **PASS** |

---

## 4. Tier 4: Multi-Day Real-World Chrono Scenarios

1. **Scenario CH-01 (Circadian Day/Night Transition):**
   - 24-hour cycle transitioning through Sleep ($20.0^\circ\text{C}$), Wake ($22.5^\circ\text{C}$), Away ($18.0^\circ\text{C}$), and Home ($22.0^\circ\text{C}$).
   - Verifies zero setpoint jitter, accurate preheat lead times, and seamless anchor handoffs.
2. **Scenario CH-02 (Weekend Shift & Prolonged Away):**
   - Automatic activation of weekend block masks on Saturday 00:00 without requiring user intervention.
3. **Scenario CH-03 (Prolonged Sensor Battery Depletion):**
   - Sensor battery dies at 02:00. Zone enters `DTC-101` at 02:10; boiler continues servicing remaining 2 zones safely without freezing or overheating the faulted room.
