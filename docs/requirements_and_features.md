# System and Software Requirements — Smart Home Heating

**Baseline:** v2, 31.10.2026 — derived from the owner-approved review [`requirements_review.md`](requirements_review.md).
**Replaces:** the Romanian specification of 07.10.2026 (v1). IDs are kept where the meaning did not change.
**Related:** [`plan.md`](plan.md), [`changelog.md`](changelog.md), [`hardware_esp32_relay.md`](hardware_esp32_relay.md),
[`underfloor_heating_hydraulics_and_failsafes.md`](underfloor_heating_hydraulics_and_failsafes.md), [`api_reference.md`](api_reference.md).

---

## 0. How to read this document

- `- **FR-x (Title):**` defines a requirement. Struck-through items (`~~`) are retired and keep their ID reserved.
- **Phase tag** after the title says *when* it is implemented, not *whether* it is required:
  `[now]` current phase · `[phase 4]` predictive control · `[later]` after phase 4 · `[nice to have]` optional.
- Every functional requirement states its **inputs**, **fallback** and **reports** (NFR-5.2); fallbacks follow the
  priorities of NFR-5.1.
- Configuration parameters are written as `name`; default, range and storage are defined once, in §6.
- DTC codes are defined once, in §5.
- IDs are cited in full (`FR-6.1, FR-6.2`), never as a range, so the checker can trace them. A parent ID (`FR-6`)
  groups its children and is not a requirement of its own.

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

- **FR-47 (Time-proportional valve control):** *[now]* Each zone is controlled by time-proportional control (TPI):
  - cycle 60 min; ON time per cycle = duty × cycle;
  - duty = (target − room temperature) / 1.5 °C + learned base duty, limited to 0–100 %;
  - learned base duty 0–60 %, adapted per zone and per outdoor band of 5 °C (7 bands), stored in NVS when it
    changes by ≥ 5 % (NFR-2.9);
  - minimum valve ON time and OFF time 10 min each (wax actuators need 3–5 min to open);
  - a new cycle starts when the target changes by more than 0.4 °C;
  - the valve closes when the room exceeds target + `hysteresis`.
  - *Inputs:* room temperature (accepted readings, FR-53), target. *Fallback:* sensor lost → FR-51; learned duty
    out of bounds → default (NFR-5.3). *Reports:* duty per zone in the admin view (FR-8.1).
  - Acceptance examples: 1.5 °C below target → 100 %; 0.2 °C below target → 13 % with zero base duty, 25 % with
    a learned base duty of 12 % (blueprint example).
- ~~**FR-1.1 (Hysteresis ON/OFF loop):** retired, replaced by FR-47; hysteresis remains only as overshoot cut.~~
- **FR-1.2 (Boost):** *[now]* A boost raises one zone's target above its current schedule level by 0.5–2.0 °C in
  0.5 °C steps; it never lowers the target (to cool use away, FR-44). Firmware enforces the limits for every
  source (page, API, console). The boost ends as defined in FR-39.
  - *Fallback:* request outside the limits → rejected, current target kept.
- **FR-39 (Boost end):** *[now]* A boost ends at the first of: `boostDuration` elapsed, next schedule slot change,
  `POST /api/zones/hold/clear`. The schedule level is never modified by a boost.
  - *Fallback:* no valid time → the boost ends after `boostDuration` measured on uptime.
- **FR-1.3 (Operating modes):** *[now]* AUTO: zones and boiler follow FR-47. MANUAL: relays and boiler are commanded
  by the user (maintenance, tests). In MANUAL, boiler ON requires at least one valve open for ≥ `interlockDelay`
  (FR-2), unless `openLoopBypass` is set. Frost protection (FR-16.1) and burner guards (FR-4) apply in both modes.
- **FR-52 (Manual mode timeout):** *[now]* MANUAL returns to AUTO after `manualTimeout`. While MANUAL is active,
  the page shows a warning with the remaining time.
