# Smart Heating ESP32 - RESTful API Reference Specification

> **Version:** 1.2.0  
> **Target IP:** `http://192.168.1.20:80` (or dynamic mDNS / DHCP lease)  
> **Response Format:** `application/json; charset=utf-8`  
> **Standard:** ASPICE SWE.5 Compliant / Headless REST First  
> **Target Frontend:** Samsung Galaxy Tab A9 Kiosk Display, Mobile Web Dashboard, Home Assistant / Automation Scripts

---

## 1. Architectural Overview & Design Principles

The `smart-heating` ESP32 controller operates on an **API-First, Headless Architecture**:
1. **Separation of Concerns:** Real-time hydraulic safety, actuator timing (interlock delays, anti-short-cycling, freeze guard), and BLE scanning reside strictly in the C++ firmware core.
2. **Sub-100ms Response:** JSON endpoints return pre-formatted or minimal-overhead payloads to ensure snappy client rendering.
3. **CORS Enabled:** All endpoints include `Access-Control-Allow-Origin: *` to support local development, web apps, and kiosk wrappers.
4. **Error Handling:** Semantic HTTP status codes:
   - `200 OK`: Request succeeded.
   - `400 Bad Request`: Missing or invalid parameters.
   - `429 Too Many Requests`: Rate-limiting / anti-short-cycling active (includes `waitSec` remaining).
   - `500 Internal Error`: Internal fault.

---

## 2. API Endpoints Directory

| Category | Endpoint | Method | Description |
|---|---|:---:|---|
| **Boiler & Mode** | `/api/boiler/status` | `GET` | Core system telemetry, burner flame state, pump state, uptime, runtime |
| | `/api/boiler/on` | `POST` | Trigger burner flame START via 433.92 MHz RF (safe guard active) |
| | `/api/boiler/off` | `POST` | Trigger burner flame STOP via 433.92 MHz RF (idempotent) |
| | `/api/boiler/cancel` | `POST` | Cancel pending deferred startup or shutdown action (`PENDING_ON`/`OFF`) |
| | `/api/boiler/reset-runtime`| `POST` | Reset boiler burner runtime tracking counters to 0 |
| | `/api/mode` | `POST` | Toggle AUTOMATIC (thermostat control) vs MANUAL mode |
| **Thermal Zones** | `/api/zones` | `GET` | All underfloor heating zones, temperatures, setpoints, valve states |
| | `/api/zones/setpoint` | `POST` | Calibrate target temperature setpoint for a specific zone |
| | `/api/zones/toggle` | `POST` | Enable or disable heating loop for a zone |
| | `/api/zones/sensor` | `POST` | Assign BLE thermometer MAC / name to a zone |
| | `/api/zones/hold/clear` | `POST` | Clear temporary circadian hold for single zone or all (`id=X\|all`) |
| | `/api/zones/window/clear`| `POST` | Clear window pause guard & reset baseline for zone or all (`id=X\|all`) |
| | `/api/zones/window/threshold`| `POST` | Set window drop sensitivity threshold (0.8°C to 2.0°C) per zone |
| **Telemetry History**| `/api/history` | `GET` | LittleFS flash telemetry history buffer (24h @ 5m & 7d @ 30m) |
| **State Events History**| `/api/history/events` | `GET` | Second-accurate edge-triggered start/stop runtime event log & daily sums |
| **LittleFS Asset Upload**| `/api/upload-file` | `POST` | Multipart upload of UI assets directly to LittleFS (no reboot needed) |
| **Bio-Schedule** | `/api/schedule` | `GET` | 4 Circadian anchors (`Sleep`, `Wake`, `Away`, `Home`) & profiles |
| | `/api/schedule` | `POST` | Update circadian anchor times & target temperatures |
| | `/api/schedule/enable` | `POST` | Toggle Bio-Schedule, Predictive Pre-Heat, and Window Guard |
| | `/api/schedule/anchor` | `POST` | Update individual anchor time and target temperatures |
| | `/api/schedule/presets` | `GET` | List factory presets (`default`, `eco`, `wfh`) and saved user presets |
| | `/api/schedule/preset/apply` | `POST` | Apply schedule preset by ID (`default`, `eco`, `wfh`, `custom1`, `custom2`) |
| | `/api/schedule/preset/save` | `POST` | Save current schedule to NVS preset slot (`custom1`, `custom2`) |
| | `/api/schedule/reset` | `POST` | Reset all anchors and setpoints to factory default preset |
| **BLE Sensors** | `/api/ble/sensors` | `GET` | List all Xiaomi BLE thermometers decoded via NimBLE passive scan |
| **Diagnostics** | `/api/diag/status` | `GET` | Overall Health Score (0-100%), Service Mode, Active DTC list |
| | `/api/diag/selftest` | `GET` | Trigger comprehensive on-board test of all 6 hardware subsystems |
| | `/api/diag/clear` | `POST` | Clear diagnostic trouble codes (DTCs) and restore 100% health |
| | `/api/diag/service` | `POST` | Toggle Engineer Service Mode (bypasses RF guards for maintenance) |
| | `/api/diag/exercise` | `POST` | Trigger 20-second anti-seize exercise cycle for valves and pump |
| **Setup & Hardware** | `/api/setup/passport` | `GET` | Hardware Device Passport, Serial, MAC, Commissioning date |
| | `/api/setup/time` | `GET` | Local calendar, NTP synchronization status, Outdoor weather |
| | `/api/setup/actuator-type` | `GET/POST`| Actuator electrical polarity: NC (Normally Closed) vs NO |
| | `/api/setup/pump` | `GET/POST`| Secondary GPA circulation pump configuration on Relay 4 |
| | `/api/setup/sensor-timeout`| `GET/POST`| Sensor failsafe timeout window (seconds before fallback) |
| | `/api/setup/discovered-sensors`| `GET`| Discovered BLE sensors formatted for Setup Wizard mapping |
| | `/api/setup/map-zone` | `POST` | Map discovered sensor to a room zone and store in NVS |

