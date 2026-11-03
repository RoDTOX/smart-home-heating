# System and Software Requirements — Smart Home Heating

**Baseline:** v2.10, 02.11.2026. v2 (31.10.2026) derived from the owner-approved review
[`requirements_review.md`](requirements_review.md); v2.1 adds the process review rows QA-7, QA-8, QA-9, QA-10,
QA-11, QA-12, QA-13, QA-14, QA-16 ([`process_review.md`](process_review.md)); v2.2 adds the owner decisions D37,
D38, D39 and the architecture observations O-12, O-13, O-14, O-15, and closes the review (applied as listed in
`requirements_review.md` §6 and §8); v2.3 adds O-16, v2.4 the 10 s bound of NFR-1.8, v2.5 the proxy clause of FR-8.1, v2.6 the owner's page test
(C-3…C-8) v2.7 the configuration export / import FR-65.1, FR-65.2, FR-65.3, FR-65.4, FR-65.5, FR-65.6, FR-65.7, FR-65.8 (C-9) v2.8 zones added and deleted by the user and the sensor search (C-10, C-11) v2.9 one Save for all zones (C-12) and v2.10 the zone target out of the settings file (C-13), v2.11 pre-import diff preview and vertical zone controls (FR-8.1, PB-70), changes after closure
(`requirements_review.md` §9).
**Replaces:** the Romanian specification of 07.10.2026 (v1). IDs are kept where the meaning did not change.
**Related:** [`plan.md`](plan.md), [`changelog.md`](changelog.md), [`hardware_esp32_relay.md`](hardware_esp32_relay.md),
[`underfloor_heating_hydraulics_and_failsafes.md`](underfloor_heating_hydraulics_and_failsafes.md), [`api_reference.md`](api_reference.md).

---

## 0. How to read this document

- `- **FR-x (Title):**` defines a requirement. Struck-through items (`~~`) are retired and keep their ID reserved.
- **Phase tag** after the title of a functional requirement says *when* it is implemented, not *whether* it is
  required; the table below maps each tag to the migration steps of [`architecture.md`](architecture.md) §18.
  A sub-item without a tag has its parent's tag. Non-functional requirements have no tag: each applies from the
  step that builds what it constrains.
- **Attribute** `{LEVEL; METHOD}` follows the phase tag, or the title where there is no tag. Retired items have none.
  - LEVEL — who meets the requirement: `SW` the firmware, `HW` hardware (controller board, boiler receiver,
    sensors, actuators, flow probes), `install` the installer at commissioning (FR-68).
  - METHOD — how it is verified: `test` automated on the board by `tools/sh_test_system.py`; `board` measured or
    observed on the board by the owner, over hours or days where the requirement needs it; `inspection` review of
    code, documents or the installation; `analysis` calculation, build output or recorded data evaluated off the
    board.
  - A comma list (`{SW, HW; test, inspection}`) means each level meets its part and each method verifies its part.
- Every functional requirement states its **inputs**, **fallback** and **reports** (NFR-5.2); fallbacks follow the
  priorities of NFR-5.1.
- Configuration parameters are written as `name`; default, range and storage are defined once, in §6.
  Requirements cite the name, never the number (process rule R2).
- DTC codes are defined once, in §5.
- IDs are cited in full (`FR-6.1, FR-6.2`), never as a range, so the checker can trace them. A parent ID (`FR-6`)
  groups its children and is not a requirement of its own.

**Phase tags and migration steps** ([`architecture.md`](architecture.md) §18):

| Tag | Built in |
|---|---|
| `[now]` | steps 1–13, in the step that builds the module the requirement is allocated to (architecture §3.2) |
| `[phase 4]` | step 14, one advisor or monitor per step |
| `[later]` | step 14, after the advisors (factory reset) |
| `[nice to have]` | optional, only on the owner's request; step 14 lists humidity compensation (FR-71) as the last advisor and extra BLE receivers (FR-72) with the `[later]` items |
| `[now; sensors optional]` | step 13, after the owner has chosen the probe type (FR-54) |
| `[now; receiver hardware later]`, `[now: configuration; hardware later]` | the firmware part in steps 1–13; the named hardware has no step until it is chosen (FR-55, FR-69) |
| `[now, documentation]` | the commissioning document, no firmware step; the firmware functions it uses have their own requirements (FR-68) |

**Glossary.** These terms are used with exactly this meaning; a value given here is part of every requirement that
uses the term.

| Term | Meaning |
|---|---|
| control tick | One pass of input, control and safety evaluation, every 1 s (architecture §5.3). "At the next control tick" means in the first tick after the trigger; the time until the output has changed is bounded by NFR-1.8. |
| TPI cycle | The 60 min period of time-proportional control (FR-47); a zone's valve is ON for duty × TPI cycle in each cycle. "Cycle" alone always means a TPI cycle, never a control tick. |
| request | What a function or a user asks for: open valve n, boiler ON. API and console calls that change an output are requests. |
| command | What is actually sent to an output: a relay state, a boiler link frame or contact state. Only the safety layer turns requests into commands (architecture §0). A *console command* (FR-7.3) is a text command, not an output command. |
| demand | A zone has demand while a request to open its valve is active: the ON time of its TPI cycle (FR-47) or of its sensor-loss duty (FR-51), frost protection (FR-16.1), a MANUAL request or relay test (FR-1.3, FR-43), valve exercise (FR-19) or a self-test step (FR-60). Demand from valve exercise never asks for boiler ON (FR-19). |
| accepted reading | A room sensor reading that passed the plausibility rules of FR-53; control uses it with the zone offset of FR-49. Control, frost protection, window detection and the sensor timeout use only accepted readings (FR-16.1, FR-16.2, FR-40, FR-53). |
| flame lit | Tier A (boiler switched ON/OFF without feedback, the reference install, §2): an ON frame (or contact close) was sent since the last OFF. A start cancelled before the ON went out — during the interlock or in `PENDING_ON` (FR-32) — never lit the flame. Tier B (a boiler link that reports the burner state, not specified yet) may use that feedback instead. |

---

## 1. Purpose and scope

Autonomous multi-zone underfloor heating: **install once, calibrate once, forget it.** Every occupied room stays
at its target (NFR-4.1) without manual tuning after commissioning (FR-68).

- **Controller:** ESP32-WROOM-32E on ESP32-Relay X4 V1.1 (4 relay outputs; more outputs need an expansion, FR-69).
- **Inputs:** BLE room sensors (Xiaomi LYWSD03MMC with PVVX / BTHome firmware), Open-Meteo weather, optional
  flow / return temperature sensors (FR-54).
- **Outputs:** 230 V thermal actuators (NC or NO), optional circulation pump, boiler ON/OFF via the boiler link.
- **Interface:** local REST/JSON API, console (Serial, Telnet, HTTP), one web page.
- **All control runs on the controller** (NFR-1.1).

**Out of scope:** cloud services, remote fleet management, Home Assistant integration, kiosk / wall-tablet UI,
floor cooling (no dew-point logic), push notifications (alarms are page-only, FR-62).

---

## 2. Reference installation (example configuration)

Zones, circuits, relays, sensors and boiler interface are configuration (FR-11, FR-69). The reference install is
one example:

| Item | Reference value |
|---|---|
| Boiler | Immergas Victrix Tera 24 (condensing), ON/OFF only (tier A) |
| Boiler link | 433.92 MHz OOK transmitter on GPIO19 → receiver dry contact (to be replaced by a fail-safe receiver, FR-55) |
| Zone 1 — living + kitchen | circuits 1, 2, 3 · relay 1 (GPIO32) · NC actuators |
| Zone 2 — kids bedroom | circuits 6, 7 · relay 2 (GPIO33) · NC actuators |
| Zone 3 — master bedroom | circuits 4, 5 · relay 3 (GPIO25) · NC actuators |
| Relay 4 (GPIO26) | unused (no pump installed) |
| Bypass | bathroom towel radiators, always open |
| Mixing valve | none — boiler water goes straight into the screed |

**Installation preconditions** (checked at commissioning, FR-68):

- An always-open bypass (radiator or differential-pressure valve) exists, so the boiler pump never runs against
  closed valves.
- Without a mixing valve, the boiler central-heating flow setpoint is set on the boiler to at most 45 °C.

---

## 3. Functional requirements

### 3.1 Room control