- **FR-48 (Weather-shifted pre-heat):** *[now]* If the next schedule slot has a higher level, it starts early by
  120 + 9 × (5 − outdoor °C) min, limited to 45–240 min (2 h at +5 °C, 3.5 h at −5 °C).
  - *Inputs:* outdoor temperature (FR-9), schedule. *Fallback:* outdoor value older than 6 h or missing → 90 min.
    Replaced by the learned model when FR-67 is available. *Reports:* lead time in the admin view.
- **FR-51 (Zone without sensor):** *[now]* A zone whose sensor is in timeout (FR-16.2) keeps heating with its learned
  duty for the current outdoor temperature (bounded by NFR-5.3); if nothing is learned, with `faultDuty`.
  Frost protection (FR-16.1), interlock (FR-2) and burner guards (FR-4) stay active. Normal control resumes on the
  first accepted reading.
  - In global OFF (FR-64) and for a disabled zone, the zone runs this duty only while the outdoor temperature is
    below `faultFrostOutdoor` or unknown (a lost sensor must not remove frost safety; safety > economy).
  - *Reports:* DTC-101 `ERR_SENSOR_TIMEOUT`, zone state FAULT in the page.

### 3.2 Hydraulic safety

- **FR-2 (Actuator opening interlock):** *[now]* Boiler ON is commanded only after at least one zone valve has been
  energised (opened) for ≥ `interlockDelay` (default 180 s). Values below the range in §6 are rejected.
  - *Fallback:* this is the safe state; if the demand ends during the interlock, FR-37 applies.
- **FR-3 (Overrun after flame OFF):** *[now]* The last zone valve that was open stays open while the boiler is ON
  or `PENDING_OFF` (FR-32), and after a boiler OFF that followed a lit flame for `overrun` (default 180 s),
  independent of the pump setting. The boiler never burns with all zone valves closed.
  - **FR-3.2 (No overrun without flame):** If the flame was never lit (start cancelled during the interlock), the
    overrun is 0 s and valves without demand close at once.
- ~~**FR-38 (Flame-conditioned pump overrun):** retired, merged into FR-3 / FR-3.2.~~
- **FR-4 (Burner anti-short-cycling):** *[now]* Burner minimum run time 180 s and minimum pause 180 s, applied to
  every ON/OFF source (automatic, API, console). Values below the range in §6 are rejected.
  - *Fallback:* a request inside the guard is deferred (FR-32), never dropped silently.
- **FR-37 (Valve follows demand):** *[now]* A zone without demand has its valve de-energised (closed, per FR-15)
  within one control cycle (1 s), except the overrun valve of FR-3. If demand drops during the interlock, the valve
  closes and no boiler ON is sent for that zone.
- **FR-14.1 (Optional circulation pump):** *[now]* One relay may be configured as circulation pump; disabled by
  default.
- **FR-14.2 (Pump interlock):** *[now]* The pump starts only after a valve has been open for ≥ `interlockDelay`.
- **FR-14.3 (Pump overrun):** *[now]* The pump runs during the overrun of FR-3.
- **FR-14.5 (No pump against closed valves):** *[now]* The pump runs only while at least one valve is open, or during
  the overrun with the overrun valve open.
- ~~**FR-14.4 (Weekly anti-seize 20 s):** retired, replaced by FR-19.~~

### 3.3 Boiler link

- **FR-5.1 (Radio frame):** *[now]* The 433.92 MHz OOK frame reproduces the original thermostat: 409 level
  transitions, 65 ms silence before and after the data train.
- **FR-5.2 (Radio range):** *[now]* The boiler receiver switches the burner ON and OFF on command from the
  controller's installed position — verified at commissioning (FR-68).
- **FR-55 (Fail-safe boiler link):** *[now; receiver hardware later]* Loss of the controller shall switch the boiler
  OFF:
  - wireless link: the controller sends its state every `hbInterval`; the receiver opens the boiler contact if no
    valid frame arrives within 3 × `hbInterval`;
  - wired link: the boiler contact is a dry normally-open relay contact on the controller (no power = OFF).
  - The heartbeat can be disabled (`boilerLinkHeartbeat` = off) for receivers without timeout.
  - *Reports:* while disabled, DTC-207 `WARN_BOILER_LINK_NOT_FAILSAFE` is active.
  - Note: the reference receiver has no timeout and will be replaced by an ESP32-class receiver (link type open).