---

## 3. Detailed Endpoint Contracts

### 3.1. Boiler & Operation Mode

#### `GET /api/boiler/status`
Returns high-level system telemetry and heating status.

- **Response `200 OK`:**
```json
{
  "firmwareVersion": "v1.10.27.4",
  "buildDate": "Oct 27 2026 11:19:34",
  "gitCommit": "fb0d4e6",
  "otaFreeBytes": 22512,
  "otaSlotBytes": 1310720,
  "awayActive": false,
  "awayUntil": 0,
  "resetReason": "Software Reboot (Firmware Update / Restart)",
  "boiler": "OFF",
  "boilerCooldownSec": 0,
  "pendingAction": "NONE",
  "pendingWaitSec": 0,
  "lastTxMs": 552,
  "freeHeap": 88228,
  "minFreeHeap": 73932,
  "cpuFreqMHz": 240,
  "fsTotalBytes": 1441792,
  "fsUsedBytes": 90112,
  "rssi": -58,
  "uptimeSec": 450,
  "healthScore": 100,
  "serviceMode": false,
  "serialNumber": "ESP32-HEAT-D4D4DA5CD880",
  "isCommissioned": true,
  "time": "01:28:30",
  "date": "2026-10-18",
  "season": "Autumn",
  "isHeatingSeason": true,
  "outdoorTemp": 8.7,
  "bleScanning": true,
  "autoMode": true,
  "boilerDemand": false,
  "actuatorType": "NC",
  "pumpEnabled": true,
  "pumpActive": false,
  "frostGuardActive": false,
  "sensorTimeoutSec": 600,
  "lastAntiSeizeEpoch": 1791958044,
  "scheduleEnabled": true,
  "activePreset": "default",
  "activeAnchor": "SLEEP",
  "nextAnchor": "WAKE",
  "minsToNextAnchor": 302,
  "preHeatActive": false,
  "boilerRuntime": {
    "totalSec": 1273,
    "todaySec": 766,
    "yesterdaySec": 120,
    "thisMonthSec": 1273,
    "lastMonthSec": 0,
    "currentBurnSec": 0
  },
  "sensors": [
    {
      "name": "Living Room",
      "mac": "A4:C1:38:A4:C1:DD",
      "temp": 24.1,
      "hum": 59.0,
      "bat": 90,
      "rssi": -65,
      "ageSec": 14
    }
  ]
}
```

#### `POST /api/boiler/on`
Triggers gas boiler burner ignition via 433.92 MHz radio signal.

- **Query Parameters:**
  - `force` (optional, boolean): Set to `true` to override anti-short-cycling guard (restricted to Service Mode).
- **Response `200 OK` (Ignition Triggered or Idempotent):**
```json
{
  "status": "ok",
  "boiler": "ON"
}
```
- **Response `429 Too Many Requests` (Anti-Short-Cycling Protected & Deferred Startup Queued):**
```json
{
  "status": "pending",
  "error": "Anti-short-cycling guard active",
  "target": "ON",
  "waitSec": 45,
  "pending": true,
  "boiler": "OFF",
  "message": "Pornire programata automat dupa expirarea timpului minim de pauza (45s)"
}
```

