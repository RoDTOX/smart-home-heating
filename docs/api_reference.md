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
| **Boiler & Mode** | `/api/boiler/status` | `GET` | Core system telemetry, burner flame state, pump state, uptime |
| | `/api/boiler/on` | `POST` | Trigger burner flame START via 433.92 MHz RF (safe guard active) |
| | `/api/boiler/off` | `POST` | Trigger burner flame STOP via 433.92 MHz RF (idempotent) |
| | `/api/mode` | `POST` | Toggle AUTOMATIC (thermostat control) vs MANUAL mode |
| **Thermal Zones** | `/api/zones` | `GET` | All underfloor heating zones, temperatures, setpoints, valve states |
| | `/api/zones/setpoint` | `POST` | Calibrate target temperature setpoint for a specific zone |
| | `/api/zones/toggle` | `POST` | Enable or disable heating loop for a zone |
| | `/api/zones/sensor` | `POST` | Assign BLE thermometer MAC / name to a zone |
| **Bio-Schedule** | `/api/schedule` | `GET` | 4 Circadian anchors (`Sleep`, `Wake`, `Away`, `Home`) & profiles |
| | `/api/schedule` | `POST` | Update circadian anchor times & target temperatures |
| | `/api/schedule/enable` | `POST` | Toggle Bio-Schedule, Predictive Pre-Heat, and Window Guard |
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
  "firmwareVersion": "1.2.0",
  "buildDate": "Oct 13 2026 23:40:28",
  "gitCommit": "8065534",
  "resetReason": "Software Reboot (Firmware Update / Restart)",
  "boiler": "OFF",
  "boilerDemand": false,
  "autoMode": true,
  "pumpActive": false,
  "frostGuardActive": false,
  "scheduleEnabled": true,
  "activeAnchor": "SLEEP",
  "preHeatActive": false,
  "freeHeap": 124928,
  "rssi": -58,
  "uptimeSec": 450,
  "sensorTimeoutSec": 600,
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
- **Response `429 Too Many Requests` (Anti-Short-Cycling Protected):**
```json
{
  "status": "rate_limited",
  "error": "Anti-short-cycling guard active",
  "waitSec": 45,
  "boiler": "OFF"
}
```

#### `POST /api/boiler/off`
Turns off gas boiler flame. Always succeeds immediately (failsafe stop is never rate-limited).

- **Response `200 OK`:**
```json
{
  "status": "ok",
  "boiler": "OFF"
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
  "boilerDemand": false,
  "actuatorType": "NC",
  "pumpEnabled": true,
  "pumpActive": false,
  "frostGuardActive": false,
  "zones": [
    {
      "id": 1,
      "name": "Living & Bucatarie",
      "relayPin": 26,
      "sensorName": "Living Room",
      "sensorMac": "A4:C1:38:A4:C1:DD",
      "targetTemp": 20.0,
      "effectiveTarget": 20.0,
      "hysteresis": 0.30,
      "enabled": true,
      "currentTemp": 24.1,
      "sensorValid": true,
      "sensorFault": false,
      "frostActive": false,
      "windowPauseActive": false,
      "windowPauseRemainingSec": 0,
      "demand": false,
      "relayEngaged": false,
      "state": "Idle (Temp OK)"
    },
    {
      "id": 2,
      "name": "Camera Copilului",
      "relayPin": 25,
      "sensorName": "Kids Room",
      "sensorMac": "A4:C1:38:84:A1:E8",
      "targetTemp": 21.0,
      "effectiveTarget": 21.0,
      "hysteresis": 0.30,
      "enabled": true,
      "currentTemp": 23.4,
      "sensorValid": true,
      "sensorFault": false,
      "frostActive": false,
      "windowPauseActive": false,
      "windowPauseRemainingSec": 0,
      "demand": false,
      "relayEngaged": false,
      "state": "Idle (Temp OK)"
    }
  ]
}
```

#### `POST /api/zones/setpoint`
Adjusts target temperature for a specific zone with immediate NVS flash persistence.

- **Query Parameters / Form Data:**
  - `id`: Zone ID (`1`, `2`, `3`, etc.).
  - `target` or `temp`: Target temperature in Celsius (`15.0` to `28.0`).
- **Response `200 OK`:**
```json
{
  "status": "ok",
  "zoneId": 1,
  "targetTemp": 22.5
}
```

---

### 3.3. Smart Bio-Schedule & Circadian Anchors

#### `GET /api/schedule`
Retrieves configured natural lifestyle anchors and profiles.

- **Response `200 OK`:**
```json
{
  "enabled": true,
  "preHeatEnabled": true,
  "windowDetectionEnabled": true,
  "activeAnchor": "SLEEP",
  "nextAnchor": "WAKE",
  "minutesToNext": 360,
  "isPreHeatActive": false,
  "isWeekend": false,
  "profile": "Weekday",
  "weekday": [
    { "name": "WAKE",  "time": "06:30", "temp": 22.0 },
    { "name": "AWAY",  "time": "08:30", "temp": 19.5 },
    { "name": "HOME",  "time": "17:00", "temp": 21.5 },
    { "name": "SLEEP", "time": "23:00", "temp": 20.0 }
  ],
  "weekend": [
    { "name": "WAKE",  "time": "08:00", "temp": 22.5 },
    { "name": "AWAY",  "time": "11:00", "temp": 20.5 },
    { "name": "HOME",  "time": "15:00", "temp": 22.0 },
    { "name": "SLEEP", "time": "23:30", "temp": 20.5 }
  ]
}
```

#### `POST /api/schedule/enable`
Enables or disables scheduling subsystems.

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