- **FR-17.1 (Idempotent command):** *[now]* An ON request while ON (or OFF while OFF) sends no radio frame and returns
  HTTP 200 with `"idempotent":true`.
- **FR-17.2 (Radio cooldown):** *[now]* At least 3 s between two radio frames; a frame requested earlier is rejected
  by the driver.
- **FR-17.3 (Manual boiler requests):** *[now]* Manual ON/OFF requests are subject to FR-4. Only OFF may be forced
  (`force=true`), which stops the burner at once. No mechanism starts the burner inside the guard.
- **FR-17.4 (Rejected request response):** *[now]* A request rejected by the guard returns HTTP 429 with
  `status` = `rate_limited` or `pending`, the remaining `waitSec` and the current boiler state.
- **FR-17.5 (Request spam):** *[now]* More than 3 rejected requests within 10 s raise DTC-201 `WARN_BOILER_SPAM_DETECTED`.
- **FR-17.6 (Radio timing):** *[now]* One radio frame blocks `loop()` for ≤ 300 ms with interrupts disabled for
  ≤ 150 ms; rejected requests send no frame.
- **FR-17.7 (State repeat / heartbeat):** *[now]* While no command is pending, the controller re-sends the current
  boiler state every `hbInterval`, so a lost OFF is repeated and FR-55 has its heartbeat.
- **FR-32 (Deferred boiler request):** *[now]* A request rejected by FR-4 is queued (`PENDING_ON` / `PENDING_OFF`)
  and executed when the guard expires. An opposite request or `POST /api/boiler/cancel` cancels it.

### 3.4 Sensors

- **FR-6.1 (Supported sensors):** *[now]* Xiaomi LYWSD03MMC with PVVX / ATC firmware, BTHome v2 or ATC format.
- **FR-6.2 (Sensor values):** *[now]* Temperature (0.1 °C), relative humidity (1 %), battery (%), RSSI (dBm).
- **FR-6.3 (Passive scan):** *[now]* Sensors are received by passive BLE scan on the controller (NimBLE).
- **FR-28 (Scan duty):** *[now]* BLE passive scan at ≥ 90 % duty cycle.
- **FR-49 (Sensor offset):** *[now]* Per-zone offset −3.0…+3.0 °C added to the reading, stored in NVS.
- **FR-53 (Reading plausibility):** *[now]* Only accepted readings drive control and window detection:
  - a reading outside `validMin`–`validMax` is ignored and raises DTC-103 `ERR_TEMP_OUT_OF_BOUNDS`;
  - a reading differing from the last accepted one by more than `maxJump` is held and accepted only if the next
    reading confirms it within 0.5 °C, otherwise discarded and counted;
  - 3 discarded readings within 1 h raise DTC-104 `WARN_SENSOR_SUSPECT`.
  - A real temperature drop (open window) repeats in consecutive readings and therefore passes.
  - *Fallback:* the last accepted reading is used until the sensor timeout (FR-16.2).
  - Optional later: per-sensor BTHome encryption key; unencrypted adverts ignored when a key is set.
- **FR-20 (Link quality):** *[now]* Each sensor is classed by RSSI: EXCELLENT ≥ −72 dBm, GOOD −73…−82 dBm,
  MARGINAL −83…−88 dBm, POOR < −88 dBm; shown via API and console.
- **FR-22 (Boot warmup):** *[now]* After every boot, automatic heating waits up to 300 s for zone sensors. The warmup
  ends early when all mapped sensors were received. Zones whose sensors were received control normally during the
  warmup. At the end, each zone still without reading enters FR-51 and raises DTC-101 `ERR_SENSOR_TIMEOUT`.
  Frost protection is active during the warmup.