#### `POST /api/boiler/off`
Turns off gas boiler flame. Always succeeds immediately (failsafe stop is never rate-limited). If an ignition was pending, it is cancelled immediately.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "boiler": "OFF"
}
```

#### `POST /api/boiler/cancel`
Cancels any scheduled deferred boiler action (`PENDING_ON` or `PENDING_OFF`) without modifying the active physical state of the burner.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "pendingCancelled": true,
  "boiler": "OFF"
}
```

#### `POST /api/boiler/reset-runtime`
Resets the non-volatile cumulative burner runtime counters (`totalSec`, `todaySec`, `thisMonthSec`, etc.) to zero.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "message": "Boiler runtime counters reset to 0"
}
```

#### `POST /api/mode`
Switches operating mode.

- **Query Parameters / Form Data:**
  - `mode`: `"auto"` or `"manual"` (or `auto=true|false`).
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "autoMode": true
}
```

---

### 3.2. Thermal Zones Management

#### `GET /api/zones`
Returns the status of all hydronic heating loops.

- **Response `200 OK`:**
```json
{
  "autoMode": true,
  "boiler": "OFF",
  "boilerCooldownSec": 0,
  "pendingAction": "NONE",
  "pendingWaitSec": 0,
  "boilerDemand": false,
  "actuatorType": "NC",
  "pumpEnabled": true,
  "pumpActive": false,
  "frostGuardActive": false,
  "sensorTimeoutSec": 600,
  "lastAntiSeizeEpoch": 1791958044,
  "scheduleEnabled": true,
  "activePreset": "default",
  "activeAnchor": "SLEEP",
  "nextAnchor": "WAKE",
  "minsToNextAnchor": 302,
  "preHeatActive": false,
  "zones": [
    {
      "id": 1,
      "name": "Living & Bucatarie",
      "sensor": "Living Room",
      "sensorMac": "A4:C1:38:A4:C1:DD",
      "relayPin": 26,
      "targetTemp": 22.0,
      "effectiveTarget": 22.0,
      "hysteresis": 0.30,
      "enabled": true,
      "currentTemp": 23.6,
      "sensorValid": true,
      "sensorFault": false,
      "frostActive": false,
      "windowPauseActive": false,
      "windowPauseRemainingSec": 0,
      "windowDropThreshold": 1.2,
      "lastWindowEpoch": 0,
      "lastWindowDurationSec": 0,
      "demand": false,
      "relayEngaged": false,
      "openSec": 0,
      "isManualHold": false,
      "holdAnchor": "",
      "state": "Idle (Temp OK)"
    }
  ]
}
```

#### `POST /api/zones/setpoint`
Adjusts target temperature for a specific zone. If Bio-Schedule is enabled, this engages a non-destructive temporary hold (`isManualHold: true`) expiring on the next circadian anchor transition.