- **FR-47 (Time-proportional valve control):** *[now]* {SW; test, board} Each zone is controlled by time-proportional control (TPI):
  - TPI cycle (§0) 60 min; ON time per TPI cycle = duty × TPI cycle;
  - when (target − room temperature) >= 0.4 °C, duty = 100 % (underfloor heating deficit: continuous heating without TPI pause to overcome thermal inertia, C-14, PB-67);
  - when (target − room temperature) < 0.4 °C, duty = (target − room temperature) / 1.5 °C + outdoor compensation + learned base duty, limited to 0–100 %;
  - outdoor compensation = max(0, min(0.30, (15 °C − outdoor temperature) * 0.015)) when outdoor temperature is valid (FR-9);
  - learned base duty 0–60 %, adapted per zone and per outdoor band of 5 °C (7 bands), stored in NVS when it
    changes by >= 5 % (NFR-2.9);
  - minimum valve ON time and OFF time 10 min each (wax actuators need 3–5 min to open);
  - a new TPI cycle starts when the target changes by more than 0.4 °C;
  - the valve closes when the room exceeds target + `hysteresis`.
  - *Inputs:* room temperature (accepted readings, §0, FR-53), target, outdoor temperature (FR-9). *Fallback:* sensor lost -> FR-51; learned duty
    out of bounds -> default (NFR-5.3). *Reports:* duty per zone in the admin view (FR-8.1).
  - Acceptance examples: 0.5 °C below target -> 100 % (deficit rule); 0.2 °C below target with 15 °C outdoor -> 13 % with zero base duty, 25 % with
    a learned base duty of 12 %; 0.2 °C below target with 5 °C outdoor -> 28 % with zero base duty (15 % weather compensation).
- ~~**FR-1.1 (Hysteresis ON/OFF loop):** retired, replaced by FR-47; hysteresis remains only as overshoot cut.~~
- **FR-1.2 (Boost):** *[now]* {SW; test} A boost raises one zone's target above its current schedule level by 0.5–2.0 °C in
  0.5 °C steps; it never lowers the target (to cool use away, FR-44). Firmware enforces the limits for every
  source (page, API, console). The boost ends as defined in FR-39.
  - *Fallback:* request outside the limits → rejected, current target kept.
- **FR-39 (Boost end):** *[now]* {SW; test} A boost ends at the first of: `boostDuration` elapsed, next schedule slot change,
  `POST /api/zones/hold/clear`. The schedule level is never modified by a boost.
  - *Fallback:* no valid time → the boost ends after `boostDuration` measured on uptime.
- **FR-1.3 (Operating modes):** *[now]* {SW; test} AUTO: zones and boiler follow FR-47. MANUAL: relays and boiler follow the
  user's requests (maintenance, tests). In MANUAL, boiler ON requires at least one valve open for ≥ `interlockDelay`
  (FR-2), unless `openLoopBypass` is set. Frost protection (FR-16.1) and burner guards (FR-4) apply in both modes.
- **FR-52 (Manual mode timeout):** *[now]* {SW; test} MANUAL returns to AUTO after `manualTimeout`. While MANUAL is active,
  the page shows a warning with the remaining time.
- **FR-48 (Weather-shifted pre-heat):** *[now]* {SW; test} If the next schedule slot has a higher level, it starts early by
  120 + 9 × (5 − outdoor °C) min, limited to 45–240 min (2 h at +5 °C, 3.5 h at −5 °C).
  - *Inputs:* outdoor temperature (FR-9), schedule. *Fallback:* outdoor value older than 6 h or missing → 90 min.
    Replaced by the learned model when FR-67 is available. *Reports:* lead time in the admin view.
- **FR-51 (Zone without sensor):** *[now]* {SW; test} A zone whose sensor is in timeout (FR-16.2) keeps heating with its learned
  duty for the current outdoor temperature (bounded by NFR-5.3); if nothing is learned, with `faultDuty`.
  Frost protection (FR-16.1), interlock (FR-2) and burner guards (FR-4) stay active. Normal control resumes on the
  first accepted reading.
  - In global OFF (FR-64) and for a disabled zone, the zone runs this duty only while the outdoor temperature is
    below `faultFrostOutdoor` or unknown (a lost sensor must not remove frost safety; safety > economy).
  - *Reports:* DTC-101 `ERR_SENSOR_TIMEOUT`, zone state FAULT in the page.

### 3.2 Hydraulic safety

- **FR-2 (Actuator opening interlock):** *[now]* {SW; test} Boiler ON is commanded only after at least one zone
  valve has been energised (opened) for ≥ `interlockDelay`. Values below the range in §6 are rejected.
  - *Fallback:* this is the safe state; if the demand ends during the interlock, FR-37 applies.
- **FR-3 (Overrun after flame OFF):** *[now]* {SW; test} The last zone valve that was open stays open while the
  boiler is ON or `PENDING_OFF` (FR-32) and, if the flame was lit (§0), for `overrun` after the boiler OFF,
  independent of the pump setting. The boiler never burns with all zone valves closed.
  - **FR-3.2 (No overrun without flame):** {SW; test} If the flame was never lit (§0: the start was cancelled
    before the ON was sent), there is no overrun: valves without demand close at the next control tick (FR-37).
- ~~**FR-38 (Flame-conditioned pump overrun):** retired, merged into FR-3 / FR-3.2.~~
- **FR-4 (Burner anti-short-cycling):** *[now]* {SW; test} The burner stays ON for at least `burnerMinRun` and OFF
  for at least `burnerMinPause`, for every ON/OFF source (automatic, API, console); only a forced OFF (FR-17.3,
  FR-54) or a violated safety rule found by the self-check may end a run earlier than `burnerMinRun` (safety >
  burner wear, NFR-5.1). Values below the range in §6 are rejected.
  - *Fallback:* a request inside the guard is deferred (FR-32), never dropped silently.
- **FR-37 (Valve follows demand):** *[now]* {SW; test} A zone without demand (§0) has its valve de-energised
  (closed, per FR-15.1) at the next control tick, except the overrun valve of FR-3. If demand ends during the
  interlock, the valve closes and no boiler ON is sent for that zone.
- **FR-14.1 (Optional circulation pump):** *[now]* {SW; test} One relay may be configured as circulation pump; disabled by
  default.
- **FR-14.2 (Pump interlock):** *[now]* {SW; test} The pump starts only after a valve has been open for ≥ `interlockDelay`.
- **FR-14.3 (Pump overrun):** *[now]* {SW; test} The pump runs during the overrun of FR-3.
- **FR-14.5 (No pump against closed valves):** *[now]* {SW; test} The pump runs only while at least one valve is open, or during
  the overrun with the overrun valve open.
- ~~**FR-14.4 (Weekly anti-seize 20 s):** retired, replaced by FR-19.~~

### 3.3 Boiler link

- **FR-5.1 (Radio frame):** *[now]* {SW; inspection} The 433.92 MHz OOK frame reproduces the original thermostat: 409 level
  transitions, 65 ms silence before and after the data train.
- **FR-5.2 (Radio range):** *[now]* {HW, install; inspection} The boiler receiver switches the burner ON and OFF on command from the
  controller's installed position — verified at commissioning (FR-68).
- **FR-55 (Fail-safe boiler link):** *[now; receiver hardware later]* {SW, HW; test, inspection} Loss of the controller shall switch the boiler
  OFF:
  - wireless link: the controller sends its state every `hbInterval`; the receiver opens the boiler contact if no
    valid frame arrives within 3 × `hbInterval`;
  - wired link: the boiler contact is a dry normally-open relay contact on the controller (no power = OFF).
  - The heartbeat can be disabled (`boilerLinkHeartbeat` = off) for receivers without timeout.
  - *Reports:* while disabled, DTC-207 `WARN_BOILER_LINK_NOT_FAILSAFE` is active.
  - Note: the reference receiver has no timeout and will be replaced by an ESP32-class receiver (link type open).
- **FR-17.1 (Idempotent command):** *[now]* {SW; test} An ON request while ON (or OFF while OFF) sends no radio frame and returns
  HTTP 200 with `"idempotent":true`.
- **FR-17.2 (Radio cooldown):** *[now]* {SW; test} At least 3 s between two radio frames; a frame requested earlier is rejected
  by the driver.
- **FR-17.3 (Manual boiler requests):** *[now]* {SW; test} Manual ON/OFF requests are subject to FR-4. Only OFF may be forced
  (`force=true`), which stops the burner at the next control tick (NFR-1.8). No mechanism starts the burner inside
  the guard.
- **FR-17.4 (Rejected request response):** *[now]* {SW; test} A request rejected by the guard returns HTTP 429 with
  `status` = `rate_limited` or `pending`, the remaining `waitSec` and the current boiler state.
- **FR-17.5 (Request spam):** *[now]* {SW; test} More than 3 rejected requests within 10 s raise DTC-201 `WARN_BOILER_SPAM_DETECTED`.
- **FR-17.6 (Radio timing):** *[now]* {SW; test, analysis} One radio frame blocks `loop()` for ≤ 300 ms with interrupts disabled for
  ≤ 150 ms; rejected requests send no frame.
- **FR-17.7 (State repeat / heartbeat):** *[now]* {SW; test} While no command is pending, the controller re-sends the current
  boiler state every `hbInterval`, so a lost OFF is repeated and FR-55 has its heartbeat.
- **FR-32 (Deferred boiler request):** *[now]* {SW; test} A request rejected by FR-4 is queued (`PENDING_ON` / `PENDING_OFF`)
  and executed when the guard expires. An opposite request or `POST /api/boiler/cancel` cancels it.