- **FR-11.1 (No installation data in firmware):** *[now]* The firmware ships without sensor MAC addresses or zone
  names; they are set at commissioning and stored in NVS.
- **FR-11.3 (Zone mapping):** *[now]* Each zone maps to a name, one sensor (MAC), one relay output and a target;
  set via API (`/api/setup/...`) and console. `/api/setup/discovered-sensors` lists received sensors by RSSI.
- **FR-11.4 (Persistence):** *[now]* Mapping, names and settings are stored in NVS and survive reboot and power loss.
- ~~**FR-11.2 (Setup wizard page `/setup`):** retired, page removed; discovery kept in FR-11.3.~~
- **FR-72 (Additional BLE receivers):** *[nice to have]* Optional extra ESP32 receivers forward sensor adverts to the
  controller over the LAN; the controller uses the most recent accepted reading.
  - Interim: a sensor below −90 dBm for 1 h raises DTC-105 `WARN_SENSOR_WEAK_SIGNAL`.

### 3.5 Protections and fault detection

- **FR-16.1 (Frost protection):** *[now]* If any zone — enabled or disabled — with an accepted reading is below
  `frostTemp` (default 12 °C), that zone heats in any mode (AUTO, MANUAL, global OFF) until it reaches
  `frostTemp` + 1.5 °C. Interlock (FR-2) and burner guards (FR-4) still apply.
- **FR-16.2 (Sensor timeout):** *[now]* A zone with no accepted reading for longer than `sensorTimeout`
  (default 20 min) enters FAULT, raises DTC-101 `ERR_SENSOR_TIMEOUT` and continues with FR-51. Normal control
  resumes on the first accepted reading.
- **FR-18 (Sensor timeout setting):** *[now]* `sensorTimeout` is set via API and console, stored in NVS; one value
  is used for control and for DTC-101 `ERR_SENSOR_TIMEOUT`.
- **FR-40 (Window detection, simple rule):** *[now]* One global `windowThreshold` for all zones: a drop of at least
  the threshold within 5 min closes that zone's valve for 30 min, or until the temperature rebounds by 0.6 °C from
  the minimum, or until `POST /api/zones/window/clear`. Per-zone thresholds are removed.
  - Used whenever FR-40.2 lacks outdoor or humidity data.
  - *Fallback:* a false detection ends after 30 min at most; frost protection overrides the pause.
- **FR-40.2 (Smart window detection):** *[phase 4]* Each zone evaluates a sliding window of ≥ 3 accepted readings
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
- **FR-54 (Flow temperature limit):** *[now; sensors optional]* Optional flow and return temperature sensors at the
  manifold, enabled in configuration (`flowSensors`). If the flow exceeds `flowMax` for more than 2 min, the boiler
  is switched OFF without waiting for the burner minimum run (forced OFF, FR-17.3) until the flow is 5 °C below
  `flowMax`, then normal control resumes with all guards (no lockout). The 2 min filter prevents chatter.
  - *Reports:* DTC-205 `ERR_FLOW_OVER_LIMIT`; flow / return values in status.
  - *Fallback:* sensors absent or implausible → function disabled, DTC-206 `WARN_FLOW_SENSOR_FAULT`; protection
    relies on the boiler setpoint (§2 preconditions).
  - Sensor type is open (wired DS18B20 would need new libraries — owner approval per AGENTS.md).
- **FR-56 (Zone overheat):** *[phase 4]* If a zone is above target + `overheatMargin` and still rising for
  `overheatTime` with its valve commanded closed, DTC-203 `ERR_ZONE_OVERHEAT` is raised (valve stuck open or wrong
  NC/NO setting). Other zones continue normally.
- **FR-57 (Heating without effect):** *[phase 4]*
  - A zone with duty ≥ `noEffectDuty` for `noEffectTime` that rises less than `noEffectRise` raises
    DTC-202 `ERR_ZONE_NO_EFFECT`.
  - If all demanding zones show it, or the flow temperature (FR-54) does not rise within 15 min of boiler ON, the
    controller re-sends ON once; if there is still no effect after another 15 min, DTC-204 `ERR_BOILER_NOT_HEATING`
    is raised.
  - Other functions continue.