- **Query Parameters / Form Data:**
  - `id`: Zone index (`0`, `1`, `2`) or 1-based ID (`1`, `2`, `3`).
  - `target` or `temp`: Target temperature in Celsius (`15.0` to `30.0`).
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "zoneId": 1,
  "targetTemp": 22.5
}
```

#### `POST /api/zones/hold/clear`
Releases the temporary setpoint hold on a specific zone or across all zones simultaneously, restoring the active circadian anchor's baseline temperature immediately.

- **Query Parameters:**
  - `id`: Zone ID (`0`, `1`, `2` or `1`, `2`, `3`) or `all` to clear all zones.
- **Response `200 OK`:**
```json
{
  "status": "ok"
}
```

#### `POST /api/zones/window/clear`
Manually exits the open window pause guard, resets the zone's thermal baseline to the current room temperature, and immediately re-evaluates the hydronic loop to resume heating if demanded.

- **Query Parameters:**
  - `id`: Zone ID (`0`, `1`, `2` or `1`, `2`, `3`) or `all` to clear all zones.
- **Response `200 OK` (Cleared or Already Inactive):**
```json
{
  "status": "ok",
  "message": "Window pause cleared for zone"
}
```

#### `POST /api/zones/window/threshold`
Calibrates the open window temperature drop sensitivity threshold for a specific zone. The value is validated and clamped strictly between `0.8°C` and `2.0°C` (default `1.2°C`) and saved persistently to NVS flash.

- **Query Parameters:**
  - `id`: Zone ID (`0`, `1`, `2` or `1`, `2`, `3`).
  - `threshold`: Drop sensitivity in Celsius (`0.8` to `2.0`).
- **Response `200 OK`:**
```json
{
  "status": "ok"
}
```
- **Response `400 Bad Request` (Out of Bounds):**
```json
{
  "error": "Threshold out of bounds (allowed: 0.8 to 2.0 C)"
}
```

---

### 3.3. Smart Bio-Schedule & Circadian Anchors

#### `GET /api/schedule`
Retrieves configured natural lifestyle anchors, per-zone setpoints, active circadian phase, and currently active preset.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "enabled": true,
  "preHeatEnabled": true,
  "windowDetectionEnabled": true,
  "activePreset": "default",
  "activeAnchor": "SLEEP",
  "nextAnchor": "WAKE",
  "minutesToNext": 360,
  "isPreHeatActive": false,
  "isWeekend": false,
  "profile": "Weekday",
  "weekday": [
    { "name": "WAKE",  "time": "06:30", "temp": 22.0, "z1": 22.0, "z2": 22.0, "z3": 21.0 },
    { "name": "AWAY",  "time": "08:30", "temp": 19.5, "z1": 19.5, "z2": 19.5, "z3": 19.0 },
    { "name": "HOME",  "time": "17:00", "temp": 21.5, "z1": 21.5, "z2": 21.5, "z3": 20.5 },
    { "name": "SLEEP", "time": "23:00", "temp": 20.0, "z1": 20.0, "z2": 20.0, "z3": 19.5 }
  ],
  "weekend": [
    { "name": "WAKE",  "time": "08:00", "temp": 22.5, "z1": 22.5, "z2": 22.5, "z3": 21.5 },
    { "name": "AWAY",  "time": "11:00", "temp": 20.5, "z1": 20.5, "z2": 20.5, "z3": 20.0 },
    { "name": "HOME",  "time": "15:00", "temp": 22.0, "z1": 22.0, "z2": 22.0, "z3": 21.0 },
    { "name": "SLEEP", "time": "23:30", "temp": 20.5, "z1": 20.5, "z2": 20.5, "z3": 20.0 }
  ]
}
```

#### `POST /api/schedule`
Updates bio-schedule toggles or applies full configuration. Supports JSON body or URL-encoded form parameters.

- **Query / Form Parameters:**
  - `enabled`: `true` | `false`
  - `preheat`: `true` | `false` (optional)
  - `window`: `true` | `false` (optional)
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "enabled": true,
  "preHeatEnabled": true,
  "windowDetectionEnabled": true
}
```

#### `POST /api/schedule/enable`
Quick toggle for scheduling subsystems.

- **Query Parameters:**
  - `enabled`: `true` | `false`
  - `preheat`: `true` | `false` (optional)
  - `window`: `true` | `false` (optional)
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "enabled": true,
  "preHeatEnabled": true,
  "windowDetectionEnabled": true
}
```

#### `POST /api/schedule/anchor`
Customizes a single anchor time and/or per-zone target temperatures. Supports JSON body or query/form parameters. Persists immediately to NVS with flash-wear protection.

- **Query / Form Parameters:**
  - `profile`: `weekday` | `weekend` (required)
  - `anchor`: `wake` | `away` | `home` | `sleep` (required)
  - `time`: `HH:MM` (e.g. `06:45`, optional)
  - `temp`: Float target temperature (applied to all zones, optional)
  - `z1`: Zone 1 target temperature (°C, optional)
  - `z2`: Zone 2 target temperature (°C, optional)
  - `z3`: Zone 3 target temperature (°C, optional)
- **Example JSON Body:**
```json
{
  "profile": "weekday",
  "anchor": "wake",
  "time": "06:15",
  "z1": 22.5,
  "z2": 22.0,
  "z3": 21.0
}
```
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "profile": "weekday",
  "anchor": "wake",
  "time": "06:15",
  "z1": 22.5,
  "z2": 22.0,
  "z3": 21.0
}
```

#### `GET /api/schedule/presets`
Returns all available presets (factory defaults + saved user presets in NVS) and indicates the currently active preset.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "activePreset": "default",
  "presets": [
    { "id": "default", "name": "Default (Balanced)", "type": "factory", "active": true },
    { "id": "eco", "name": "Eco / Away (18.0°C)", "type": "factory", "active": false },
    { "id": "wfh", "name": "Work From Home (22.5°C)", "type": "factory", "active": false },
    { "id": "custom1", "name": "My Custom Schedule", "type": "user", "active": false, "saved": true },
    { "id": "custom2", "name": "Custom Slot 2", "type": "user", "active": false, "saved": false }
  ]
}
```