### 3.4 Sensors

- **FR-6.1 (Supported sensors):** *[now]* {SW; test, inspection} Xiaomi LYWSD03MMC with PVVX / ATC firmware, BTHome v2 or ATC format.
- **FR-6.2 (Sensor values):** *[now]* {SW; test} Temperature (0.1 °C), relative humidity (1 %), battery (%), RSSI (dBm).
- **FR-6.3 (Passive scan):** *[now]* {SW; inspection} Sensors are received by passive BLE scan on the controller (NimBLE).
- **FR-28 (Scan duty):** *[now]* {SW; inspection} BLE passive scan at ≥ 90 % duty cycle.
- **FR-49 (Sensor offset):** *[now]* {SW; test} Per-zone offset −3.0…+3.0 °C added to the reading, stored in NVS.
- **FR-53 (Reading plausibility):** *[now]* {SW; test} Only accepted readings drive control and window detection:
  - a reading outside `validMin`–`validMax` is ignored and raises DTC-103 `ERR_TEMP_OUT_OF_BOUNDS`;
  - a reading differing from the last accepted one by more than `maxJump` is held and accepted only if the next
    reading confirms it within 0.5 °C, otherwise discarded and counted;
  - 3 discarded readings within 1 h raise DTC-104 `WARN_SENSOR_SUSPECT`.
  - A real temperature drop (open window) repeats in consecutive readings and therefore passes.
  - *Fallback:* the last accepted reading is used until the sensor timeout (FR-16.2).
  - Optional later: per-sensor BTHome encryption key; unencrypted adverts ignored when a key is set.
- **FR-20 (Link quality):** *[now]* {SW; test} Each sensor is classed by RSSI: EXCELLENT ≥ −72 dBm, GOOD −73…−82 dBm,
  MARGINAL −83…−88 dBm, POOR < −88 dBm; shown via API and console.
- **FR-22 (Boot warmup):** *[now]* {SW; test} After every boot, automatic heating waits up to 300 s for zone sensors. The warmup
  ends early when all mapped sensors were received. Zones whose sensors were received control normally during the
  warmup. At the end, each zone still without reading enters FR-51 and raises DTC-101 `ERR_SENSOR_TIMEOUT`.
  Frost protection is active during the warmup.
- **FR-11.1 (No installation data in firmware):** *[now]* {SW; inspection} The firmware ships without sensor MAC addresses or zone
  names; they are set at commissioning and stored in NVS.