### 3.6 Schedule, modes and season

- **FR-29 (Levels):** *[now]* Each zone has three levels: day, night, away (5–30 °C; factory 23 / 21 / 18 °C).
- **FR-30 (Level storage):** *[now]* Level temperatures are stored in NVS, written only on change.
- **FR-31 (Schedule):** *[now]* One weekday and one weekend list of up to 8 slots `HH:MM=level`, kept sorted.
- **FR-44 (Away):** *[now]* Global away sets every zone's target to its away level and ends boosts; switching away off
  returns to the schedule.
- **FR-12.1 (Time):** *[now]* Time from NTP with the Romanian time zone (`EET-2EEST,M3.5.0/3,M10.5.0/4`), including
  daylight-saving changes.
- **FR-61 (No valid time):** *[now]* Without valid time, each zone keeps the last active schedule level (stored in NVS
  on level change) until NTP succeeds.
  - *Reports:* DTC-302 `WARN_TIME_NOT_SET`, 'time not set' in the page.
- **FR-64 (Global OFF):** *[now]* Global OFF mode: no heating except frost protection (FR-16.1), valve exercise
  (FR-19) and zones in sensor FAULT when it is cold outside (FR-51). Entered manually or by the season logic (FR-12.2).
- **FR-12.2 (Heating season):** *[now]* Season mode AUTO / ON / OFF; season OFF = global OFF (FR-64).
  - AUTO switches ON when the daily mean outdoor temperature is below `heatLimit` for `seasonOnDays` consecutive
    days, and OFF when it is above `heatLimit` + 2 °C for `seasonOffDays` consecutive days.
  - Safety net (season mode AUTO only): in season OFF, if an enabled zone with accepted readings stays below its
    night level for 12 h, the season switches ON. A season OFF chosen by hand is kept; frost protection covers
    safety.
  - *Fallback:* no weather data → season state unchanged. Every change is logged.
- ~~**FR-12.3 (Open-Meteo weather):** retired, merged into FR-9.~~
- ~~**FR-12.4 (City search, county list, GPS button):** retired, replaced by FR-9.2.~~

### 3.7 Weather and predictive control

- **FR-9 (Outdoor temperature):** *[now]* Outdoor temperature from Open-Meteo every 15 min; retry after 5 min on
  failure.
  - *Fallback:* last value is used for 6 h, then functions that need it use their own fallback;
    DTC-401 `WARN_WIFI_SIGNAL_DEGRADED` covers a network loss.
- **FR-9.2 (Location):** *[now]* Weather location as latitude, longitude stored in NVS; set via API and console.
- **FR-67 (Learned heat-up model):** *[phase 4]* Per zone, the heat-up rate (°C/h as a function of outdoor
  temperature) is learned from operation, stored in NVS, and used for the pre-heat lead instead of the FR-48 formula.
  - *Fallback:* too little data or values out of bounds → FR-48 formula (NFR-5.3).
- **FR-63 (Aligned cycles, boiler minimum load):** *[phase 4]* TPI cycles of all zones start aligned, so open valves
  overlap. A zone below `minLoops` circuits demanding alone is extended or grouped with another zone, so the boiler
  does not short-cycle on its own thermostat.
- **FR-70 (Solar gain throttling):** *[phase 4]* For zones flagged `solarGain`, if the Open-Meteo irradiance forecast
  for the next 3 h exceeds `solarLimit`, the TPI duty is reduced by `solarReduction`. Values are set from recorded data.
  - *Fallback:* no forecast → no reduction.
- **FR-71 (Humidity compensation):** *[nice to have]* Per-zone opt-in (`humidityComp`): at relative humidity ≥ 60 % the control target
  is lowered by up to 0.5 °C; the effective target is shown in the admin view.
  - *Fallback:* no humidity reading → no compensation.

### 3.8 Diagnostics

- **FR-10.2 (Diagnostic codes):** *[now]* The controller raises the codes listed in §5 and shows active codes in the
  page and `/api/status`.