#### `POST /api/schedule/preset/apply`
Applies a preset to the active bio-schedule. Can apply factory presets (`default`, `eco`, `wfh`) or user-saved presets (`custom1`, `custom2`).

- **Query / Form Parameters:**
  - `id`: Preset identifier (`default`, `eco`, `wfh`, `custom1`, `custom2`)
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "activePreset": "eco"
}
```

#### `POST /api/schedule/preset/save`
Captures the current active schedule configuration into an NVS user preset slot (`custom1` or `custom2`) with a custom friendly label.

- **Query / Form Parameters:**
  - `id`: Preset slot identifier (`custom1` | `custom2`, required)
  - `name`: Human-readable label (max 31 chars, e.g. `Winter Vacation`, optional)
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "id": "custom1",
  "name": "Winter Vacation"
}
```

#### `POST /api/schedule/reset`
Resets all weekday and weekend circadian anchor times and zone setpoints back to factory defaults (`default`). Restores active preset tag to `"default"`.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "message": "Schedule reset to factory default"
}
```

---

### 3.4. Diagnostics, Self-Test & DTC Engine

#### `GET /api/diag/status`
Returns system health score and active Trouble Codes.

- **Response `200 OK`:**
```json
{
  "serviceMode": false,
  "healthScore": 100,
  "dtcCount": 0,
  "dtcs": []
}
```
*If faults exist (e.g. sensor timeout or rapid spam attempts):*
```json
{
  "serviceMode": false,
  "healthScore": 90,
  "dtcCount": 1,
  "dtcs": [
    {
      "code": "DTC-201",
      "severity": "WARNING",
      "message": "Rapid command spam throttled",
      "occurrenceCount": 4,
      "ageSec": 12
    }
  ]
}
```

#### `GET /api/diag/selftest`
Performs an on-demand hardware diagnostic self-test of all subsystems.

- **Response `200 OK`:**
```json
{
  "overall": "PASS",
  "tests": [
    { "subsystem": "WiFi",     "status": "PASS", "details": "Connected (RSSI: -58 dBm)" },
    { "subsystem": "Heap",     "status": "PASS", "details": "124 KB free" },
    { "subsystem": "BLE",      "status": "PASS", "details": "Listening (2 sensors heard)" },
    { "subsystem": "Pump",     "status": "PASS", "details": "Relay 4 Configured" },
    { "subsystem": "Zones",    "status": "PASS", "details": "3/3 Zones Valid" },
    { "subsystem": "BoilerRF", "status": "PASS", "details": "GPIO19 Driver Ready" }
  ]
}
```

#### `POST /api/diag/clear`
Clears all active DTC records and restores Health Score to 100%.

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "message": "All diagnostic trouble codes cleared"
}
```

---

### 3.5. Setup, Passport & Device Metadata

#### `GET /api/setup/passport`
Returns immutable hardware identity and commissioning certificate.

- **Response `200 OK`:**
```json
{
  "serialNumber": "SH-14721A0D5020",
  "isCommissioned": true,
  "commissionTimestamp": 1791537315,
  "commissionDate": "2026-10-09 10:15:15",
  "customerName": "Bogdan Seceleanu",
  "installerNotes": "4-Zone Underfloor Hydronic Relay Setup",
  "hardwareModel": "ESP32-Relay X4 V1.1",
  "macAddress": "20:50:0D:1A:72:14"
}
```

#### `GET /api/setup/time`
Returns synchronized local time and outdoor weather telemetry.

- **Response `200 OK`:**
```json
{
  "timeSynced": true,
  "time": "23:59:33",
  "date": "2026-10-13",
  "year": 2026,
  "month": 10,
  "day": 13,
  "dayOfWeek": "Tuesday",
  "season": "Autumn",
  "outdoorTemp": 11.4,
  "outdoorHum": 78.0,
  "weatherValid": true,
  "weatherAgeSec": 180
}
```

---

### 3.6. Flash Telemetry History Buffer (LittleFS)

#### `GET /api/history`
Streams historical temperature and actuation states persisted in non-volatile LittleFS flash memory. Data is organized into compact binary records (10 bytes per point) and served in a structured JSON payload with chunked streaming to prevent RAM exhaustion.