- **FR-11.3 (Zone mapping):** *[now]* {SW; test} Each zone maps to a name, one sensor (MAC), one relay output and a target;
  set in the engineering view of the page (FR-8.1), via API (`/api/setup/...`) and console. The sensor is set by
  typing its MAC or by choosing it from the list of a sensor search (FR-11.5).
  - A zone name is 1-31 bytes and a sensor name 0-31 bytes, UTF-8, without `"`, `\` or a control character.
  - A sensor MAC serves at most one zone; a setting that gives a MAC already used by another zone returns HTTP 400
    and changes nothing.
  - The engineering view saves all zones with one button (`Salvează zonele`): one request carries every zone shown,
    and the zones are checked together as in FR-65.5, on the configuration that results, so two zones can exchange
    their sensors or relays in one save. A zone that breaks a check is not saved and the page shows its reason; the
    other zones are saved.
  - A relay output serves at most one zone, and not the one the pump uses (FR-14.1); a zone is switched on only with
    a sensor and a relay output. A rejected setting returns HTTP 400 and changes nothing.
  - A zone exists once it is added (FR-73) and until it is deleted (FR-74); switching it on or off does not change
    the number of zones.
  - While a zone's valve is energised, its relay is not changed and the zone is not switched off (FR-3: until the
    overrun hold of migration step 4 exists); the setting is rejected and can be repeated once the valve is closed.
- **FR-11.5 (Sensor search):** *[now]* {SW; test} The user starts a sensor search with the button `Caută senzori`
  in the engineering view (FR-8.1) or by API, and ends it with Stop or after 10 min, whichever comes first (a fixed
  time, not a parameter). While it runs, every compatible sensor heard (FR-6.1) is listed with MAC, temperature,
  battery and RSSI (FR-6.2). The list holds the sensors assigned to zones (never dropped, FR-11.6) plus at most 32
  sensors not assigned to a zone, the first 32 heard; further ones are ignored. Starting a search removes from the
  list the sensors not assigned to a zone; when the search ends, the sensors not assigned to a zone are dropped.
  `start` during a running search does nothing (the list and the 10 min are not restarted); `stop` with no search
  running does nothing; both answer HTTP 200 with the state of `discovered-sensors`.
  - *Reports:* `GET /api/setup/discovered-sensors` returns `active`, `remainingSec` (0 when not active, at most 600)
    and `sensors[]` with `mac`, `name`, `temp`, `hum`, `bat`, `rssi`, `ageSec` and `zone` (the number of the zone the
    sensor is assigned to, or 0).
- **FR-11.6 (Only assigned sensors kept):** *[now]* {SW; test} Outside a search the controller keeps only the
  sensors assigned to zones. Every zone's sensor has its place whatever other sensors are heard (a zone never loses
  its sensor to the neighbours' sensors, PB-55).
- **FR-11.4 (Persistence):** *[now]* {SW; test} Mapping, names and settings are stored in NVS and survive reboot and power loss.
- ~~**FR-11.2 (Setup wizard page `/setup`):** retired, page removed; discovery kept in FR-11.3.~~
- **FR-72 (Additional BLE receivers):** *[nice to have]* {SW, HW; test} Optional extra ESP32 receivers forward sensor adverts to the
  controller over the LAN; the controller uses the most recent accepted reading.
  - Interim: a sensor below −90 dBm for 1 h raises DTC-105 `WARN_SENSOR_WEAK_SIGNAL`.

### 3.5 Protections and fault detection

- **FR-16.1 (Frost protection):** *[now]* {SW; test} If any zone — enabled or disabled — with an accepted reading
  is below `frostTemp`, that zone heats in any mode (AUTO, MANUAL, global OFF) until it reaches `frostTemp` + 1.5 °C.
  Interlock (FR-2) and burner guards (FR-4) still apply.
- **FR-16.2 (Sensor timeout):** *[now]* {SW; test} A zone with no accepted reading for longer than `sensorTimeout`
  enters FAULT, raises DTC-101 `ERR_SENSOR_TIMEOUT` and continues with FR-51. Normal control resumes on the first
  accepted reading.
- **FR-18 (Sensor timeout setting):** *[now]* {SW; test} `sensorTimeout` is set via API and console, stored in NVS; one value
  is used for control and for DTC-101 `ERR_SENSOR_TIMEOUT`.
- **FR-40 (Window detection, simple rule):** *[now]* {SW; test} One global `windowThreshold` for all zones: a drop of at least
  the threshold within 5 min closes that zone's valve for 30 min, or until the temperature rebounds by 0.6 °C from
  the minimum, or until `POST /api/zones/window/clear`. Per-zone thresholds are removed.
  - Used whenever FR-40.2 lacks outdoor or humidity data.
  - *Fallback:* a false detection ends after 30 min at most; frost protection overrides the pause.
- **FR-40.2 (Smart window detection):** *[phase 4]* {SW; board, analysis} Each zone evaluates a sliding window of ≥ 3 accepted readings
  (default 5 min). A window-open event is declared when
  - the temperature slope ≤ −`windowK` × (room − outdoor) °C/min **and** absolute humidity (from temperature and
    relative humidity) drops by ≥ `windowAhDrop` g/m³ in the same window, or
  - the temperature drop alone exceeds 2 × `windowThreshold`.
  - Detection is suspended while room − outdoor < 5 °C (an open window does not cool the room noticeably).
  - The event ends when the slope stays ≥ 0 and absolute humidity recovers for 3 readings, or after 30 min.
  - Parameters are global; `windowK` and `windowAhDrop` are set from recorded traces.
  - Acceptance on recorded traces of the reference install: ≥ 90 % of openings longer than 3 min detected within
    5 min; ≤ 1 false event per zone per week.
  - *Fallback:* no outdoor or humidity data → FR-40.
- **FR-54 (Flow temperature limit):** *[now; sensors optional]* {SW, HW; test} Optional flow and return temperature sensors at the
  manifold, enabled in configuration (`flowSensors`). If the flow exceeds `flowMax` for more than 2 min, the boiler
  is switched OFF without waiting for the burner minimum run (forced OFF, FR-17.3) until the flow is 5 °C below
  `flowMax`, then normal control resumes with all guards (no lockout). The 2 min filter prevents chatter.
  - *Reports:* DTC-205 `ERR_FLOW_OVER_LIMIT`; flow / return values in status.
  - *Fallback:* sensors absent or implausible → function disabled, DTC-206 `WARN_FLOW_SENSOR_FAULT`; protection
    relies on the boiler setpoint (§2 preconditions).
  - Sensor type is open (wired DS18B20 would need new libraries — owner approval per AGENTS.md).
- **FR-56 (Zone overheat):** *[phase 4]* {SW; board} If a zone is above target + `overheatMargin` and still rising for
  `overheatTime` with its valve commanded closed, DTC-203 `ERR_ZONE_OVERHEAT` is raised (valve stuck open or wrong
  NC/NO setting). Other zones continue normally.
- **FR-57 (Heating without effect):** *[phase 4]* {SW; board}
  - A zone with duty ≥ `noEffectDuty` for `noEffectTime` that rises less than `noEffectRise` raises
    DTC-202 `ERR_ZONE_NO_EFFECT`.
  - If all demanding zones show it, or the flow temperature (FR-54) does not rise within 15 min of boiler ON, the
    controller re-sends ON once; if there is still no effect after another 15 min, DTC-204 `ERR_BOILER_NOT_HEATING`
    is raised.
  - Other functions continue.

### 3.6 Schedule, modes and season

- **FR-29 (Levels & Moments):** *[now]* {SW; test} Each zone has comfort levels and multi-moment targets (up to 4 configurable daily moments P1..P4: Wake, Leave, Return, Sleep, with user-customizable moment names, plus configurable global Away temperature per zone; 5–30 °C; factory 23 / 21 / 18 °C).
- **FR-30 (Level storage):** *[now]* {SW; test, inspection} Level and moment temperatures are stored in NVS, written only on change.
- **FR-31 (Schedule):** *[now]* {SW; test} Multi-moment daily schedule supporting 4 selectable scheduling modes: Constant 24/7 (single setpoint all days), Weekly Unified (L–D identical moments), 5+2 (Weekday Mon–Fri / Weekend Sat–Sun), and 7-day Individual (Mon–Sun each customizable). Up to 8 slots per day HH:MM=level or direct moment targets, kept sorted. Time inputs use 24h format (HH:MM). Active moment indicator is day-group aware (active moment displayed exclusively for current day group; in Constant 24/7 mode, indicates Constant 24/7 rather than moment index). Bilingual display (English / Romanian, NFR-3.1).
- **FR-44 (Away):** *[now]* {SW; test} Global away sets every zone's target to its away level and ends boosts; switching away off
  returns to the schedule.
- **FR-12.1 (Time):** *[now]* {SW; test} Time from NTP with the Romanian time zone (`EET-2EEST,M3.5.0/3,M10.5.0/4`), including
  daylight-saving changes.
- **FR-61 (No valid time):** *[now]* {SW; test} Without valid time, each zone keeps the last active schedule level (stored in NVS
  on level change) until NTP succeeds.
  - *Reports:* DTC-302 `WARN_TIME_NOT_SET`, 'time not set' in the page.
- **FR-64 (Global OFF):** *[now]* {SW; test} Global OFF mode: no heating except frost protection (FR-16.1), valve exercise
  (FR-19) and zones in sensor FAULT when it is cold outside (FR-51). Entered manually or by the season logic (FR-12.2).
- **FR-12.2 (Heating season):** *[now]* {SW; test, board} Season mode AUTO / ON / OFF; season OFF = global OFF (FR-64).
  - AUTO switches ON when the daily mean outdoor temperature is below `heatLimit` for `seasonOnDays` consecutive
    days, and OFF when it is above `heatLimit` + 2 °C for `seasonOffDays` consecutive days.
  - Safety net (season mode AUTO only): in season OFF, if an enabled zone with accepted readings stays below its
    night level for 12 h, the season switches ON. A season OFF chosen by hand is kept; frost protection covers
    safety.
  - *Fallback:* no weather data → season state unchanged. Every change is logged.
- ~~**FR-12.3 (Open-Meteo weather):** retired, merged into FR-9.~~
- ~~**FR-12.4 (City search, county list, GPS button):** retired, replaced by FR-9.2.~~

### 3.7 Weather and predictive control

- **FR-9 (Outdoor temperature):** *[now]* {SW; test} Outdoor temperature from Open-Meteo every 15 min; retry after 5 min on
  failure.
  - *Fallback:* last value is used for 6 h, then functions that need it use their own fallback;
    DTC-401 `WARN_WIFI_SIGNAL_DEGRADED` covers a network loss.
- **FR-9.2 (Location):** *[now]* {SW; test} Weather location as latitude, longitude stored in NVS; set via API and console.
- **FR-67 (Learned heat-up model):** *[phase 4]* {SW; board} Per zone, the heat-up rate (°C/h as a function of outdoor
  temperature) is learned from operation, stored in NVS, and used for the pre-heat lead instead of the FR-48 formula.
  - *Fallback:* too little data or values out of bounds → FR-48 formula (NFR-5.3).
- **FR-63 (Aligned cycles, boiler minimum load):** *[phase 4]* {SW; test} TPI cycles of all zones start aligned, so open valves
  overlap. A zone below `minLoops` circuits demanding alone is extended or grouped with another zone, so the boiler
  does not short-cycle on its own thermostat.
- **FR-70 (Solar gain throttling):** *[phase 4]* {SW; board} For zones flagged `solarGain`, if the Open-Meteo irradiance forecast
  for the next 3 h exceeds `solarLimit`, the TPI duty is reduced by `solarReduction`. Values are set from recorded data.
  - *Fallback:* no forecast → no reduction.
- **FR-71 (Humidity compensation):** *[nice to have]* {SW; test} Per-zone opt-in (`humidityComp`): at relative humidity ≥ 60 % the control target
  is lowered by up to 0.5 °C; the effective target is shown in the admin view.
  - *Fallback:* no humidity reading → no compensation.

### 3.8 Diagnostics

- **FR-10.2 (Diagnostic codes):** *[now]* {SW; test} The controller raises the codes listed in §5 and shows active codes in the
  page and `/api/status`. The web interface provides a dedicated active DTCs panel with code, severity, human-readable cause explanation, and duration, or confirms normal operation when no DTC is active.
- **FR-10.4 (Basic self-test):** *[now]* {SW; test} `/api/diag/status`, `/api/diag/selftest`, console `DIAG` and the
  engineering view report a list of checks, each with its rule, its measured value and the result `OK` / `FAIL`, or
  `INFO` when the controller cannot verify it. The UI clarifies rules as operating acceptance criteria without ambiguous code prefixes, and supplements checks with operational status for actuators/relays and boiler state. No output is switched; no health score or percentage is reported.

  | Check | `OK` when |
  |---|---|
  | `wifi` | Wi-Fi up and RSSI not below the DTC-401 limit (§5) |
  | `heap` | free heap not below the DTC-402 limit (§5) |
  | `ble` | BLE scan running |
  | `sensors` | every switched-on zone has an accepted reading within `sensorTimeout` (the DTC-101 rule); value: zones with a reading / zones switched on; `INFO` while no zone is switched on |
  | `time` | clock synchronised |
  | `weather` | outdoor temperature not older than 6 h (FR-9 fallback) |
  | `radio` | always `INFO`: last command and its age; the 433 MHz link has no return channel, so the boiler's reaction cannot be checked |

  The full self-test is FR-60.
- **FR-62 (Event history and alarms):** *[now]* {SW; test} The last 50 safety events (DTC raised / cleared, frost, FAULT,
  watchdog reset, controlled restart) are kept across reboots with time stamp (uptime if time is not set), written
  only when an event occurs. Alarms are shown in the page and `/api/status`; there is no push channel.
  - *Fallback:* event store cannot be written → events are kept in RAM until reboot, with a log line.
- **FR-21 (Firmware identity):** *[now]* {SW; test} Version, build date and time, git commit and reset reason are reported in
  status, console `STATUS` and the boot message.
  - The version is `1.MM.DD.NN`: `MM` and `DD` are the month and day of the last commit that changed a firmware input
    (`docs/process.md` §6), `NN` is the count of such commits on that day up to and including it (`01` for the
    first), e.g. `1.11.01.01`. It is computed from git at build time, so the same firmware always gets the same
    version; the git commit tells apart two versions of the same day in different years.
- **FR-23 (Console status):** *[now]* {SW; test} `STATUS` shows zones, sensors, boiler, protections, heap (free, minimum, largest
  block) and OTA margin. `STATUS` and `/api/status` also report, since boot, the longest control-tick delay (how
  much later than due a tick started) and the longest web/loop pass, so NFR-1.8 can be checked on the board.
  Layout is not specified.
- ~~**FR-24 (Hardware telemetry in STATUS):** retired, merged into FR-23.~~
- ~~**FR-25 (Zone cards, UTF-8 degree sign):** retired.~~
- ~~**FR-26 (Web console colouring):** retired, page removed.~~
- ~~**FR-27 (STATUS table layout, `utf8VisibleLen`):** retired.~~
- ~~**FR-10.1 (Cloud connection):** retired, out of scope.~~
- ~~**FR-10.3 (Service mode):** retired, removed in phase 2.~~
- ~~**FR-10.5 (Fleet OTA):** retired, out of scope.~~

### 3.9 Interfaces and security

- **FR-7.1 (Telnet console):** *[now]* {SW; test} Telnet server on port 23.
- **FR-7.3 (Console commands):** *[now]* {SW; test} Console commands are accepted via Serial, Telnet and HTTP (FR-45); `HELP`
  lists them.
- **FR-45 (Console over HTTP):** *[now]* {SW; test} `POST /api/console/send` executes a console command and returns the new log
  lines; `CLEAR` empties the log buffer.
- ~~**FR-7.2 (Web console page `/console`):** retired, page removed.~~
- **FR-8.1 (Web page):** *[now]* {SW; test, inspection} One page served at `/`:
  - family view: rooms (temperature, relative humidity e.g. 💧 X% RH, active heating state with TPI cycle minutes and duty percentage, TPI pause status, boost indicator, without redundant setpoints in subtitles), outdoor weather, boost, one global Home / Away switch,
    active alarms; each room card provides direct vertical setpoint steppers ([+], target, [-], boost reset) visible
    without expanding; the header says in words what the schedule does now and when it changes next (e.g.
    `Program: zi până la 22:30, apoi noapte`), or why the house does not follow it (away, schedule off);
  - admin view (`#admin`) is the engineering view, like the watch window of a bus or debugger tool: every value the
    API reports (zones, boiler, sensors, parameters, schedule, diagnostics, time and weather, system) is shown with its
    API name (e.g. `zones[2].relayEngaged`) and a short explanation, and on tap the requirement it belongs to; a value
    without an explanation is still shown; a value that changed since the last refresh is highlighted; while the view
    is open it keeps a trace of every change with its time, exportable as CSV; it holds:
    - the zone commissioning (FR-11.3): a button adds a zone (FR-73), a button on each zone deletes it (FR-74) after
      a confirmation;
    - the sensor search (FR-11.5): the button `Caută senzori`, a Stop button, the remaining time and the list of
      sensors heard, from which a sensor is chosen for a zone;
    - the checks of FR-10.4;
    - the configuration export, which saves the answer of GET /api/config/export as a .json file, and the
      import: upon selecting a file, the page parses the JSON, renders a diff preview comparing incoming settings
      against current configuration (GET /api/config/export), warns on invalid parameters against §6 ranges and
      zone rules with Romanian explanations, and requires explicit user confirmation before sending the body to
      POST /api/config/import with Content-Type: application/json (FR-65.7); on completion, it shows the lists
      and reasons of FR-65.6;
  - zone states are shown with the codes and texts of FR-8.4;
  - Family view language configurable (Romanian / English, NFR-3.1).
  - Behind a reverse proxy that serves the controller under a path prefix (e.g. `/heating/` on a home server) the
    page works as served directly: every URL the page requests is relative to the page address (no leading `/`).