- **FR-10.4 (Basic self-test):** *[now]* `/api/diag/selftest` reports Wi-Fi, heap, BLE, zones, radio and a health
  score 0–100 %, without switching any output. The full self-test is FR-60.
- **FR-62 (Event history and alarms):** *[now]* The last 50 safety events (DTC raised / cleared, frost, FAULT,
  watchdog reset, controlled restart) are kept across reboots with time stamp (uptime if time is not set), written
  only when an event occurs. Alarms are shown in the page and `/api/status`; there is no push channel.
  - *Fallback:* event store cannot be written → events are kept in RAM until reboot, with a log line.
- **FR-21 (Firmware identity):** *[now]* Version, build date and time, git commit and reset reason are reported in
  status, console `STATUS` and the boot message.
- **FR-23 (Console status):** *[now]* `STATUS` shows zones, sensors, boiler, protections, heap (free, minimum, largest
  block) and OTA margin. Layout is not specified.
- ~~**FR-24 (Hardware telemetry in STATUS):** retired, merged into FR-23.~~
- ~~**FR-25 (Zone cards, UTF-8 degree sign):** retired.~~
- ~~**FR-26 (Web console colouring):** retired, page removed.~~
- ~~**FR-27 (STATUS table layout, `utf8VisibleLen`):** retired.~~
- ~~**FR-10.1 (Cloud connection):** retired, out of scope.~~
- ~~**FR-10.3 (Service mode):** retired, removed in phase 2.~~
- ~~**FR-10.5 (Fleet OTA):** retired, out of scope.~~

### 3.9 Interfaces and security

- **FR-7.1 (Telnet console):** *[now]* Telnet server on port 23.
- **FR-7.3 (Console commands):** *[now]* Console commands are accepted via Serial, Telnet and HTTP (FR-45); `HELP`
  lists them.
- **FR-45 (Console over HTTP):** *[now]* `POST /api/console/send` executes a console command and returns the new log
  lines; `CLEAR` empties the log buffer.
- ~~**FR-7.2 (Web console page `/console`):** retired, page removed.~~
- **FR-8.1 (Web page):** *[now]* One page served at `/`:
  - family view: rooms (temperature, target, humidity, state), outdoor weather, boost, one global Home / Away switch,
    active alarms;
  - admin view (`#admin`): duty per zone, RSSI per sensor, pre-heat lead, active DTCs, configuration.
  - Family view language configurable (Romanian / English, NFR-3.1).
- ~~**FR-8.2 (Remote access via Tailscale):** retired, user network setup, not product behaviour.~~
- **FR-8.3 (Firmware update):** *[now]* Firmware update via `/update` with authentication (FR-58);
  `tools/sh_ota_upload.py` completes a 1.25 MB image in < 30 s on the home LAN. OTA rules: FR-60.
- **FR-15.1 (Actuator polarity):** *[now]* NC or NO actuator type is configurable (API, console), stored in NVS; all
  valve commands respect it.
- ~~**FR-15.2 (Compatibility statement):** retired, merged into FR-15.1.~~
- **FR-34 (Telemetry history):** *[now]* History in LittleFS: 24 h at 5 min and 5 days at 30 min resolution, served by
  `GET /api/history?range=24h|5d`.
- **FR-35 (Burner counters):** *[now]* Burner time counters: total, today, yesterday, this month, last month,
  current burn; reported in boiler status.
- **FR-41 (Wi-Fi supervisor):** *[now]* Wi-Fi is checked every 10 s and reconnected; after 90 s down the stack is
  re-initialised. DTC-401 `WARN_WIFI_SIGNAL_DEGRADED` while down or RSSI < −82 dBm. Control continues (NFR-1.1).
- **FR-42 (Configuration API limits):** *[now]* `/api/config/system` exposes hydraulic and radio timings and rejects
  values outside the §6 ranges, in particular below the safety floors of FR-2, FR-3 and FR-4.