- **Query Parameters:**
  - `range`: `"24h"` (default: 5-minute sampling interval, up to 288 points) or `"5d"` (30-minute downsampled interval, up to 240 points).
- **Response `200 OK`:**
```json
{
  "range": "24h",
  "stepSec": 300,
  "currentSlot": 18,
  "points": [
    [15, null, null, null, null, 128],
    [16, 23.7, 23.6, 23.5, 8.7, 128],
    [17, 23.6, 23.6, 23.4, 8.7, 128],
    [18, 23.6, 23.6, 23.4, 8.7, 128]
  ]
}
```
- **Tuple Structure per Point:** `[slotIndex, z1Temp, z2Temp, z3Temp, outdoorTemp, flagsBitmask]`
  - `slotIndex` (integer): Ring buffer slot position (0..287 for 24h, 0..239 for 5d).
  - `z1Temp`, `z2Temp`, `z3Temp` (float | null): Hydronic zone temperatures in °C (or `null` if uninitialized).
  - `outdoorTemp` (float | null): Open-Meteo external temperature in °C.
  - `flagsBitmask` (uint8): Binary bitmask of actuation status:
    * Bit 0 (`0x01`): Gas Boiler Burner Flame Active
    * Bit 1 (`0x02`): Secondary GPA Pump Active
    * Bit 2 (`0x04`): Zone 1 Actuator Relay Engaged
    * Bit 3 (`0x08`): Zone 2 Actuator Relay Engaged
    * Bit 4 (`0x10`): Zone 3 Actuator Relay Engaged
    * Bit 7 (`0x80`): Record Valid / Initialized

---

#### `GET /api/history/events`
Returns second-accurate state transition logs (edge-triggered) for the boiler, secondary pump, and all 3 thermal zone relays, plus today's runtime aggregates. Persisted in LittleFS (`/events.bin`) with a 2,048-event circular buffer.

- **Query Parameters:**
  - `limit`: Maximum number of recent events to return (default: `100`, max: `2048`).
- **Response `200 OK`:**
```json
{
  "events": [
    {
      "epoch": 1793090542,
      "device": 0,
      "devName": "Boiler",
      "state": 1,
      "stateStr": "START",
      "durationSec": 0
    },
    {
      "epoch": 1793090722,
      "device": 0,
      "devName": "Boiler",
      "state": 0,
      "stateStr": "STOP",
      "durationSec": 180
    }
  ],
  "todayRuntime": {
    "boilerSec": 766,
    "livingSec": 1200,
    "copilSec": 1850,
    "matrimonialSec": 900,
    "pumpSec": 820
  }
}
```
- **Device Index Map:**
  - `0`: Boiler (RF Burner)
  - `1`: Living & Bucătărie (Zone 1)
  - `2`: Camera Copilului (Zone 2)
  - `3`: Dormitor Matrimonial (Zone 3)
  - `4`: Secondary Circulation Pump (GPA)

---

#### `POST /api/upload-file`
Multipart upload endpoint that writes files directly into the ESP32 LittleFS flash filesystem. Used by `tools/upload_ui.py` to update web pages (`/index.html.gz`, `/setup.html.gz`, `/console.html.gz`) in under 1 second without recompiling C++ or rebooting the ESP32.

- **Payload:** `multipart/form-data` with `file` field.
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "message": "Upload complete"
}
```

---

## 4. Frontend Integration Best Practices (Kiosk UI)

1. **Polling Intervals:**
   - Background Telemetry (`GET /api/boiler/status` or `GET /api/zones`): Poll every **2.5 to 3.5 seconds**.
   - Slower Telemetry (`GET /api/setup/time`): Poll every **60 seconds**.
   - Diagnostics (`GET /api/diag/status`): Poll every **10 seconds**.
2. **Optimistic UI Updates:**
   - When a user changes a temperature slider, update UI state immediately.
   - Send `POST /api/zones/setpoint?id=X&temp=Y` in the background.
3. **Idempotency & Re-tries:**
   - The gas boiler RF trigger has an automatic 60-second anti-short-cycling guard.
   - If the API returns `HTTP 429`, parse the `"waitSec"` property and display a countdown timer in the UI instead of failing abruptly.
4. **Offline Resilience:**
   - If the ESP32 loses Wi-Fi, the internal hardware RTC and autonomous thermodynamic control loop continue running seamlessly.
   - The kiosk display should display a subtle reconnect banner (`DISCONNECTED`) without crashing or freezing.
