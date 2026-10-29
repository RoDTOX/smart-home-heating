# [SYS.3] Hydronic Architecture & Physical Failsafe Specification

> **Standard:** ASPICE 4.0 PAM — Process SYS.3 (System Architectural Design / Hydronic Physics Element)  
> **Status:** RELEASED / BASELINED  
> **Traceability:** Fulfills `[SYS-REQ-001]`, `[SYS-REQ-003]`, `[SYS-REQ-004]`, `[SYS-REQ-006]`, `[SYS-REQ-008]`  
> **Condition Adherence:** Complies with Independent Quality Audit Condition **[🔴CRT-02]**

---

## 1. Physical Manifold Topology & Loop Mapping

The residential installation features an underfloor heating distributor/collector manifold with **7 hydraulic loops** controlled across **3 logical thermal zones**:

```
+-----------------------------------------------------------------------------------+
|                        7-LOOP DISTRIBUTOR / COLLECTOR MANIFOLD                     |
+---------+---------+---------+---------+---------+---------+---------+                 |
| Loop 1  | Loop 2  | Loop 3  | Loop 4  | Loop 5  | Loop 6  | Loop 7  |                 |
| Living  | Living  | Kitchen | M. Bed  | M. Bed  | Kids    | Kids    |                 |
| (Area A)| (Area B)| (Area C)| (Loop 1)| (Loop 2)| (Loop 1)| (Loop 2)|                 |
+---------+---------+---------+---------+---------+---------+---------+                 |
     \         |         /         \         /         \         /                      |
      +--------+--------+           +---+---+           +---+---+                       |
               |                        |                   |                           |
        ZONE 1 (RELAY 1)         ZONE 3 (RELAY 3)    ZONE 2 (RELAY 2)                   |
          GPIO32 (NC)              GPIO25 (NC)         GPIO33 (NC)                      |
+-----------------------------------------------------------------------------------+
                                                                    |
                                 NATURAL HYDRAULIC BYPASS ----------> [ 2x Bathroom Towel Radiators ]
                                 (Permanent Unrestricted Loop)        (Open Mechanical Valves)
```

### 1.1 Detailed Circuit Assignment

| Logical Zone | Physical Room | Actuator Relay | Manifold Loops | Screed Type & Area | Assigned Sensor MAC |
|:---|:---|:---:|:---:|:---|:---:|
| **Zone 1** | Living Room & Kitchen | Relay 1 (`GPIO32`) | Loops 1, 2, 3 in parallel | Ceramic tiles on $5.0\text{ cm}$ concrete screed (~$38\text{ m}^2$) | `A4:C1:38:A4:C1:DD` |
| **Zone 2** | Children's Bedroom | Relay 2 (`GPIO33`) | Loops 6, 7 in parallel | Floating parquet on $5.0\text{ cm}$ screed (~$16\text{ m}^2$) | `A4:C1:38:84:A1:E8` |
| **Zone 3** | Master Bedroom | Relay 3 (`GPIO25`) | Loops 4, 5 in parallel | Floating parquet on $5.0\text{ cm}$ screed (~$18\text{ m}^2$) | `A4:C1:38:94:0F:44` |
| **Bypass** | Bathrooms (2x) | *None (Manual)* | Direct radiator loops | Wall towel radiators (no underfloor circuits) | Natural bypass path |

---

## 2. Natural Hydraulic Bypass Invariant

A primary hazard in hydronic heating is boiler circulation pump deadheading (cavitation, motor overheating, pressure relief valve discharge) when all thermostatic valves close while the pump is active.

### Engineering Protection Mechanism
- The two apartment bathrooms are equipped with wall-mounted towel radiators connected directly to the primary heating circuit.
- These radiators **do not possess electrothermal actuators** and their mechanical balancing valves are set **permanently 100% open**.
- **Hydraulic Function:** They serve as a natural, passive differential pressure bypass loop. If all 3 underfloor zone relays de-energize (0V) and close their wax actuators, the boiler primary pump retains an open, low-resistance fluid circulation path through both bathrooms, ensuring safe heat dissipation.

---

## 3. Hydronic Failsafe Timing Envelopes

Electrothermal wax actuators operate via phase change of an internal paraffin wax charge heated by a PTC resistor. Their stroke is slow and non-linear. The system enforces strict software timing invariants to guarantee hydraulic integrity:

```
[Demand ON]
     |
     v
[Relay ON (230V)] ======= Actuator Interlock (120s - 180s) =======> [Boiler Flame ON (433MHz)]
                               (Paraffin Wax Dilates)

[Demand OFF]
     |
     v
[Boiler Flame OFF] ====== Pump Overrun (180s) ====== Actuator Cooldown ======> [Relay OFF]
                         (Heat Exchanger Flushed)    (Spring Closes Valve)
```

### 3.1 Actuator Interlock Delay ($t_{\text{interlock}} = 120\text{s} - 180\text{s}$)
- **Physics:** When 230V AC is applied to a cold NC actuator, the wax element requires $90\text{ s}$ to begin expansion and $180\text{ s}$ to reach full $4.0\text{ mm}$ stroke.
- **Safety Invariant:** The controller **never** commands boiler flame ignition simultaneously with relay closure. The software enters state `ZONE_WARMUP` and holds the boiler command in pause for $120\text{ s}$ ($180\text{ s}$ on cold boot), ensuring water circulation begins before burner firing.

### 3.2 Circulation Pump Overrun ($t_{\text{overrun}} = 180\text{s}$)
- **Physics:** Modern gas condensing heat exchangers hold high thermal mass in their aluminum-silicon or stainless steel heat cores. Sudden flow cessation at burner cut-off causes localized boiling, lime precipitation, and thermal tripping.
- **Safety Invariant:** Following burner shutdown, the secondary circulation pump (Relay 4 / `GPIO26`) or keeper zone valve remains energized for $180\text{ s}$, extracting residual thermal energy into the floor slab and bathroom radiators.

### 3.3 Burner Anti-Short-Cycling Guard ($t_{\text{min\_burn}} = 60\text{s}$, $t_{\text{min\_pause}} = 60\text{s}$)
- **Physics:** Frequent burner ignition cycles degrade the gas valve solenoid, spark igniter, and flue gas blower, while causing inefficient combustion.
- **Safety Invariant:** Once ignited, the burner must burn for at least $60\text{ s}$ before being allowed to shut down (unless emergency freeze protection or manual forced override intervenes). Once extinguished, the burner must rest for at least $60\text{ s}$ before reigniting. Rapid requests return `HTTP 429 Too Many Requests` with a live cooldown counter.

### 3.4 Emergency Freeze Protection Guard ($T_{\text{freeze}} < 12.0^\circ\text{C}$)
- **Safety Invariant:** If any calibrated zone reports a temperature below $+12.0^\circ\text{C}$, the controller forces that zone's relay ON and triggers the boiler burner, overriding manual OFF states, Eco mode, and Away mode until the room reaches $+14.0^\circ\text{C}$.

### 3.5 Sensor Loss Failsafe Isolation (`DTC-101`, $t_{\text{timeout}} = 600\text{s}$)
- **Safety Invariant:** If a Xiaomi BLE sensor drops offline for $> 10\text{ minutes}$ ($600\text{ s}$), the associated zone is marked faulted (`sensorFault = true`), its actuator relay is immediately de-energized to prevent uncontrolled heating, and diagnostic trouble code `DTC-101` is logged. Control resumes automatically upon packet reception.

### 3.6 Weekly Anti-Seize Exercise Cycle ($t_{\text{exercise}} = 20\text{s}$)
- **Physics:** In non-heating months, dissolved minerals in heating water precipitate onto the valve spindle and pump rotor, causing mechanical seizure upon autumn restart.
- **Safety Invariant:** Every 7 days ($604,800\text{ s}$) recorded via NVS epoch, the controller cycles each actuator relay and the circulation pump for $20\text{ s}$.

---

## 4. Outdoor Weather Sensing: Hybrid Radio / Cloud Fallback

To support the 2R2C dynamic thermal model and weather compensation curves ($36.0^\circ\text{C} - 43.5^\circ\text{C}$ supply water):
1. **Primary Physical Radio Channel:** External weather station **Tronic 4-LD7396-2** (IAN `515492_2510`) transmitting outside ambient temperature and humidity on 433.92 MHz.
2. **Autonomous Cloud REST Fallback:** Open-Meteo REST API queried every 15 minutes over Wi-Fi (`http://api.open-meteo.com/v1/forecast`), providing localized GPS outdoor conditions (`lat: 45.6579, lon: 25.6012`). In case of network drop, the last valid reading is cached for up to 30 minutes before gracefully switching to seasonal defaults.