- **FR-43 (Relay test):** *[now]* Relay test is accepted only in MANUAL mode and only for configured relay outputs,
  respecting NC/NO; otherwise HTTP 409.
- **FR-50 (Setup access point):** *[now]* If the configured Wi-Fi is not joined at boot, the controller opens access
  point `SmartHeating-Setup`; control continues.
- **FR-58 (Authentication):** *[now]* These actions require authentication (same credentials as OTA):
  - changing a safety parameter of §6: `interlockDelay`, `overrun`, `burnerMinRun`, `burnerMinPause`,
    `openLoopBypass`, `sensorTimeout`, `faultDuty`, `faultFrostOutdoor`, `frostTemp`, `hbInterval`,
    `boilerLinkHeartbeat`, `boilerLinkType`, `flowSensors`, `flowMax`, `actuatorType`, `pumpOutput`, zone outputs;
  - boiler commands, relay test, MANUAL mode, fault injection (NFR-5.5), credentials.
  - Comfort settings (levels, schedule, boost, away, `windowThreshold`) need no authentication.
  - CORS is limited to the controller's own origin.
  - *Fallback:* unauthenticated request → HTTP 401, no change.
- **FR-59 (Credentials):** *[now]* The first flash may use universal default credentials (OTA, access point, Wi-Fi).
  Credentials are changed via API or console without re-flashing and stored in NVS.
  - *Reports:* while any default is active, DTC-403 `WARN_DEFAULT_CREDENTIALS`; everything else works.

### 3.10 Maintenance and commissioning

- **FR-19 (Valve exercise):** *[now]* A valve closed for ≥ `exerciseIdleDays` is opened for 5 min with the boiler
  OFF; the optional pump runs during the exercise. Idle time is tracked per valve on epoch time and stored in NVS
  (on change only). Runs in every mode, including global OFF.
- **FR-60 (OTA safety and self-test):** *[now]*
  - OTA starts only with the boiler OFF.
  - After boot of a new image, automatic checks without the boiler (loop and watchdog alive, Wi-Fi up, sensors
    received within 60 s); the image is marked valid only if they pass, otherwise the previous image is restored
    and DTC-303 `ERR_OTA_ROLLBACK` is raised.
  - Full self-test on request only (API, console): each zone relay switched, boiler link frame sent, one zone
    opened (interlock respected), boiler ON for `selfTestBurn`, flow rise checked if FR-54 sensors are enabled,
    boiler OFF with all guards. Result stored and shown. Failures raise DTC-304 `ERR_SELFTEST_FAILED`; a plant
    failure never causes a rollback. The boiler step is skipped in global OFF.
- **FR-65 (Configuration export / import):** *[later]* The complete configuration (zones, sensors, offsets, learned
  values, credentials excluded) is exported and imported as JSON via API.
- **FR-66 (Factory reset):** *[later]* Factory reset via console or API, with confirmation; keeps Wi-Fi credentials.
- **FR-68 (Commissioning checklist):** *[now, documentation]* Commissioning follows a checklist; the result is stored:
  1. installation — manifold flow meters balanced per zone; always-open bypass present; boiler flow setpoint
     ≤ 45 °C (§2);
  2. sensors — PVVX / BTHome firmware, advert interval ≤ 10 s; all sensors 24 h side by side next to a reference,
     offsets set to ±0.2 °C (FR-49); mounted 1.2–1.5 m high on an internal wall, away from floor radiation, sun,
     doors and drafts;
  3. function — relay test per zone (FR-43), valve opening time measured, boiler ON/OFF confirmed by the full
     self-test (FR-60), radio range (FR-5.2).
- **FR-69 (Number of zones):** *[now: configuration; hardware later]* 1–32 zones, each with name, sensor and output.
  The number of outputs is limited by the configured hardware; a zone without an output is rejected. Memory for
  32 zones is reserved statically (NFR-2.6). Relay 4 is a zone or the pump.
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

- **NFR-1.1 (Local autonomy):** Loss of Wi-Fi, router or internet does not stop heating control; all decisions run on
  the controller.