- ~~**FR-8.2 (Remote access via Tailscale):** retired, user network setup, not product behaviour.~~
- **FR-8.4 (Zone states):** *[now]* {SW; test} The API reports each zone in exactly one of these states, the first that
  applies from the top; the page shows the code and the text. Any other value is a defect.

  | Code | Page text | When |
  |---|---|---|
  | `FROST` | Protecție îngheț | frost protection heats the zone (FR-16.1) |
  | `DISABLED` | Zonă oprită | zone switched off (FR-11.3), including a zone just added (FR-73) |
  | `FAULT` | Senzor pierdut – încălzește de rezervă | sensor in timeout (FR-16.2, FR-51) |
  | `ACQUIRING` | Așteaptă senzorul | after boot, no reading yet, at most the warmup (FR-22) |
  | `WINDOW` | Fereastră deschisă | window pause (FR-40) |
  | `HEATING` | Încălzește | valve opening or open |
  | `IDLE` | Temperatură atinsă / Pauză TPI | valve closed, no heat demand; page shows 'Temperatură atinsă' when room >= target - 0.2 °C, else 'Pauză TPI' (PB-66, C-14) |
- **FR-8.3 (Firmware update):** *[now]* {SW; test} Firmware update via `/update` with authentication (FR-58);
  `tools/sh_ota_upload.py` completes a 1.25 MB image in < 30 s on the home LAN. OTA rules: FR-60.
- **FR-15.1 (Actuator polarity):** *[now]* {SW; test} NC or NO actuator type is configurable (API, console), stored in NVS; all
  valve commands respect it.
- ~~**FR-15.2 (Compatibility statement):** retired, merged into FR-15.1.~~
- **FR-34 (Telemetry history):** *[now]* {SW; test} History in LittleFS: 24 h at 5 min and 5 days at 30 min resolution, served by
  GET /api/history?range=24h|5d. The web UI presents a 24h timeline / history chart (grpHistory) displaying burner flame and per-zone valve binary activations labeled by configured zone names, outdoor and room temperatures, and time-cursor inspection.
- **FR-35 (Burner counters):** *[now]* {SW; test} Burner time counters: total, today, yesterday, this month, last month,
  current burn; reported in boiler status. Zone-level heating runtime counters (todaySec, yesterdaySec) are reported in `/api/zones` and on zone cards.
- **FR-35.1 (Gas meter readings and boiler flow setpoint):** *[now]* {SW; test} The readings of the physical gas meter that the user types are data: kept, never overwritten, used for the consumption estimate (C-25).
  - *Reading:* the user types the meter index (m³, 3 decimals, 0–999999) in the page (`Citire contor`) and may give the time of the reading (default: now). Each typed reading is stored as one entry `{reading time (epoch), index, burner runtime (s) when it was typed}`; a reading with a given time in the past has no runtime (unknown) and is not used for the measured flow. Only a typed reading writes an entry: not a change of `boilerSetTemp`, `boilerFlowObserved`, `boilerPowerKw`, `gasPrice` or `gasFlameRate`, not an import, not a restore. A reading outside the range, or with a time in the future, is refused (HTTP 400) and writes nothing.
  - *Storage:* the entries are in a LittleFS file that a firmware flash does not erase; the last reading (index, time) and the burner runtime at that time are in NVS, written only on change (NFR-2.9). Entries survive a flash and a reset; the configuration export and import neither carry nor delete them (FR-65.1). Up to 1000 entries (about 8 KB). Over 1000, the oldest entry is replaced by the new one (about 10 years of readings typed now and then) and the self-test (FR-10.4) reports `INFO` with the number of replaced entries; nothing is replaced before the log is full.
  - *Estimate and measured flow:* the user is never asked for the burner flow. Between two consecutive readings that both have a runtime, with at least 1 h of boiler ON time (the time the controller asks for heat: the controller has no flame signal, so this is not the flame time) and a rise of the index, the flow is rise of the index / boiler ON hours. `gasFlowMeasured` (m³/h) is the average of the last 10 such intervals weighted by their boiler ON hours, or null while there is none. Estimated index = last reading + burner runtime since it × (`gasFlowMeasured`, or `gasFlameRate` while null). Gas of a month (m³) = last reading of the month − last reading of the previous month, when both exist; otherwise the runtime estimate. Gas today, yesterday and this month from the runtime counters (FR-35) × the same flow.
  - *Setpoint and price:* `boilerSetTemp` (dial of the boiler, default 40 °C, 25–60 °C), `boilerFlowObserved` (default 43 °C, 25–65 °C) and `gasPrice` (default 3.30 Lei/m³, 1.00–10.00 Lei/m³) are §6 parameters that the user may set (page and `/api/config/system`); an out-of-range value is rejected (HTTP 400, FR-42). `gasFlameRate` (default 0.60 m³/h, 0.20–2.50 m³/h) and `boilerPowerKw` (default 24 kW, 10–45 kW) are **not** set by the user: no input in the page, and `/api/config/system` refuses them with HTTP 400 (read-only, owner 03.11.2026: nobody can calculate them). `gasFlameRate` is only the starting flow of the estimate until a flow is measured; `boilerPowerKw` is information only. The meter index is not a parameter: it is the last reading.
  - *Old log:* the first start of the firmware with this requirement ignores the old ring `/gas_calib.json` of the earlier firmware (it holds test entries) and removes it; the new log starts empty and is seeded only by readings the owner confirms (first reading 2265.304 m³ taken 03.11.2026 15:39, `docs/gas_meter_readings_03_11.md`). No entry of the old ring is carried over.
  - *Tests:* no system test types a reading, writes an entry or changes the stored reading on the controller; tests that exercise the endpoint use a test-only mode that does not store (the owner's data is never test data).
  - *Report:* `/api/boiler/status` reports the last reading with its time, the estimated index, `gasFlowMeasured` (m³/h or null), the number of stored readings, gas today, yesterday and this month; `GET /api/boiler/gas-readings` lists all readings (time, index). The page lists them with date and time and never calls a reading a calibration. The page shows gas of today, yesterday and this month, and their cost, only when a flow has been measured (`gasFlowMeasured` not null); before that it shows `—` and says that the consumption is calculated after two readings, because the starting flow `gasFlameRate` is not the flow of this installation. The page also shows the boiler ON time of today. No reading is a default: a controller without a reading shows `—` and estimates nothing.
- **FR-41 (Wi-Fi supervisor):** *[now]* {SW; test} Wi-Fi is checked every 10 s and reconnected; after 90 s down the stack is
  re-initialised. DTC-401 `WARN_WIFI_SIGNAL_DEGRADED` while down or RSSI < −82 dBm. Control continues (NFR-1.1).
- **FR-42 (Configuration API limits):** *[now]* {SW; test} `/api/config/system` exposes hydraulic and radio timings and rejects
  values outside the §6 ranges, in particular below the safety floors of FR-2, FR-3 and FR-4.
- **FR-43 (Relay test):** *[now]* {SW; test} Relay test is accepted only in MANUAL mode and only for a relay output
  that belongs to a zone or is the pump (FR-14.1), respecting NC/NO (FR-15.1); otherwise HTTP 409. A free output (no
  zone, not the pump) is refused with 409; a pin that is no relay output of the hardware returns HTTP 400.
- **FR-50 (Setup access point):** *[now]* {SW; board} If the configured Wi-Fi is not joined at boot, the controller opens access
  point `SmartHeating-Setup`; control continues.
- **FR-58 (Authentication):** *[now]* {SW; test} These actions require authentication (same credentials as OTA):
  - changing a safety parameter of §6: `interlockDelay`, `overrun`, `burnerMinRun`, `burnerMinPause`,
    `openLoopBypass`, `sensorTimeout`, `faultDuty`, `faultFrostOutdoor`, `frostTemp`, `hbInterval`,
    `boilerLinkHeartbeat`, `boilerLinkType`, `flowSensors`, `flowMax`, `actuatorType`, `pumpOutput`, zone outputs;
  - manual boiler requests (FR-17.3, FR-32), relay test, MANUAL mode, fault injection (NFR-5.5), credentials,
    configuration import (FR-65.3) whatever the file contains.
  - Comfort settings (levels, schedule, boost, away, `windowThreshold`) need no authentication.
  - CORS is limited to the controller's own origin.
  - *Fallback:* unauthenticated request → HTTP 401, no change.
- **FR-59 (Credentials):** *[now]* {SW; test} The first flash may use universal default credentials (OTA, access point, Wi-Fi).
  Credentials are changed via API or console without re-flashing and stored in NVS.
  - *Reports:* while any default is active, DTC-403 `WARN_DEFAULT_CREDENTIALS`; everything else works.

### 3.10 Maintenance and commissioning

- **FR-19 (Valve exercise):** *[now]* {SW; board} A valve closed for ≥ `exerciseIdleDays` is opened for 5 min with the boiler
  OFF; the optional pump runs during the exercise. Idle time is tracked per valve on epoch time and stored in NVS
  (on change only). Runs in every mode, including global OFF.
- **FR-60 (OTA safety and self-test):** *[now]* {SW; test}
  - OTA starts only with the boiler OFF.
  - After boot of a new image, automatic checks without the boiler must pass within 60 s: loop and watchdog
    alive, Wi-Fi up, and at least one advert received from a mapped sensor. The sensor check is skipped while no
    zone has a mapped sensor (before commissioning, FR-11.1). Only then is the image marked valid; otherwise the
    previous image is restored and DTC-303 `ERR_OTA_ROLLBACK` is raised.
  - Full self-test on request only (API, console): each zone relay switched, boiler link frame sent, one zone
    opened (interlock respected), boiler ON for `selfTestBurn`, flow rise checked if FR-54 sensors are enabled,
    boiler OFF with all guards. Result stored and shown. Failures raise DTC-304 `ERR_SELFTEST_FAILED`; a plant
    failure never causes a rollback. The boiler step is skipped in global OFF.
- **FR-65.1 (Configuration export):** *[now]* {SW; test, analysis} `GET /api/config/export` returns one JSON file with
  the format version (`format` = 1), the firmware version of FR-21 and every setting the user or the installer made:
  - `params`: every field that `POST /api/config/system` accepts (`api_reference.md`), with the name, unit, JSON type
    and current value that `GET /api/config/system` reports; no credential;
  - per zone: name, sensor MAC, sensor name, relay output, on / off, sensor offset (FR-11.3, FR-49); the zone's current target is not stored: the schedule sets it at every control pass
    (FR-29), and its setpoints are the level temperatures below;
  - the schedule: weekday and weekend slots (FR-31), its switches (schedule, pre-heat, window detection) and the
    level temperatures of each zone (FR-29): the day, night and away setpoint of every zone, which an import
    restores;
  - the location (FR-9.2), when it is set;
  - `learnedDuty`: the learned base duty of each zone in %, one value per zone, equal to `learnedDuty` of
    `GET /api/zones`. The step that builds the outdoor bands of FR-47 changes this item and `format`.
  - The number of zones is not a setting of the file; it follows from `zones`.
  - The export is valid JSON whatever text is stored (test: names with non-ASCII UTF-8 and every character FR-11.3
    accepts). The export of 32 zones (FR-69) with every name at its maximum length of FR-11.3, in its longest JSON
    escaping, is at most 16 384 bytes (analysis).
  - *Purpose:* the settings survive a firmware change or a replaced controller.
- **FR-65.2 (Not exported):** *[now]* {SW; test} The export has only the keys of FR-65.1 and `api_reference.md`
  format 1 (`format`, `firmwareVersion`, `params`, `zones`, `schedule`, `location`, `learnedDuty` and their fields):
  no credential (FR-59), no counter, and no state other than the learned base duty, in particular no burner counter,
  operating mode (FR-1.3), away, boost, DTC, event, history or sensor reading. An import never changes a credential:
  a credential in the file is skipped like an unknown name (FR-65.4).
- **FR-65.3 (Import check):** *[now]* {SW; test, analysis} `POST /api/config/import` with a file of FR-65.1 checks each
  value like the same setting set alone: the §6 range (FR-42), the zone rules of FR-11.3, the level range of FR-29, the
  slots of FR-31, the location of FR-9.2 and the learned duty bounds of FR-47. Each value that passes is in effect
  when the answer is sent (the export and the setting's own GET endpoint return it from then on) and is stored (NVS
  on change only, NFR-2.9). An imported learned duty outside the bounds of FR-47 is skipped like any other value: the
  current value stays and DTC-301 `WARN_LEARNING_RESET` is not raised.
  - Receiving and applying an import of at most 16 384 bytes delays no safety reaction beyond NFR-1.8 (analysis: the
    chain of architecture §5.4.1 with the NVS writes of the import). The answer comes within 2 s after the last byte
    of the request; the 100 ms of NFR-2.3 does not apply.
- **FR-65.4 (Partial import):** *[now]* {SW; test} A name the firmware does not know is skipped; a setting missing
  from the file keeps its current value; a value that fails its check is skipped and the current value stays. Unless
  FR-58 or FR-65.7 refuses the request, the answer is HTTP 200, also when every setting is skipped.
  - *Fallback:* a setting that cannot be applied keeps its current value (safety > comfort, NFR-5.1).
- **FR-65.5 (Zones checked together):** *[now]* {SW; test} A zone of the file is applied or skipped as a whole: one
  field that fails its check skips the zone; an unknown field is skipped alone (FR-65.4); a missing field keeps its
  value; a zone `id` the controller does not have is skipped (reason `no-zone`). The `id` is the zone number at the time of
  the export; an import never adds or deletes a zone. The pump setting of the file is
  checked first, like set alone, against the zones as they are before the import; the zones of the file are then
  checked against the resulting pump. In the configuration that results, a relay output serves at most one zone and
  not the pump (FR-11.3, FR-14.1), and a sensor MAC serves at most one zone (FR-11.3): every zone of the file that
  would break this is skipped (reasons `relay-taken`, `pump-relay`, `sensor-taken`), and the check repeats until no
  conflict is left. A MAC held by a zone of the controller that the file moves away is free for another zone of the
  file. A zone that FR-11.3 would reject if set alone
  because its valve is energised is skipped (reason `valve-energised`). The same `id` twice in `zones`,
  `schedule.levels` or `learnedDuty` skips every entry with that `id` (reason `duplicate`).
- **FR-65.6 (Import report):** *[now]* {SW; test} The HTTP 200 answer has the lists `applied`, `kept` and `skipped`,
  with the names of `api_reference.md`. Every setting of the export (FR-65.1, not `format` or `firmwareVersion`) and
  every unknown key of the file is in exactly one list; `kept` = missing from the file. Each `skipped` entry has a
  `reason`: `unknown`, `invalid`, `no-zone`, `relay-taken`, `pump-relay`, `sensor-taken`, `valve-energised` or `duplicate`. For each
  applied setting the console log has one line with its report name and new value.
- **FR-65.7 (Rejected file):** *[now]* {SW; test} The checks run in this order and the first that fails answers, with
  nothing changed: authentication (HTTP 401, FR-58); `Content-Type` other than `application/json` (HTTP 415; a
  parameter such as `charset=utf-8` is accepted); body larger than 16 384 bytes (HTTP 413); body not a JSON object,
  or `format` missing or not the number 1 (HTTP 400). No answer of `GET /api/config/export` or
  `POST /api/config/import`, the answer to an `OPTIONS` request on either path included, carries an
  `Access-Control-Allow-Origin` or `Access-Control-Allow-Headers` header, whatever the `Origin` of the request
  (FR-58).
- **FR-65.8 (Round trip):** *[now]* {SW; test} `POST /api/config/import` with the unchanged body of
  `GET /api/config/export` from the same controller answers HTTP 200 with `skipped` empty, and a following export is
  identical to the first.
- **FR-66 (Factory reset):** *[later]* {SW; test} Factory reset via console or API, with confirmation; keeps Wi-Fi credentials.
- **FR-73 (Add a zone):** *[now]* {SW; test} The user adds a zone in the engineering view (FR-8.1) or by API, only
  while a relay output is free: the outputs of the configured hardware minus the zones minus the pump output
  (FR-14.1). The new zone gets the lowest free relay output (in the order relay 1…4 of the board), the name `Zone n` (n = its number), no sensor, is
  switched off (FR-11.3) and has the factory levels (FR-29). Zone numbers are 1…N without gaps. A given name must
  pass the name rule of FR-11.3, otherwise HTTP 400 and nothing changes; without a name, or with an empty one, the
  name is `Zone n`. Success answers HTTP 200 with `zone` (its number) and `relayPin` (the output it got). With no
  free output, or with 32 zones already, the answer is HTTP 409 and nothing changes.
- **FR-74 (Delete a zone):** *[now]* {SW; test} The user deletes a zone in the engineering view or by API. Its
  mapping, levels and learned duty are removed, its relay output is free again (the valve is closed, because a zone
  with an energised valve is not deleted), and the zones after it move up by one number. Everything that belongs to
  a zone moves with it (mapping, target, offset, levels, learned duty, sensor timeout state, DTC-101
  `ERR_SENSOR_TIMEOUT`); a DTC of the deleted zone is cleared. Success answers HTTP 200. A zone whose valve is energised is not deleted (FR-3, as FR-11.3): HTTP
  409, nothing changes, the request can be repeated once the valve is closed. A zone number that does not exist
  returns HTTP 400.
- **FR-68 (Commissioning checklist):** *[now, documentation]* {install; inspection} Commissioning follows a checklist; the installer records the result in the commissioning document (the controller does not store it):
  1. installation — manifold flow meters balanced per zone; always-open bypass present; boiler flow setpoint
     ≤ 45 °C (§2);
  2. sensors — PVVX / BTHome firmware, advert interval ≤ 10 s; all sensors 24 h side by side next to a reference,
     offsets set to ±0.2 °C (FR-49); mounted 1.2–1.5 m high on an internal wall, away from floor radiation, sun,
     doors and drafts;
  3. function — relay test per zone (FR-43), valve opening time measured, boiler ON/OFF confirmed by the full
     self-test (FR-60), radio range (FR-5.2).
- **FR-69 (Number of zones):** *[now: configuration; hardware later]* {SW, HW; test} 0–32 zones (`number of zones`,
  §6), each with name, sensor and output; a new controller has none. The number of outputs is limited by the
  configured hardware; a zone always has an output, it gets one when it is added (FR-73) and an output cannot be
  removed from a zone. Memory for 32 zones is reserved statically (NFR-2.6).
  Relay 4 is a zone or the pump. A controller that already has zones keeps them.
  - *Fallback:* an output expansion that stops responding → its outputs count as closed (their unpowered state),
    DTC-208 `ERR_OUTPUT_EXPANSION_FAULT`; other zones continue. Details are fixed with the expansion hardware.

### 3.11 Retired requirements (IDs reserved)

- ~~**FR-13 (Device passport, FR-13.1–13.4):** retired, removed in phase 2; location moved to FR-9.2.~~
- ~~**FR-33 (Sparklines):** retired, not in the current page.~~
- ~~**FR-36 (Home Assistant package):** retired, out of scope.~~
- ~~**FR-46 (Dynamic time blocks):** retired, never defined.~~

---

## 4. Non-functional requirements

### 4.1 Safety

- **NFR-1.1 (Local autonomy):** {SW; board, inspection} Loss of Wi-Fi, router or internet does not stop heating control; all decisions run on
  the controller.
- **NFR-1.4 (Safe boot):** {SW; test} At every boot the controller drives all zone valves to closed (per FR-15.1),
  sends one boiler OFF, which starts `burnerMinPause` (FR-4), and enters the boot warmup (FR-22) before any heating
  command.
- **NFR-1.5 (Task watchdog):** {SW; inspection} A 90 s task watchdog covers every task that runs control or
  interfaces (with the control tick in its own task, architecture AR-21: `loop()` and the control task); if either
  hangs, the controller resets into the safe boot (NFR-1.4).
- **NFR-1.8 (Safety reaction time):** {SW; test, analysis} Every safety reaction has changed the outputs within
  10 s of its trigger; the 10 s are fixed, not a configuration parameter. Safety reactions are:
  - the output changes decided in each control tick (§0);
  - closing a valve without demand (FR-37);
  - stopping the pump when no valve is open any more (FR-14.5);
  - opening the valve of a zone below `frostTemp` (FR-16.1);
  - the forced boiler OFF of the flow limit, once its filter time has elapsed (FR-54);
  - the forced manual boiler OFF (FR-17.3); its trigger is the acceptance of the request by the controller (the
    network before that is outside the bound);
  - interlock and burner-guard decisions: boiler ON once `interlockDelay` and the guard have elapsed, a deferred
    request when its guard expires (FR-2, FR-4, FR-32).
  - The only exception is a firmware update (FR-8.3), which runs only with the boiler OFF (FR-60).
  - On the board the bound is checked with the longest control-tick delay reported by FR-23.
- ~~**NFR-1.2 (Sensor failsafe):** retired, merged into FR-16.2.~~
- ~~**NFR-1.3 (Frost protection):** retired, merged into FR-16.1.~~
- ~~**NFR-1.6 (Burner cycle limit 60 s + 60 s):** retired, merged into FR-4.~~
- ~~**NFR-1.7 (Failure-mode description):** retired, moved to the hydraulics document.~~

### 4.2 Resources and performance

- **NFR-2.2 (Free heap):** {SW; test, board} Free heap ≥ 100 KB during normal operation. Below 60 KB
  DTC-402 `WARN_HEAP_FRAGMENTATION` is raised; below 30 KB a controlled restart is performed while the boiler is OFF,
  with state restored from NVS (FR-62 records it).
- **NFR-2.3 (API response):** {SW; test} Local API response time < 100 ms, except `POST /api/config/import` (FR-65.3).
- **NFR-2.4 (Log time stamps):** {SW; test} Every console line carries a `[HH:MM:SS]` time stamp.
- **NFR-2.6 (No heap drift):** {SW; board} No heap decrease > 5 KB over 24 h of operation; buffers are bounded and reserved
  statically.
- **NFR-2.8 (Firmware size):** {SW; analysis} Firmware image < 1.25 MiB (the OTA partition size, C-22), so OTA fits.
- **NFR-2.9 (Flash wear):** {SW; inspection} NVS is written only when a value changes.
- ~~**NFR-2.1 (Power < 1.2 W):** retired.~~
- ~~**NFR-2.5 (Paced OTA upload):** retired, merged into FR-8.3.~~
- ~~**NFR-2.7 (Development server):** retired, tooling, documented in the README.~~

### 4.3 Code and language

- **NFR-3.1 (Language):** {SW; test, inspection} Code, identifiers, API messages, logs and documentation in English. The family view of the
  page is available in Romanian and English (configurable).
- **NFR-3.2 (Modules):** {SW; inspection} Firmware is split into the modules of [`architecture.md`](architecture.md), one
  responsibility each.

### 4.4 Comfort

- **NFR-4.1 (Comfort accuracy):** {SW; board} In steady state (no schedule change for 3 h, window closed) the room temperature
  stays within ±0.3 °C of the target.

### 4.5 Graceful degradation

- **NFR-5.1 (Priorities):** {SW; inspection} When goals conflict: safety > comfort > economy. A fallback may give up economy, never
  safety, and gives up comfort only when safety requires it. Faults are logged and raised as DTC; occupants shall
  not notice them.
- **NFR-5.2 (Fallback clause):** {SW; inspection} Every functional requirement states its inputs, its fallback when an input is
  missing or implausible, and the DTC or log entry raised. A requirement without a fallback is incomplete.
- **NFR-5.3 (Bounded learning):** {SW; test, inspection} Learned values (TPI base duty, heat-up model, window and solar parameters) are
  bounded; a learned value outside its bounds or a corrupt NVS record is discarded, the default is used and
  DTC-301 `WARN_LEARNING_RESET` is raised. Learning never stops control.
- **NFR-5.4 (Short interruptions):** {SW; analysis} Heating interruptions shorter than 60 min are acceptable as fallback (the screed
  buffers them, room drop < 0.3 °C). Longer fallbacks keep heating at an estimated duty (FR-51).
- **NFR-5.5 (Fault injection):** {SW; test} Every fallback can be triggered on the board for test via console / API (sensor
  lost, no weather, no time, no humidity, boiler link lost, flow over limit), so `tools/sh_test_system.py` can verify
  it. Fault injection is refused where it would bypass a safety rule, and ends with a reboot.

---

## 5. Diagnostic codes

| Code | Meaning | Raised by | Cleared |
|---|---|---|---|
| `DTC-101`: `ERR_SENSOR_TIMEOUT` | zone sensor without accepted reading > `sensorTimeout` | FR-16.2, FR-22 | first accepted reading |
| `DTC-102`: `WARN_BATTERY_LOW` | sensor battery < 20 % | FR-6.2 | battery ≥ 25 % |
| `DTC-103`: `ERR_TEMP_OUT_OF_BOUNDS` | reading outside `validMin`–`validMax` | FR-53 | next reading in range |
| `DTC-104`: `WARN_SENSOR_SUSPECT` | 3 discarded spikes within 1 h | FR-53 | 24 h without spikes |
| `DTC-105`: `WARN_SENSOR_WEAK_SIGNAL` | sensor RSSI < −90 dBm for 1 h | FR-72 | RSSI ≥ −88 dBm for 1 h |
| `DTC-201`: `WARN_BOILER_SPAM_DETECTED` | > 3 rejected boiler requests within 10 s | FR-17.5 | 10 min without rejection |
| `DTC-202`: `ERR_ZONE_NO_EFFECT` | zone demand without temperature rise | FR-57 | zone rises by `noEffectRise` |
| `DTC-203`: `ERR_ZONE_OVERHEAT` | zone rising above target with valve closed | FR-56 | zone below target |
| `DTC-204`: `ERR_BOILER_NOT_HEATING` | no zone and no flow reacts to boiler ON | FR-57 | any zone or flow rises |
| `DTC-205`: `ERR_FLOW_OVER_LIMIT` | flow above `flowMax` > 2 min | FR-54 | flow 5 °C below `flowMax` |
| `DTC-206`: `WARN_FLOW_SENSOR_FAULT` | flow / return sensor missing or implausible | FR-54 | valid readings for 10 min |
| `DTC-207`: `WARN_BOILER_LINK_NOT_FAILSAFE` | heartbeat disabled | FR-55 | heartbeat enabled |
| `DTC-208`: `ERR_OUTPUT_EXPANSION_FAULT` | output expansion not responding | FR-69 | expansion responds for 1 min |
| `DTC-301`: `WARN_LEARNING_RESET` | learned value discarded, default used | NFR-5.3 | manual clear |
| `DTC-302`: `WARN_TIME_NOT_SET` | no valid time | FR-61 | NTP success |
| `DTC-303`: `ERR_OTA_ROLLBACK` | new image failed its checks, previous image restored | FR-60 | manual clear |
| `DTC-304`: `ERR_SELFTEST_FAILED` | full self-test step failed | FR-60 | next passed self-test |
| `DTC-401`: `WARN_WIFI_SIGNAL_DEGRADED` | Wi-Fi down or RSSI < −82 dBm | FR-41 | Wi-Fi up and RSSI ≥ −80 dBm |
| `DTC-402`: `WARN_HEAP_FRAGMENTATION` | free heap < 60 KB | NFR-2.2 | free heap > 75 KB |
| `DTC-403`: `WARN_DEFAULT_CREDENTIALS` | a default credential is active | FR-59 | all credentials changed |

Every raise and clear is recorded in the event history (FR-62).

---

## 6. Configuration parameters

Set via API and console, stored in NVS (written on change only). A value outside its range is rejected
(HTTP 400 / console error) and never stored.

| Parameter | Default | Range | Requirement |
|---|---|---|---|
| `interlockDelay` | 180 s | 120–600 s | FR-2, FR-14.2 |
| `overrun` | 180 s | 180–600 s | FR-3 |
| `burnerMinRun` / `burnerMinPause` | 180 s / 180 s | 120–600 s | FR-4 |
| `hysteresis` | 0.3 °C | 0.1–1.0 °C | FR-47 |
| `boostDuration` | 3 h | 1–6 h | FR-39 |
| `manualTimeout` | 3 h | 1–6 h | FR-52 |
| `openLoopBypass` | off | on / off | FR-1.3 |
| `faultDuty` | 20 % | 0–30 % | FR-51 |
| `sensorTimeout` | 20 min | 1–60 min | FR-16.2, FR-18 |
| `frostTemp` | 12 °C | 8–16 °C | FR-16.1 |
| `validMin` / `validMax` | 5 / 45 °C | 0–10 / 35–50 °C | FR-53 |
| `maxJump` | 2.0 °C | 0.5–5.0 °C | FR-53 |
| `windowThreshold` | 1.2 °C | 0.8–2.0 °C | FR-40 |
| `windowK`, `windowAhDrop` | from traces | bounded per NFR-5.3 | FR-40.2 |
| `hbInterval` | 60 s | 30–900 s | FR-17.7, FR-55 |
| `boilerLinkHeartbeat` | on | on / off | FR-55 |
| `flowSensors` | off | on / off | FR-54 |
| `flowMax` | 45 °C | 35–55 °C | FR-54 |
| `overheatMargin` / `overheatTime` | 2 °C / 60 min | 1–5 °C / 30–180 min | FR-56 |
| `noEffectDuty` / `noEffectTime` / `noEffectRise` | 50 % / 3 h / 0.2 °C | 30–100 % / 1–6 h / 0.1–1.0 °C | FR-57 |
| `heatLimit` | 15 °C | 10–18 °C | FR-12.2 |
| `seasonOnDays` / `seasonOffDays` | 2 / 5 days | 1–7 / 1–10 days | FR-12.2 |
| `exerciseIdleDays` | 14 days | 7–14 days | FR-19 |
| `selfTestBurn` | 10 min | 5–20 min | FR-60 |
| `minLoops` | 3 circuits | 1–7 | FR-63 |
| `solarGain`, `solarLimit`, `solarReduction` | off / from data | per zone, bounded | FR-70 |
| zone levels day / night / away | 23 / 21 / 18 °C | 5–30 °C | FR-29 |
| sensor offset | 0.0 °C | −3.0…+3.0 °C | FR-49 |
| `faultFrostOutdoor` | 5 °C | −10…+10 °C | FR-51, FR-64 |
| `actuatorType` | NC | NC / NO | FR-15.1 |
| `pumpOutput` | none | none / one relay output | FR-14.1 |
| `boilerSetTemp` | 40 °C | 25–60 °C | FR-35.1 |
| `boilerFlowObserved` | 43 °C | 25–65 °C | FR-35.1 |
| `gasFlameRate` | 0.60 m³/h | 0.20–2.50 m³/h | FR-35.1 |
| `boilerPowerKw` | 24 kW | 10–45 kW | FR-35.1 |
| `gasPrice` | 3.30 Lei/m³ | 1.00–10.00 Lei/m³ | FR-35.1 |
| `boilerLinkType` | `rf433` | `rf433` / `wired` / `receiver` (ESP32-class, link open) | FR-55 |
| `seasonMode` | AUTO | AUTO / ON / OFF | FR-12.2 |
| `location` | not set (commissioning) | latitude −90…90, longitude −180…180 | FR-9.2 |
| `pageLanguage` | `ro` | `ro` / `en` | FR-8.1, NFR-3.1 |
| credentials (OTA, access point, Wi-Fi) | universal default | 8–63 characters | FR-59 |
| number of zones | 0 | 0–32 (set by adding and deleting zones) | FR-69, FR-73, FR-74 |
| zone mapping (name, sensor MAC, output) | empty (commissioning) | one per zone | FR-11.3, FR-69 |
| `circuits` per zone | 1 (reference: 3 / 2 / 2) | 1–16 | FR-63 |
| `humidityComp` | off | on / off per zone | FR-71 |

---

## 7. Traceability

The only hand-kept links are requirement → module (Requirements column of [`architecture.md`](architecture.md)
§3.2), test → requirement (test docstrings) and code tags. The matrix requirements ↔ modules ↔ tests ↔ code is
generated from them:

```bash
python tools/sh_process_check.py trace --matrix
```