- **NFR-1.4 (Safe boot):** At every boot the controller drives all zone valves to closed (per FR-15.1), sends one
  boiler OFF, and enters the boot warmup (FR-22) before any heating command.
- **NFR-1.5 (Task watchdog):** A 90 s task watchdog on `loop()` resets the controller on a software hang.
- ~~**NFR-1.2 (Sensor failsafe):** retired, merged into FR-16.2.~~
- ~~**NFR-1.3 (Frost protection):** retired, merged into FR-16.1.~~
- ~~**NFR-1.6 (Burner cycle limit 60 s + 60 s):** retired, merged into FR-4.~~
- ~~**NFR-1.7 (Failure-mode description):** retired, moved to the hydraulics document.~~

### 4.2 Resources and performance

- **NFR-2.2 (Free heap):** Free heap ≥ 100 KB during normal operation. Below 60 KB
  DTC-402 `WARN_HEAP_FRAGMENTATION` is raised; below 30 KB a controlled restart is performed while the boiler is OFF,
  with state restored from NVS (FR-62 records it).
- **NFR-2.3 (API response):** Local API response time < 100 ms.
- **NFR-2.4 (Log time stamps):** Every console line carries a `[HH:MM:SS]` time stamp.
- **NFR-2.6 (No heap drift):** No heap decrease > 5 KB over 24 h of operation; buffers are bounded and reserved
  statically.
- **NFR-2.8 (Firmware size):** Firmware image < 1.25 MB, so OTA fits.
- **NFR-2.9 (Flash wear):** NVS is written only when a value changes.
- ~~**NFR-2.1 (Power < 1.2 W):** retired.~~
- ~~**NFR-2.5 (Paced OTA upload):** retired, merged into FR-8.3.~~
- ~~**NFR-2.7 (Development server):** retired, tooling, documented in the README.~~

### 4.3 Code and language

- **NFR-3.1 (Language):** Code, identifiers, API messages, logs and documentation in English. The family view of the
  page is available in Romanian and English (configurable).
- **NFR-3.2 (Modules):** Firmware is split into the modules of [`architecture.md`](architecture.md), one
  responsibility each.

### 4.4 Comfort

- **NFR-4.1 (Comfort accuracy):** In steady state (no schedule change for 3 h, window closed) the room temperature
  stays within ±0.3 °C of the target.

### 4.5 Graceful degradation

- **NFR-5.1 (Priorities):** When goals conflict: safety > comfort > economy. A fallback may give up economy, never
  safety, and gives up comfort only when safety requires it. Faults are logged and raised as DTC; occupants shall
  not notice them.
- **NFR-5.2 (Fallback clause):** Every functional requirement states its inputs, its fallback when an input is
  missing or implausible, and the DTC or log entry raised. A requirement without a fallback is incomplete.
- **NFR-5.3 (Bounded learning):** Learned values (TPI base duty, heat-up model, window and solar parameters) are
  bounded; a learned value outside its bounds or a corrupt NVS record is discarded, the default is used and
  DTC-301 `WARN_LEARNING_RESET` is raised. Learning never stops control.
- **NFR-5.4 (Short interruptions):** Heating interruptions shorter than 60 min are acceptable as fallback (the screed
  buffers them, room drop < 0.3 °C). Longer fallbacks keep heating at an estimated duty (FR-51).
- **NFR-5.5 (Fault injection):** Every fallback can be triggered on the board for test via console / API (sensor
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
| `boilerLinkType` | `rf433` | `rf433` / `wired` / `receiver` (ESP32-class, link open) | FR-55 |
| `seasonMode` | AUTO | AUTO / ON / OFF | FR-12.2 |
| `location` | not set (commissioning) | latitude −90…90, longitude −180…180 | FR-9.2 |
| `pageLanguage` | `ro` | `ro` / `en` | FR-8.1, NFR-3.1 |
| credentials (OTA, access point, Wi-Fi) | universal default | 8–63 characters | FR-59 |
| number of zones | 3 | 1–32 | FR-69 |
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
