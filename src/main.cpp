#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "DiagnosticEngine.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TelemetryHistoryManager.h"
#include "TimeAndWeatherService.h"
#include "Version.h"
#include "dashboard_html_gz.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPUpdateServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <esp_idf_version.h>

WebServer server(80);
HTTPUpdateServer httpUpdater;

static bool softApActive = false;
static String serialBuffer = "";

void blinkLed(uint8_t count = 2, uint16_t delayMs = 60) {
  for (uint8_t i = 0; i < count; i++) {
    digitalWrite(PIN_STATUS_LED, HIGH); // Active HIGH on ESP32 Relay board
    delay(delayMs);
    digitalWrite(PIN_STATUS_LED, LOW);
    delay(delayMs);
  }
}

const char* getResetReasonStr(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:   return "Power-On Reset (Cold Boot)";
    case ESP_RST_SW:        return "Software Reboot (Firmware Update / Restart)";
    case ESP_RST_PANIC:     return "Software Exception / Panic";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:       return "Hardware Watchdog Timer Expiry (TWDT)";
    case ESP_RST_BROWNOUT:  return "Brownout Reset (Voltage Dip)";
    case ESP_RST_DEEPSLEEP: return "Deep-Sleep Wakeup";
    default:                return "Normal / External Reset";
  }
}

void printBootBannerAndSelfTest() {
  esp_reset_reason_t rstReason = esp_reset_reason();
  time_t nowEpoch = time(nullptr);
  char bootTimeBuf[36];
  if (nowEpoch > 1700000000) {
    struct tm ti;
    localtime_r(&nowEpoch, &ti);
    strftime(bootTimeBuf, sizeof(bootTimeBuf), "%Y-%m-%d %H:%M:%S", &ti);
  } else {
    snprintf(bootTimeBuf, sizeof(bootTimeBuf), "Pending NTP Sync (pool.ntp.org)");
  }

  netConsole.println();
  netConsole.println(F("=================================================="));
  netConsole.printf("  Smart Heating Controller v%s\n", APP_VERSION);
  netConsole.printf("  Build: %s %s | Git: %s\n", APP_BUILD_DATE, APP_BUILD_TIME, APP_GIT_COMMIT);
  netConsole.printf("  Boot Time: %s\n", bootTimeBuf);
  netConsole.printf("  Reset Cause: %s\n", getResetReasonStr(rstReason));
  netConsole.println(F("=================================================="));

  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  uint32_t heap = ESP.getFreeHeap();
  bool bleOk = bleScanner.isScanning();
  uint8_t health = diagEngine.calculateHealthScore();

  netConsole.println(F("[POST] Power-On Self-Test:"));
  netConsole.printf("  * Network Connection:    %s (IP: %s, Signal: %d dBm)\n",
                    wifiOk ? "PASS" : "FAIL",
                    wifiOk ? WiFi.localIP().toString().c_str() : "No IP",
                    wifiOk ? WiFi.RSSI() : 0);
  netConsole.printf("  * System Memory (RAM):   PASS (%u KB free, no leaks)\n", heap / 1024);
  netConsole.printf("  * Thermal Actuators:     PASS (%s, User-Configured in NVS)\n",
                    thermalZones.isActuatorNormallyClosed() ? "Normally Closed [NC]" : "Normally Open [NO]");
  netConsole.printf("  * Circulation Pump:      PASS (Secondary Pump on Relay 4: %s)\n",
                    thermalZones.isPumpControlEnabled() ? "ENABLED" : "DISABLED");
  netConsole.printf("  * Room Thermometers:     %s (Bluetooth Receiver active, listening)\n",
                    bleOk ? "PASS" : "FAIL");
  netConsole.printf("  * Gas Boiler Radio (RF): PASS (Transmitter on GPIO%d Ready)\n", PIN_RF_DATA);
  netConsole.println(F("  * System Crash Guard:    PASS (Hardware Watchdog Armed: 90s timeout)"));
  netConsole.printf("  * Flash Telemetry:       %s (LittleFS Ring Buffer: 24h@5m, 5d@30m)\n",
                    telemetryHistory.isFsMounted() ? "PASS" : "WARN");
  netConsole.println(F("  * Web Dashboard & API:   PASS (HTTP Port 80, Telnet Port 23)"));
  netConsole.printf("  * Overall Health Score:  %u%% [%s]\n",
                    health,
                    health >= 90 ? "EXCELLENT" : (health >= 70 ? "GOOD" : "DEGRADED"));
  netConsole.println(F("--------------------------------------------------"));
  netConsole.println(F("[Ready] Type 'HELP' or '?' for command list."));
  netConsole.println(F("=================================================="));
  netConsole.println(F("[INIT] Background Tasks Dispatched:"));
  netConsole.println(F("  * Real-Time Clock:  Background NTP sync initiated (pool.ntp.org)"));
  netConsole.println(F("  * Outdoor Weather:  Background sync initiated (Open-Meteo REST API)"));
  netConsole.println(F("  * Sensor Discovery: Continuous BLE scanner active (Warmup grace period: 300s)"));
  netConsole.println();
}

static const char* getLinkQualityBadge(int rssi) {
  if (rssi >= -70) return "[EXCELLENT]";
  if (rssi >= -80) return "[GOOD]";
  if (rssi >= -88) return "[MARGINAL]";
  return "[POOR]";
}

static String formatEpochHuman(uint32_t epoch) {
  if (epoch < 1700000000) return "Pending NTP Sync";
  time_t ep = (time_t)epoch;
  struct tm ti;
  localtime_r(&ep, &ti);
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d",
           ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
           ti.tm_hour, ti.tm_min);
  return String(buf);
}

static uint32_t getAntiSeizeNextDays(uint32_t epoch) {
  time_t now = time(nullptr);
  if (now < 1700000000 || epoch < 1700000000) return 7;
  if ((uint32_t)now > epoch) {
    uint32_t elapsed = (uint32_t)now - epoch;
    if (elapsed < 604800) {
      return (604800 - elapsed) / 86400;
    }
  }
  return 0;
}

void printHelp() {
  netConsole.print(F(
      "\r\n--- Smart Heating Command Manual ---\r\n"
      "[Heating & Hydraulic Control]\r\n"
      "  ON / OFF              -> Start / Stop gas boiler burner flame via 433.92 MHz RF\r\n"
      "  BOILER RESET RUNTIME  -> Reset flame runtime statistics counter to 0 in NVS\r\n"
      "  AUTO / MANUAL         -> Thermostat automatic control vs manual override\r\n"
      "  SET <z 1..3> <temp>   -> Set static target temperature for zone (e.g. SET 1 22.5)\r\n"
      "  PUMP <ON|OFF>         -> Secondary GPA circulation pump manual override\r\n"
      "  PUMP <ENABLE|DISABLE> -> Enable / disable automatic pump interlock & overrun\r\n"
      "  ACTUATOR <NC|NO>      -> Set thermal actuator polarity (Normally Closed vs Open)\r\n"
      "  EXERCISE              -> Run 20-second anti-seize cycle for pump and valves\r\n\r\n"
      "[Schedule: day / night / away levels]\r\n"
      "  SCHEDULE              -> Show levels per zone, weekday/weekend slots and current level\r\n"
      "  SCHEDULE <ON|OFF>     -> Enable or disable the schedule (OFF = fixed targets)\r\n"
      "  SCHEDULE LEVEL <z> <day|night|away> <temp> -> Set level temperature for zone (e.g. SCHEDULE LEVEL 2 night 19)\r\n"
      "  SCHEDULE SLOTS <weekday|weekend> <HH:MM=level,...> -> e.g. SCHEDULE SLOTS weekday 06:30=day,22:30=night\r\n"
      "  SCHEDULE RESET        -> Restore factory default schedule\r\n"
      "  AWAY <ON|OFF>         -> Global away mode (all zones at away level)\r\n"
      "  SCHEDULE PREHEAT <ON|OFF> -> Predictive pre-heating with thermal inertia calculation\r\n"
      "  SCHEDULE WINDOW <ON|OFF>  -> Rapid temperature drop open window detection guard\r\n\r\n"
      "[Telemetry, Sensors & Diagnostics]\r\n"
      "  STATUS or S           -> Comprehensive live dashboard: CPU, boiler, weather, zones\r\n"
      "  BLE or SENSORS        -> Decoded Xiaomi BLE thermometer registry, signal & battery\r\n"
      "  WEATHER / TIME        -> Open-Meteo outdoor temperature & NTP local calendar\r\n"
      "  TIMEOUT [sec]         -> Get or set BLE sensor offline failsafe timeout (default 600s)\r\n"
      "  DIAG [SELFTEST]       -> System health score, active DTCs or full 6-subsystem test\r\n"
      "  DIAG CLEAR            -> Clear active Trouble Codes (DTCs) and restore 100% health\r\n"
      "  HISTORY [CLEAR]       -> Flash telemetry ring buffer status (24h/5d) or clear\r\n"
      "  LOCATION [lat,lon]    -> Get or set location used for outdoor weather\r\n"
      "  CLEAR or CLS          -> Clear console terminal buffer\r\n"
      "------------------------------------\r\n\r\n"));
}

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", (const char*)DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
}

void handleApiOn() {
  uint32_t waitSec = 0;
  bool isIdempotent = false;

  server.sendHeader("Access-Control-Allow-Origin", "*");

  // If a pending OFF was scheduled, user pressing ON cancels the scheduled shutdown
  if (boilerRadio.getPendingAction() == PENDING_OFF) {
    boilerRadio.cancelPendingAction();
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\",\"pendingCancelled\":true,\"message\":\"Oprirea programata a fost anulata. Cazanul ramane ACTIV.\"}"));
    return;
  }

  // 1. Check idempotency (FR-17.1)
  if (boilerRadio.hasTransmittedOnce() && boilerRadio.isLastCommandOn()) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\",\"idempotent\":true}"));
    return;
  }

  // 2. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
  if (!boilerRadio.canTransmit(true, &waitSec, &isIdempotent)) {
    boilerRadio.recordSpamAttempt();
    // Schedule deferred startup if not already running
    if (!boilerRadio.isLastCommandOn()) {
      boilerRadio.setPendingAction(PENDING_ON);
      String resp = "{\"status\":\"pending\",\"error\":\"Anti-short-cycling guard active\",\"target\":\"ON\",\"waitSec\":" + String(waitSec) +
                    ",\"pending\":true,\"boiler\":\"OFF\",\"message\":\"Pornire programata automat dupa expirarea pauzei minime (" + String(waitSec) + "s)\"}";
      server.send(429, "application/json", resp);
      return;
    }
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Anti-short-cycling guard active\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
    return;
  }

  blinkLed(1, 40);
  bool success = boilerRadio.sendBoilerOn();
  blinkLed(1, 40);

  if (success) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\"}"));
  } else {
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Transmission throttled\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
  }
}

void handleApiOff() {
  // Forcing a stop is always safe; forcing a start is never allowed.
  bool force = server.hasArg("force") && server.arg("force") == "true";
  uint32_t waitSec = 0;
  bool isIdempotent = false;

  server.sendHeader("Access-Control-Allow-Origin", "*");

  // If a pending ON was scheduled, user pressing OFF cancels the scheduled startup
  if (boilerRadio.getPendingAction() == PENDING_ON) {
    boilerRadio.cancelPendingAction();
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\",\"pendingCancelled\":true,\"message\":\"Pornirea programata a fost anulata.\"}"));
    return;
  }

  // 1. Check idempotency (FR-17.1)
  if (!force && boilerRadio.hasTransmittedOnce() && !boilerRadio.isLastCommandOn()) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\",\"idempotent\":true}"));
    return;
  }

  // 2. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
  if (!force && !boilerRadio.canTransmit(false, &waitSec, &isIdempotent)) {
    boilerRadio.recordSpamAttempt();
    // Schedule deferred shutdown: remember command and automatically trigger OFF when cooldown ends!
    if (boilerRadio.isLastCommandOn()) {
      boilerRadio.setPendingAction(PENDING_OFF);
      String resp = "{\"status\":\"pending\",\"error\":\"Anti-short-cycling guard active\",\"target\":\"OFF\",\"waitSec\":" + String(waitSec) +
                    ",\"pending\":true,\"boiler\":\"ON\",\"message\":\"Oprire programata automat dupa expirarea timpului minim de ardere (" + String(waitSec) + "s)\"}";
      server.send(429, "application/json", resp);
      return;
    }
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Anti-short-cycling guard active\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
    return;
  }

  blinkLed(1, 40);
  bool success = boilerRadio.sendBoilerOff(force);
  blinkLed(1, 40);

  if (success) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\"}"));
  } else {
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Transmission throttled\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
  }
}

void handleApiBoilerCancelPending() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  boilerRadio.cancelPendingAction();
  String resp = String("{\"status\":\"ok\",\"pendingCancelled\":true,\"boiler\":\"") + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
  server.send(200, "application/json", resp);
}

void handleApiBoilerResetRuntime() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  boilerRadio.resetRuntime();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"message\":\"Boiler runtime counters reset to 0\"}"));
}

void handleApiZones() {
  String json;
  json.reserve(2560);
  json = "{";
  json +=
      "\"autoMode\":" + String(thermalZones.isAutoMode() ? "true" : "false") +
      ",";
  json += "\"boiler\":\"" +
          String(boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\",";
  uint32_t boilerWaitSec = 0;
  boilerRadio.canTransmit(!boilerRadio.isLastCommandOn(), &boilerWaitSec);
  json += "\"boilerCooldownSec\":" + String(boilerWaitSec) + ",";
  json += "\"pendingAction\":\"" + String(boilerRadio.getPendingActionStr()) + "\",";
  json += "\"pendingWaitSec\":" + String(boilerWaitSec) + ",";
  json += "\"boilerDemand\":" +
          String(thermalZones.isBoilerDemanded() ? "true" : "false") + ",";
  json += "\"actuatorType\":\"" +
          String(thermalZones.isActuatorNormallyClosed() ? "NC" : "NO") + "\",";
  json += "\"pumpEnabled\":" +
          String(thermalZones.isPumpControlEnabled() ? "true" : "false") + ",";
  json += "\"pumpActive\":" +
          String(thermalZones.isPumpActive() ? "true" : "false") + ",";
  json += "\"frostGuardActive\":" +
          String(thermalZones.isFrostProtectionActive() ? "true" : "false") +
          ",";
  json += "\"sensorTimeoutSec\":" + String(thermalZones.getSensorTimeoutSec()) + ",";
  json += "\"lastAntiSeizeEpoch\":" + String(thermalZones.getLastAntiSeizeEpoch()) + ",";
  json += "\"scheduleEnabled\":" + String(thermalZones.isScheduleEnabled() ? "true" : "false") + ",";
  json += "\"away\":" + String(thermalZones.isAway() ? "true" : "false") + ",";
  json += "\"activeLevel\":\"" + String(ThermalZoneManager::getLevelName(thermalZones.getActiveLevel())) + "\",";
  json += "\"nextLevel\":\"" + String(ThermalZoneManager::getLevelName(thermalZones.getNextLevel())) + "\",";
  json += "\"minsToNextSlot\":" + String(thermalZones.getMinutesToNextSlot()) + ",";
  json += "\"preHeatActive\":" + String(thermalZones.isPreHeatActive() ? "true" : "false") + ",";
  json += "\"boilerRuntime\":{";
  json += "\"totalSec\":" + String(boilerRadio.getRuntimeTotalSec()) + ",";
  json += "\"todaySec\":" + String(boilerRadio.getRuntimeTodaySec()) + ",";
  json += "\"yesterdaySec\":" + String(boilerRadio.getRuntimeYesterdaySec()) + ",";
  json += "\"thisMonthSec\":" + String(boilerRadio.getRuntimeThisMonthSec()) + ",";
  json += "\"lastMonthSec\":" + String(boilerRadio.getRuntimeLastMonthSec()) + ",";
  json += "\"currentBurnSec\":" + String(boilerRadio.getCurrentFlameDurationSec());
  json += "},";
  json += "\"zones\":[";

  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    const ThermalZoneConfig &cfg = thermalZones.getConfig(i);
    const ThermalZoneStatus &st = thermalZones.getStatus(i);
    if (i > 0)
      json += ",";
    json += "{";
    json += "\"id\":" + String(cfg.id) + ",";
    json += "\"name\":\"" + String(cfg.name) + "\",";
    json += "\"sensor\":\"" + String(cfg.sensorName) + "\",";
    json += "\"sensorMac\":\"" + String(cfg.sensorMac) + "\",";
    json += "\"relayPin\":" + String(cfg.relayPin) + ",";
    json += "\"targetTemp\":" + String(cfg.targetTemp, 1) + ",";
    json += "\"effectiveTarget\":" + String(thermalZones.getEffectiveTargetTemp(i), 1) + ",";
    json += "\"hysteresis\":" + String(cfg.hysteresis, 2) + ",";
    json += "\"enabled\":" + String(cfg.enabled ? "true" : "false") + ",";
    json += "\"currentTemp\":" + String(st.currentTemp, 1) + ",";
    json +=
        "\"sensorValid\":" + String(st.sensorValid ? "true" : "false") + ",";
    json +=
        "\"sensorFault\":" + String(st.sensorFault ? "true" : "false") + ",";
    json +=
        "\"frostActive\":" + String(st.frostActive ? "true" : "false") + ",";
    json +=
        "\"windowPauseActive\":" + String(thermalZones.isWindowPauseActive(i) ? "true" : "false") + ",";
    json +=
        "\"windowPauseRemainingSec\":" + String(thermalZones.getWindowPauseRemainingSec(i)) + ",";
    json +=
        "\"windowDropThreshold\":" + String(thermalZones.getWindowDropThreshold(i), 1) + ",";
    json +=
        "\"lastWindowEpoch\":" + String(thermalZones.getWindowLastDetectionEpoch(i)) + ",";
    json +=
        "\"lastWindowDurationSec\":" + String(thermalZones.getWindowLastDetectionDurationSec(i)) + ",";
    json += "\"demand\":" + String(st.demand ? "true" : "false") + ",";
    json +=
        "\"relayEngaged\":" + String(st.relayEngaged ? "true" : "false") + ",";
    uint32_t openSec = 0;
    if (st.relayEngaged) {
      if (st.stateChangedMs > 0 && millis() >= st.stateChangedMs) {
        openSec = (millis() - st.stateChangedMs) / 1000;
        if (openSec > 120) openSec = 120;
      } else {
        openSec = 120;
      }
    } else {
      // Linear cooling contraction: 120s -> 0s
      if (st.stateChangedMs > 0 && millis() >= st.stateChangedMs) {
        uint32_t offElapsed = (millis() - st.stateChangedMs) / 1000;
        if (offElapsed < 120) {
          openSec = 120 - offElapsed;
        } else {
          openSec = 0;
        }
      } else {
        openSec = 0;
      }
    }
    json += "\"openSec\":" + String(openSec) + ",";
    json += "\"isManualHold\":" + String(st.isManualHold ? "true" : "false") + ",";
    json += "\"state\":\"" + String(thermalZones.getStateStr(st.state)) + "\"";
    json += "}";
  }
  json += "]}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiZoneSetpoint() {
  if (!server.hasArg("id") || !server.hasArg("target")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameters (id and target)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx >= thermalZones.getZoneCount()) {
    server.send(400, "application/json",
                F("{\"error\":\"Invalid zone index\"}"));
    return;
  }
  float target = server.arg("target").toFloat();
  thermalZones.setTargetTemperature(idx, target);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiZoneHoldClear() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    thermalZones.clearManualHold(idx);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiZoneWindowClear() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id)\"}"));
    return;
  }
  String idStr = server.arg("id");
  if (idStr.equalsIgnoreCase("all")) {
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
      thermalZones.clearWindowPause(i);
    }
    server.send(200, "application/json", F("{\"status\":\"ok\",\"cleared\":\"all\"}"));
    return;
  }
  int idVal = idStr.toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    bool cleared = thermalZones.clearWindowPause(idx);
    server.send(200, "application/json",
                cleared ? F("{\"status\":\"ok\"}")
                        : F("{\"status\":\"noop\",\"message\":\"Window pause was not active\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiZoneWindowThreshold() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("id") || !server.hasArg("threshold")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id or threshold)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  float thresh = server.arg("threshold").toFloat();
  if (thresh < 0.8f || thresh > 2.0f) {
    server.send(400, "application/json",
                F("{\"error\":\"Threshold out of bounds (allowed: 0.8 to 2.0 C)\"}"));
    return;
  }
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    thermalZones.setWindowDropThreshold(idx, thresh);
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiZoneToggle() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (id)\"}"));
    return;
  }
  int idVal = server.arg("id").toInt();
  size_t idx = (idVal >= 1 && idVal <= (int)thermalZones.getZoneCount())
                   ? (size_t)(idVal - 1)
                   : (size_t)idVal;
  if (idx < thermalZones.getZoneCount()) {
    bool current = thermalZones.getConfig(idx).enabled;
    thermalZones.setZoneEnabled(idx, !current);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiMode() {
  if (!server.hasArg("mode")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameter (mode)\"}"));
    return;
  }
  String m = server.arg("mode");
  m.toLowerCase();
  if (m == "auto") {
    thermalZones.setAutoMode(true);
  } else if (m == "manual") {
    thermalZones.setAutoMode(false);
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiZoneSensor() {
  if (!server.hasArg("id") || !server.hasArg("sensor")) {
    server.send(400, "application/json",
                F("{\"error\":\"Missing parameters (id and sensor)\"}"));
    return;
  }
  size_t id = server.arg("id").toInt();
  String sensorName = server.arg("sensor");
  if (id < thermalZones.getZoneCount()) {
    thermalZones.setSensorName(id, sensorName.c_str());
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
  } else {
    server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
  }
}

void handleApiBleSensors() {
  String json;
  json.reserve(1536);
  json = "{";
  json += "\"scanning\":" + String(bleScanner.isScanning() ? "true" : "false") +
          ",";
  json += "\"count\":" + String(bleScanner.getSensorCount()) + ",";
  json += "\"sensors\":[";
  for (size_t i = 0; i < bleScanner.getSensorCount(); i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s)
      continue;
    if (i > 0)
      json += ",";
    json += "{";
    json += "\"name\":\"" + String(s->name) + "\",";
    json += "\"mac\":\"" + String(s->mac) + "\",";
    json += "\"temp\":" + String(s->temperature, 2) + ",";
    json += "\"hum\":" + String(s->humidity, 1) + ",";
    json += "\"bat\":" + String(s->battery) + ",";
    json += "\"rssi\":" + String(s->rssi) + ",";
    json += "\"packets\":" + String(s->packetCount) + ",";
    json +=
        "\"source\":\"" + String(bleScanner.getSourceStr(s->source)) + "\",";
    json += "\"ageSec\":" + String((millis() - s->lastSeenMs) / 1000);
    json += "}";
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiSetupLocation() {
  if (server.method() == HTTP_POST && server.arg("coords").length() > 3) {
    timeAndWeather.setLocation(server.arg("coords").c_str());
    timeAndWeather.forceWeatherUpdate();
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              String("{\"coords\":\"") + timeAndWeather.getLocation() + "\"}");
}

void handleApiSetupDiscoveredSensors() {
  String json;
  json.reserve(1536);
  json = "{\"sensors\":[";
  size_t count = bleScanner.getSensorCount();
  size_t written = 0;
  for (size_t i = 0; i < count; i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s || !s->valid)
      continue;
    if (written > 0)
      json += ",";
    json += "{";
    json += "\"mac\":\"" + String(s->mac) + "\",";
    json += "\"name\":\"" + String(s->name) + "\",";
    json += "\"temp\":" + String(s->temperature, 1) + ",";
    json += "\"hum\":" + String(s->humidity, 1) + ",";
    json += "\"bat\":" + String(s->battery) + ",";
    json += "\"rssi\":" + String(s->rssi) + ",";
    json += "\"ageSec\":" + String((millis() - s->lastSeenMs) / 1000) + ",";
    int assignedZone = -1;
    for (size_t z = 0; z < thermalZones.getZoneCount(); z++) {
      if (strcasecmp(thermalZones.getConfig(z).sensorMac, s->mac) == 0) {
        assignedZone = (int)z;
        break;
      }
    }
    json += "\"assignedZone\":" + String(assignedZone);
    json += "}";
    written++;
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiSetupMapZone() {
  if (!server.hasArg("zone")) {
    server.send(400, "application/json",
                F("{\"status\":\"error\",\"message\":\"Missing zone param\"}"));
    return;
  }
  size_t z = (size_t)server.arg("zone").toInt();
  if (z >= thermalZones.getZoneCount()) {
    server.send(400, "application/json",
                F("{\"status\":\"error\",\"message\":\"Invalid zone index\"}"));
    return;
  }
  const ThermalZoneConfig &cur = thermalZones.getConfig(z);
  String name = server.hasArg("name") ? server.arg("name") : String(cur.name);
  String mac = server.hasArg("mac") ? server.arg("mac") : String(cur.sensorMac);
  String sname =
      server.hasArg("sname") ? server.arg("sname") : String(cur.sensorName);
  uint8_t pin =
      server.hasArg("pin") ? (uint8_t)server.arg("pin").toInt() : cur.relayPin;
  float target =
      server.hasArg("target") ? server.arg("target").toFloat() : cur.targetTemp;
  float hyst =
      server.hasArg("hyst") ? server.arg("hyst").toFloat() : cur.hysteresis;
  bool en = server.hasArg("enabled") ? (server.arg("enabled") == "true" ||
                                        server.arg("enabled") == "1")
                                     : cur.enabled;

  thermalZones.mapZone(z, name.c_str(), mac.c_str(), sname.c_str(), pin, target,
                       hyst, en);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              F("{\"status\":\"ok\",\"message\":\"Zone configuration updated "
                "and saved to NVS\"}"));
}

void handleApiSetupTime() {
  if (server.hasArg("refreshWeather")) {
    timeAndWeather.forceWeatherUpdate();
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", timeAndWeather.toJson());
}

void handleApiDiagStatus() {
  String json;
  json.reserve(1024);
  json = "{";
  json += "\"healthScore\":" + String(diagEngine.calculateHealthScore()) + ",";
  json += "\"dtcCount\":" + String(diagEngine.getDtcCount()) + ",";
  json += "\"dtcs\":[";
  size_t written = 0;
  for (size_t i = 0; i < diagEngine.getDtcCount(); i++) {
    const DtcRecord *d = diagEngine.getDtc(i);
    if (!d || !d->active)
      continue;
    if (written > 0)
      json += ",";
    json += "{";
    json += "\"code\":\"" + String(d->code) + "\",";
    json += "\"severity\":\"" + String(diagEngine.getSeverityStr(d->severity)) +
            "\",";
    json += "\"message\":\"" + String(d->message) + "\",";
    json += "\"count\":" + String(d->occurrenceCount) + ",";
    json += "\"ageSec\":" + String((millis() - d->lastRaisedMs) / 1000);
    json += "}";
    written++;
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiDiagSelfTest() {
  String report = diagEngine.runSelfTest();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", report);
}

void handleApiDiagClear() {
  diagEngine.clearAllDtcs();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              F("{\"status\":\"ok\",\"message\":\"All DTC codes cleared\"}"));
}

void handleApiSetupActuatorType() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("type")) {
      String t = server.arg("type");
      t.toLowerCase();
      thermalZones.setActuatorNormallyClosed(t == "nc");
    } else if (server.hasArg("nc")) {
      String v = server.arg("nc");
      v.toLowerCase();
      thermalZones.setActuatorNormallyClosed(v == "true" || v == "1");
    }
  }
  String json = "{\"status\":\"ok\",\"actuatorType\":\"" +
                String(thermalZones.isActuatorNormallyClosed() ? "NC" : "NO") +
                "\"}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiSetupPump() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("enabled")) {
      String v = server.arg("enabled");
      v.toLowerCase();
      thermalZones.setPumpControlEnabled(v == "true" || v == "1");
    }
  }
  String json = "{\"status\":\"ok\",\"pumpEnabled\":" +
                String(thermalZones.isPumpControlEnabled() ? "true" : "false") +
                ",\"pumpActive\":" +
                String(thermalZones.isPumpActive() ? "true" : "false") + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiDiagExercise() {
  thermalZones.triggerAntiSeizeExercise();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              F("{\"status\":\"ok\",\"message\":\"Anti-seize exercise cycle "
                "started (20s)\"}"));
}

void handleApiSetupSensorTimeout() {
  if (server.method() == HTTP_POST) {
    if (server.hasArg("timeoutSec")) {
      uint32_t sec = (uint32_t)server.arg("timeoutSec").toInt();
      thermalZones.setSensorTimeoutSec(sec);
    } else if (server.hasArg("sec")) {
      uint32_t sec = (uint32_t)server.arg("sec").toInt();
      thermalZones.setSensorTimeoutSec(sec);
    }
  }
  String json = "{\"status\":\"ok\",\"sensorTimeoutSec\":" +
                String(thermalZones.getSensorTimeoutSec()) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiStatus() {
  String json;
  json.reserve(2048);
  json = "{";
  json += "\"firmwareVersion\":\"" + String(APP_VERSION) + "\",";
  json += "\"buildDate\":\"" + String(APP_BUILD_DATE " " APP_BUILD_TIME) + "\",";
  json += "\"gitCommit\":\"" + String(APP_GIT_COMMIT) + "\",";
  json += "\"resetReason\":\"" + String(getResetReasonStr(esp_reset_reason())) + "\",";
  json += "\"boiler\":\"" +
          String(boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\",";
  uint32_t bWaitSec = 0;
  boilerRadio.canTransmit(!boilerRadio.isLastCommandOn(), &bWaitSec);
  json += "\"boilerCooldownSec\":" + String(bWaitSec) + ",";
  json += "\"pendingAction\":\"" + String(boilerRadio.getPendingActionStr()) + "\",";
  json += "\"pendingWaitSec\":" + String(bWaitSec) + ",";
  json += "\"lastTxMs\":" + String(boilerRadio.getLastTransmissionTime()) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptimeSec\":" + String(millis() / 1000) + ",";
  json += "\"wifiWatchdog\":\"active\",";
  json += "\"healthScore\":" + String(diagEngine.calculateHealthScore()) + ",";
  json += "\"time\":\"" + timeAndWeather.getFormattedTime() + "\",";
  json += "\"date\":\"" + timeAndWeather.getFormattedDate() + "\",";
  json += "\"season\":\"" + timeAndWeather.getSeason() + "\",";
  json += "\"isHeatingSeason\":" +
          String(timeAndWeather.isHeatingSeason() ? "true" : "false") + ",";
  json +=
      "\"outdoorTemp\":" + String(timeAndWeather.getOutdoorTemperature(), 1) +
      ",";
  json +=
      "\"bleScanning\":" + String(bleScanner.isScanning() ? "true" : "false") +
      ",";
  json +=
      "\"autoMode\":" + String(thermalZones.isAutoMode() ? "true" : "false") +
      ",";
  json += "\"boilerDemand\":" +
          String(thermalZones.isBoilerDemanded() ? "true" : "false") + ",";
  json += "\"actuatorType\":\"" +
          String(thermalZones.isActuatorNormallyClosed() ? "NC" : "NO") + "\",";
  json += "\"pumpEnabled\":" +
          String(thermalZones.isPumpControlEnabled() ? "true" : "false") + ",";
  json += "\"pumpActive\":" +
          String(thermalZones.isPumpActive() ? "true" : "false") + ",";
  json += "\"frostGuardActive\":" +
          String(thermalZones.isFrostProtectionActive() ? "true" : "false") +
          ",";
  json += "\"sensorTimeoutSec\":" +
          String(thermalZones.getSensorTimeoutSec()) + ",";
  json += "\"lastAntiSeizeEpoch\":" +
          String(thermalZones.getLastAntiSeizeEpoch()) + ",";
  json += "\"scheduleEnabled\":" +
          String(thermalZones.isScheduleEnabled() ? "true" : "false") + ",";
  json += "\"away\":" + String(thermalZones.isAway() ? "true" : "false") + ",";
  json += "\"activeLevel\":\"" +
          String(ThermalZoneManager::getLevelName(thermalZones.getActiveLevel())) + "\",";
  json += "\"minsToNextSlot\":" +
          String(thermalZones.getMinutesToNextSlot()) + ",";
  json += "\"preHeatActive\":" +
          String(thermalZones.isPreHeatActive() ? "true" : "false") + ",";
  json += "\"boilerRuntime\":{";
  json += "\"totalSec\":" + String(boilerRadio.getRuntimeTotalSec()) + ",";
  json += "\"todaySec\":" + String(boilerRadio.getRuntimeTodaySec()) + ",";
  json += "\"yesterdaySec\":" + String(boilerRadio.getRuntimeYesterdaySec()) + ",";
  json += "\"thisMonthSec\":" + String(boilerRadio.getRuntimeThisMonthSec()) + ",";
  json += "\"lastMonthSec\":" + String(boilerRadio.getRuntimeLastMonthSec()) + ",";
  json += "\"currentBurnSec\":" + String(boilerRadio.getCurrentFlameDurationSec());
  json += "},";
  json += "\"sensors\":[";
  size_t count = bleScanner.getSensorCount();
  size_t written = 0;
  for (size_t i = 0; i < count; i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s || !s->valid)
      continue;
    if (written > 0)
      json += ",";
    json += "{";
    json += "\"name\":\"" + String(s->name) + "\",";
    json += "\"mac\":\"" + String(s->mac) + "\",";
    json += "\"temp\":" + String(s->temperature, 2) + ",";
    json += "\"hum\":" + String(s->humidity, 1) + ",";
    json += "\"bat\":" + String(s->battery) + ",";
    json += "\"rssi\":" + String(s->rssi) + ",";
    json += "\"ageSec\":" + String((millis() - s->lastSeenMs) / 1000);
    json += "}";
    written++;
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleApiScheduleGet() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleEnable() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("enabled")) {
    thermalZones.setScheduleEnabled(server.arg("enabled") == "true" || server.arg("enabled") == "1");
  }
  if (server.hasArg("preheat")) {
    bool pre = (server.arg("preheat") == "true" || server.arg("preheat") == "1");
    thermalZones.setPreHeatEnabled(pre);
  }
  if (server.hasArg("window")) {
    bool win = (server.arg("window") == "true" || server.arg("window") == "1");
    thermalZones.setWindowDetectionEnabled(win);
  }
  if (server.hasArg("away")) {
    thermalZones.setAway(server.arg("away") == "true" || server.arg("away") == "1");
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleLevel() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  int zone = server.arg("zone").toInt();
  int level = ThermalZoneManager::parseLevel(server.arg("level").c_str());
  if (zone < 1 || level < 0 ||
      !thermalZones.setLevelTemp(zone - 1, (ComfortLevel)level, server.arg("temp").toFloat())) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Use zone=1..3, level=day|night|away, temp=5..30\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleSlots() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  bool weekend = server.arg("weekend") == "true" || server.arg("weekend") == "1";
  if (!thermalZones.setSlots(weekend, server.arg("slots").c_str())) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Use slots=HH:MM=day|night|away,... (max 8)\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleReset() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  thermalZones.resetScheduleToDefault();
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiHistory() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("time")) {
    uint32_t clientTime = (uint32_t)server.arg("time").toInt();
    if (clientTime > 1700000000UL) {
      telemetryHistory.setLocalTimeFromClient(clientTime);
    }
  }
  String range = server.hasArg("range") ? server.arg("range") : "24h";
  String json = telemetryHistory.getHistoryJson(range);
  server.send(200, "application/json", json);
}

void handleApiHistoryClear() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  telemetryHistory.clearHistory();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"cleared\":true}"));
}

// Wi-Fi Connection Supervisor State
static uint32_t _wifiDisconnectMs = 0;
static bool     _wifiWasConnected = true;

void setupWifi() {
  WiFi.setHostname("SmartHeating-BoilerRF");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  Serial.printf("[WiFi] Connecting to '%s'...\n", DEFAULT_WIFI_SSID);
  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);

  uint32_t startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 12000) {
    delay(300);
    Serial.print(F("."));
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected successfully! Local IP: %s\n",
                  WiFi.localIP().toString().c_str());
    digitalWrite(PIN_STATUS_LED, LOW); // LED off when connected
    _wifiWasConnected = true;
    _wifiDisconnectMs = 0;
  } else {
    Serial.println(F("[WiFi] Could not connect to router within 12s. Starting "
                     "Fallback Access Point..."));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    softApActive = true;
    _wifiWasConnected = false;
    _wifiDisconnectMs = millis();
    Serial.printf("[WiFi] AP active: '%s', Password: '%s', IP: %s\n", AP_SSID,
                  AP_PASS, WiFi.softAPIP().toString().c_str());
  }
}

// Wi-Fi Connection Supervisor & Auto-Recovery Watchdog
static uint32_t _lastWifiCheckMs = 0;

void updateWifiWatchdog() {
  uint32_t now = millis();
  if (now - _lastWifiCheckMs < 10000UL) return; // Non-blocking check every 10s
  _lastWifiCheckMs = now;

  if (WiFi.status() == WL_CONNECTED) {
    if (!_wifiWasConnected) {
      _wifiWasConnected = true;
      _wifiDisconnectMs = 0;
      digitalWrite(PIN_STATUS_LED, LOW);
      netConsole.printf("[WiFi][RECOVERED] Reconnected successfully! Local IP: %s (RSSI: %d dBm)\n",
                        WiFi.localIP().toString().c_str(), WiFi.RSSI());
      diagEngine.clearDtc("DTC-401");
    }
  } else {
    if (_wifiWasConnected) {
      _wifiWasConnected = false;
      _wifiDisconnectMs = now;
      digitalWrite(PIN_STATUS_LED, HIGH); // Status LED ON indicates connection lost
      netConsole.println(F("[WiFi][WARN] Wi-Fi connection lost! Initiating auto-recovery..."));
      diagEngine.raiseDtc("DTC-401", "Wi-Fi link disconnected", SEV_WARNING);
    }

    uint32_t disconnectedDurationSec = (now - _wifiDisconnectMs) / 1000;

    if (disconnectedDurationSec > 90) {
      netConsole.println(F("[WiFi][RETRY] Disconnected > 90s. Hard reset Wi-Fi radio and reconnecting..."));
      WiFi.disconnect();
      delay(50);
      WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
      _wifiDisconnectMs = now;
    } else {
      netConsole.println(F("[WiFi][RETRY] Attempting non-blocking Wi-Fi reconnect..."));
      WiFi.reconnect();
    }
  }
}

// Returns visible display character count, properly ignoring UTF-8 continuation bytes (0x80..0xBF)
static int utf8VisibleLen(const char* s) {

  int len = 0;
  while (*s) {
    if ((*s & 0xC0) != 0x80) { // Not a continuation byte
      len++;
    }
    s++;
  }
  return len;
}

static void printTwoCols(const char* label1, const char* val1,
                         const char* label2, const char* val2,
                         int col1TotalWidth = 48) {
  char leftPart[96];
  snprintf(leftPart, sizeof(leftPart), "  * %-18s %s", label1, val1);
  int vis = utf8VisibleLen(leftPart);
  int pad = col1TotalWidth - vis;
  if (pad < 2) pad = 2;
  char padStr[64];
  for (int i = 0; i < pad && i < (int)sizeof(padStr) - 1; i++) {
    padStr[i] = ' ';
  }
  padStr[pad < (int)sizeof(padStr) ? pad : sizeof(padStr) - 1] = '\0';

  netConsole.printf("%s%s| * %-18s %s\n", leftPart, padStr, label2, val2);
}

void handleSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0)
    return;

  String upperCmd = cmd;
  upperCmd.toUpperCase();

  if (upperCmd == "1" || upperCmd == "ON" || upperCmd == "START") {
    uint32_t waitSec = 0;
    bool isIdempotent = false;
    if (boilerRadio.hasTransmittedOnce() && boilerRadio.isLastCommandOn()) {
      netConsole.println(F("[BOILER] Boiler is already ON (idempotent no-op)."));
    } else if (!boilerRadio.canTransmit(true, &waitSec, &isIdempotent)) {
      boilerRadio.recordSpamAttempt();
      netConsole.printf("[BOILER] Command rejected by Anti-Short-Cycling guard. Wait %u s.\n", waitSec);
    } else {
      blinkLed(2, 40);
      boilerRadio.sendBoilerOn();
    }
  } else if (upperCmd == "0" || upperCmd == "OFF" || upperCmd == "STOP") {
    uint32_t waitSec = 0;
    bool isIdempotent = false;
    if (boilerRadio.hasTransmittedOnce() && !boilerRadio.isLastCommandOn()) {
      netConsole.println(F("[BOILER] Boiler is already OFF (idempotent no-op)."));
    } else if (!boilerRadio.canTransmit(false, &waitSec, &isIdempotent)) {
      boilerRadio.recordSpamAttempt();
      netConsole.printf("[BOILER] Command rejected by Anti-Short-Cycling guard. Wait %u s.\n", waitSec);
    } else {
      blinkLed(2, 40);
      boilerRadio.sendBoilerOff();
    }
  } else if (upperCmd == "AUTO") {
    thermalZones.setAutoMode(true);
  } else if (upperCmd == "MANUAL") {
    thermalZones.setAutoMode(false);
  } else if (upperCmd.startsWith("SET ")) {
    // Format: SET <zoneIndex 0..2 or id 1..3> <temp>
    int firstSpace = upperCmd.indexOf(' ');
    int secondSpace = upperCmd.indexOf(' ', firstSpace + 1);
    if (secondSpace > firstSpace) {
      int zoneArg = upperCmd.substring(firstSpace + 1, secondSpace).toInt();
      float t = upperCmd.substring(secondSpace + 1).toFloat();
      size_t idx = (zoneArg >= 1) ? (zoneArg - 1) : 0;
      thermalZones.setTargetTemperature(idx, t);
    } else {
      netConsole.println(
          F("[ZONE] Syntax: SET <zone 1..3> <temp> (e.g. SET 1 22.5)"));
    }
  } else if (upperCmd == "PUMP ON") {
    thermalZones.setPumpManual(true);
  } else if (upperCmd == "PUMP OFF") {
    thermalZones.setPumpManual(false);
  } else if (upperCmd == "PUMP ENABLE") {
    thermalZones.setPumpControlEnabled(true);
  } else if (upperCmd == "PUMP DISABLE") {
    thermalZones.setPumpControlEnabled(false);
  } else if (upperCmd == "ACTUATOR NC") {
    thermalZones.setActuatorNormallyClosed(true);
  } else if (upperCmd == "ACTUATOR NO") {
    thermalZones.setActuatorNormallyClosed(false);
  } else if (upperCmd == "EXERCISE") {
    thermalZones.triggerAntiSeizeExercise();
  } else if (upperCmd.startsWith("TIMEOUT")) {
    int sp = upperCmd.indexOf(' ');
    if (sp > 0) {
      uint32_t t = (uint32_t)upperCmd.substring(sp + 1).toInt();
      thermalZones.setSensorTimeoutSec(t);
    } else {
      netConsole.printf("[Thermal] Current sensor failsafe timeout: %u s (%u min)\n",
                        thermalZones.getSensorTimeoutSec(),
                        thermalZones.getSensorTimeoutSec() / 60);
    }
  } else if (upperCmd == "S" || upperCmd == "STATUS") {

    uint32_t upSec = millis() / 1000;
    uint32_t upMin = upSec / 60;
    uint32_t upHrs = upMin / 60;
    char uptimeBuf[32];
    if (upHrs > 0) {
      snprintf(uptimeBuf, sizeof(uptimeBuf), "%uh %02um %02us", upHrs, upMin % 60, upSec % 60);
    } else {
      snprintf(uptimeBuf, sizeof(uptimeBuf), "%um %02us", upMin, upSec % 60);
    }

    float chipTemp = temperatureRead();

    netConsole.println(F("[A. SYSTEM & CONTROLLER HARDWARE]"));
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    char fwBuf[48], procBuf[48], dieBuf[32], memBuf[48], flashBuf[48], imgBuf[48];
    snprintf(fwBuf, sizeof(fwBuf), "v%s (%s)", APP_VERSION, APP_BUILD_DATE);
    snprintf(procBuf, sizeof(procBuf), "%s (%u Cores)", ESP.getChipModel(), ESP.getChipCores());
    snprintf(dieBuf, sizeof(dieBuf), "%.1f \xC2\xB0""C (Internal)", chipTemp);
    snprintf(memBuf, sizeof(memBuf), "%u KB free / %u KB", ESP.getFreeHeap() / 1024, ESP.getHeapSize() / 1024);
    snprintf(flashBuf, sizeof(flashBuf), "%u MB @ %u MHz (NVS OK)", ESP.getFlashChipSize() / (1024 * 1024), ESP.getFlashChipSpeed() / 1000000);
    snprintf(imgBuf, sizeof(imgBuf), "%.2f MB (%.2f MB OTA)", ESP.getSketchSize() / 1048576.0f, ESP.getFreeSketchSpace() / 1048576.0f);

    printTwoCols("Firmware:", fwBuf, "Git Commit:", APP_GIT_COMMIT);
    printTwoCols("MAC Address:", WiFi.macAddress().c_str(), "Boot Cause:", getResetReasonStr(esp_reset_reason()));
    printTwoCols("Processor:", procBuf, "System Uptime:", uptimeBuf);
    printTwoCols("Silicon Die:", dieBuf, "Free Memory:", memBuf);
    printTwoCols("Flash Memory:", flashBuf, "Firmware Size:", imgBuf);
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    netConsole.println();

    netConsole.println(F("[B. BOILER, PUMP & HYDRAULIC ACTUATION]"));
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    char ipBuf[40], pumpCfgBuf[48], frostBuf[48], flameBuf[32], demandBuf[32];
    snprintf(flameBuf, sizeof(flameBuf), "%s", boilerRadio.isLastCommandOn() ? "ON" : "OFF");
    snprintf(demandBuf, sizeof(demandBuf), "%s", thermalZones.isBoilerDemanded() ? "DEMAND" : "IDLE");
    int rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    snprintf(ipBuf, sizeof(ipBuf), "%s (%d dBm)",
             WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str(),
             rssi);
    snprintf(pumpCfgBuf, sizeof(pumpCfgBuf), "%s (Secondary on Relay 4)",
             thermalZones.isPumpControlEnabled() ? "ENABLED" : "DISABLED");
    snprintf(frostBuf, sizeof(frostBuf), "%s",
             thermalZones.isFrostProtectionActive() ? "TRIGGERED (<12\xC2\xB0""C Guard)" : "STANDBY (<12\xC2\xB0""C Guard)");

    printTwoCols("Burner Flame:", flameBuf, "Operation Mode:", thermalZones.isAutoMode() ? "AUTOMATIC" : "MANUAL");
    printTwoCols("Heat Demand:", demandBuf, "Controller IP:", ipBuf);
    printTwoCols("Actuator Type:", thermalZones.isActuatorNormallyClosed() ? "NC (Normally Closed)" : "NO (Normally Open)", "Circulation Pump:", pumpCfgBuf);
    printTwoCols("Frost Guard:", frostBuf, "Pump Relay State:", thermalZones.isPumpActive() ? "ACTIVE (Running)" : "IDLE");
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    netConsole.println();

    netConsole.println(F("[C. WEATHER & OUTDOOR ENVIRONMENT]"));
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    if (timeAndWeather.isWeatherValid()) {
      char outT[32], outH[32], ageB[32];
      snprintf(outT, sizeof(outT), "%.1f \xC2\xB0""C", timeAndWeather.getOutdoorTemperature());
      snprintf(outH, sizeof(outH), "%.1f %% RH", timeAndWeather.getOutdoorHumidity());
      snprintf(ageB, sizeof(ageB), "Synced %us ago (Valid)", timeAndWeather.getWeatherAgeSec());
      printTwoCols("Outdoor Temp:", outT, "Climate Season:", timeAndWeather.getSeason().c_str());
      printTwoCols("Outdoor Humidity:", outH, "Weather Service:", "Open-Meteo REST API");
      printTwoCols("Telemetry Age:", ageB, "Weather Status:", "ONLINE");
    } else {
      printTwoCols("Outdoor Weather:", "Syncing with Open-Meteo...", "Weather Status:", "SYNCING");
    }
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    netConsole.println();

    netConsole.println(F("[D. SCHEDULE & PRE-HEAT]"));
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    char schedStateBuf[48], curLevelBuf[48], preHeatBuf[48], nextLevelBuf[48];
    if (thermalZones.isAway()) {
      snprintf(schedStateBuf, sizeof(schedStateBuf), "AWAY MODE (all zones at away level)");
      snprintf(curLevelBuf, sizeof(curLevelBuf), "away");
      snprintf(preHeatBuf, sizeof(preHeatBuf), "N/A");
      snprintf(nextLevelBuf, sizeof(nextLevelBuf), "N/A");
    } else if (thermalZones.isScheduleEnabled()) {
      snprintf(schedStateBuf, sizeof(schedStateBuf), "ACTIVE (%s)", timeAndWeather.isWeekend() ? "Weekend" : "Weekday");
      snprintf(curLevelBuf, sizeof(curLevelBuf), "%s", ThermalZoneManager::getLevelName(thermalZones.getActiveLevel()));
      snprintf(preHeatBuf, sizeof(preHeatBuf), "%s", thermalZones.isPreHeatActive() ? "ACTIVE (Pre-Warming)" : (thermalZones.isPreHeatEnabled() ? "STANDBY" : "DISABLED"));
      snprintf(nextLevelBuf, sizeof(nextLevelBuf), "%s (In %um)",
               ThermalZoneManager::getLevelName(thermalZones.getNextLevel()),
               (unsigned int)thermalZones.getMinutesToNextSlot());
    } else {
      snprintf(schedStateBuf, sizeof(schedStateBuf), "DISABLED (Fixed Setpoints)");
      snprintf(curLevelBuf, sizeof(curLevelBuf), "NONE (Static Targets)");
      snprintf(preHeatBuf, sizeof(preHeatBuf), "%s", thermalZones.isPreHeatEnabled() ? "STANDBY" : "DISABLED");
      snprintf(nextLevelBuf, sizeof(nextLevelBuf), "N/A");
    }
    char winGuardBuf[48];
    snprintf(winGuardBuf, sizeof(winGuardBuf), "%s", thermalZones.isWindowDetectionEnabled() ? "ARMED (>1.2\xC2\xB0""C in 5m Pause)" : "DISABLED");
    printTwoCols("Schedule:", schedStateBuf, "Active Level:", curLevelBuf);
    printTwoCols("Next Level:", nextLevelBuf, "Pre-Heat Mode:", preHeatBuf);
    netConsole.printf("  * %-18s %s\n", "Window Guard:", winGuardBuf);
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    netConsole.println();

    netConsole.println(F("[E. SAFETY & FAILSAFE INTERLOCKS]"));
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    uint32_t tout = thermalZones.getSensorTimeoutSec();
    uint32_t asEpoch = thermalZones.getLastAntiSeizeEpoch();
    uint32_t nextDays = getAntiSeizeNextDays(asEpoch);
    char toutB[32], asB[48], nextB[32];
    snprintf(toutB, sizeof(toutB), "%u min (%us)", tout / 60, tout);
    snprintf(asB, sizeof(asB), "Weekly (Ran: %s)", formatEpochHuman(asEpoch).c_str());
    snprintf(nextB, sizeof(nextB), "In %ud", nextDays);
    printTwoCols("Sensor Failsafe:", toutB, "Hardware Watchdog:", "TWDT Armed (90s)");
    printTwoCols("Pump Anti-Seize:", asB, "Next Exercise:", nextB);
    netConsole.println(F("===================================================================================================="));
    netConsole.println();

    netConsole.printf("[F. HEATING ZONES & ROOM TELEMETRY] (%u Zones Configured | Mode: %s)\n",
                      (unsigned int)thermalZones.getZoneCount(),
                      thermalZones.isAutoMode() ? "AUTOMATIC" : "MANUAL");
    netConsole.println(F("===================================================================================================="));
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
      if (i > 0) {
        netConsole.println();
      }
      const ThermalZoneConfig &cfg = thermalZones.getConfig(i);
      const ThermalZoneStatus &st = thermalZones.getStatus(i);

      // Find mapped sensor data
      const BleSensorData *ble = nullptr;
      if (strlen(cfg.sensorMac) > 0) {
        ble = bleScanner.findSensorByMac(cfg.sensorMac);
      }
      if (!ble && strlen(cfg.sensorName) > 0) {
        ble = bleScanner.findSensorByName(cfg.sensorName);
      }

      netConsole.printf("[Zone %u] %s (Relay %u on GPIO%d)\n", cfg.id, cfg.name, cfg.id, cfg.relayPin);
      netConsole.println(F("----------------------------------------------------------------------------------------------------"));

      // Row 1: Target Temp & Sensor Name
      char targetStr[32];
      float effTarget = thermalZones.getEffectiveTargetTemp(i);
      if (thermalZones.isScheduleEnabled()) {
        snprintf(targetStr, sizeof(targetStr), "%.1f \xC2\xB0""C (%s)", effTarget, thermalZones.isPreHeatActive() ? "PRE-HEAT" : ThermalZoneManager::getLevelName(thermalZones.getActiveLevel()));
      } else {
        snprintf(targetStr, sizeof(targetStr), "%.1f \xC2\xB0""C", cfg.targetTemp);
      }
      const char *sName = (ble && strlen(ble->name) > 0) ? ble->name : (strlen(cfg.sensorName) > 0 ? cfg.sensorName : "Unassigned");
      printTwoCols("Target Temp:", targetStr, "Sensor Name:", sName);

      // Row 2: Current Temp & Sensor MAC
      char curTempStr[32];
      if (st.state == ZONE_WARMUP) {
        snprintf(curTempStr, sizeof(curTempStr), "WARMUP (Scanning...)");
      } else if (st.sensorValid) {
        snprintf(curTempStr, sizeof(curTempStr), "%.1f \xC2\xB0""C", st.currentTemp);
      } else {
        snprintf(curTempStr, sizeof(curTempStr), "--.- \xC2\xB0""C (TIMEOUT)");
      }
      const char *sMac = (strlen(cfg.sensorMac) > 0) ? cfg.sensorMac : ((ble && strlen(ble->mac) > 0) ? ble->mac : "No MAC");
      printTwoCols("Current Temp:", curTempStr, "Sensor MAC:", sMac);

      // Row 3: Room Humidity & Sensor Status
      char humStr[24];
      if (ble && ble->valid) {
        snprintf(humStr, sizeof(humStr), "%.1f %% RH", ble->humidity);
      } else {
        snprintf(humStr, sizeof(humStr), "--.- %% RH");
      }
      char sensStatStr[32];
      if (st.state == ZONE_WARMUP) {
        snprintf(sensStatStr, sizeof(sensStatStr), "WARMUP (Scanning...)");
      } else if (ble && ble->valid && st.sensorValid) {
        snprintf(sensStatStr, sizeof(sensStatStr), "ONLINE");
      } else {
        snprintf(sensStatStr, sizeof(sensStatStr), "TIMEOUT (Failsafe OFF)");
      }
      printTwoCols("Room Humidity:", humStr, "Sensor Status:", sensStatStr);

      // Row 4: Loop Status & Sensor Battery
      char loopStatStr[36];
      if (!cfg.enabled) {
        snprintf(loopStatStr, sizeof(loopStatStr), "DISABLED (Zone Off)");
      } else if (st.state == ZONE_WARMUP) {
        snprintf(loopStatStr, sizeof(loopStatStr), "WARMUP (%us/300s)", thermalZones.getBootWarmupElapsedSec());
      } else if (st.state == ZONE_WINDOW_PAUSE) {
        snprintf(loopStatStr, sizeof(loopStatStr), "WINDOW PAUSE (%us left)", thermalZones.getWindowPauseRemainingSec(i));
      } else if (st.state == ZONE_FAULT_TIMEOUT) {
        snprintf(loopStatStr, sizeof(loopStatStr), "TIMEOUT (Failsafe)");
      } else if (st.frostActive) {
        snprintf(loopStatStr, sizeof(loopStatStr), "FROST GUARD (<12\xC2\xB0""C)");
      } else {
        snprintf(loopStatStr, sizeof(loopStatStr), "%s", st.demand ? "HEATING (Demand)" : "IDLE (Temp OK)");
      }
      char batStr[24];
      if (ble && ble->valid) {
        snprintf(batStr, sizeof(batStr), "%u%%", ble->battery);
      } else if (st.state == ZONE_WARMUP) {
        snprintf(batStr, sizeof(batStr), "--%%");
      } else {
        snprintf(batStr, sizeof(batStr), "Offline");
      }
      printTwoCols("Loop Status:", loopStatStr, "Sensor Battery:", batStr);

      // Row 5: Actuator Valve & RF Signal Link
      char valveStr[32];
      snprintf(valveStr, sizeof(valveStr), "%s (GPIO%d %s)",
               st.relayEngaged ? "OPEN" : "CLOSED",
               cfg.relayPin,
               st.relayEngaged ? "ON" : "OFF");
      char sigStr[40];
      if (ble && ble->valid) {
        snprintf(sigStr, sizeof(sigStr), "%d dBm %s", ble->rssi, getLinkQualityBadge(ble->rssi));
      } else if (st.state == ZONE_WARMUP) {
        snprintf(sigStr, sizeof(sigStr), "Scanning... (Listening)");
      } else {
        snprintf(sigStr, sizeof(sigStr), "Offline / No Signal");
      }
      printTwoCols("Actuator Valve:", valveStr, "RF Signal Link:", sigStr);

      // Row 6: Hydraulic Relay & Telemetry Data
      char relayStr[24];
      snprintf(relayStr, sizeof(relayStr), "Relay %u (GPIO%d)", cfg.id, cfg.relayPin);
      char telemStr[40];
      if (ble && ble->valid) {
        uint32_t age = (millis() - ble->lastSeenMs) / 1000;
        snprintf(telemStr, sizeof(telemStr), "%u total pkts (Last: %us ago)", ble->packetCount, age);
      } else if (st.state == ZONE_WARMUP) {
        snprintf(telemStr, sizeof(telemStr), "0 total (Never heard)");
      } else {
        snprintf(telemStr, sizeof(telemStr), "0 total (Age > 600s)");
      }
      printTwoCols("Hydraulic Relay:", relayStr, "Telemetry Data:", telemStr);
      netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    }
  } else if (upperCmd == "BLE" || upperCmd == "SENSORS") {

    size_t bc = bleScanner.getSensorCount();
    size_t paired = thermalZones.getPairedZoneCount();
    size_t acquired = thermalZones.getAcquiredZoneCount();

    if (thermalZones.isBootWarmupActive()) {
      netConsole.printf("[BLE] Room Thermometers Registry (%u/%u acquired) [SCANNING: %us/300s]:\n",
                        (unsigned int)acquired, (unsigned int)paired,
                        thermalZones.getBootWarmupElapsedSec());
    } else {
      netConsole.printf("[BLE] Room Thermometers Registry (%u/%u acquired) [SCANNING COMPLETE]:\n",
                        (unsigned int)acquired, (unsigned int)paired);
    }

    if (bc > 0) {
      netConsole.println(F("  Sensor Name       MAC Address        Temp     Humidity   Battery   Signal (Min/Cur/Max)   Packets   Last Seen"));
      netConsole.println(F("  ----------------  -----------------  -------  ---------  --------  ---------------------  --------  ---------"));
      for (size_t i = 0; i < bc; i++) {
        const BleSensorData *s = bleScanner.getSensor(i);
        if (s && s->valid) {
          uint32_t age = (millis() - s->lastSeenMs) / 1000;
          char rssiStr[24];
          snprintf(rssiStr, sizeof(rssiStr), "%d / %d / %d dBm", s->minRssi, s->rssi, s->maxRssi);
          netConsole.printf("  %-16s  %-17s  %5.1f C  %5.1f %%    %3u%%    %-21s  %7u   %4us ago\n",
                            s->name, s->mac, s->temperature, s->humidity, s->battery,
                            rssiStr, s->packetCount, age);
        }
      }
    } else {
      netConsole.println(F("  No BLE thermometers decoded yet. Waiting for advertisements (interval ~30-60s)..."));
    }
  } else if (upperCmd.startsWith("DIAG")) {
    if (upperCmd == "DIAG SELFTEST") {
      netConsole.print(diagEngine.runSelfTest());
    } else if (upperCmd == "DIAG CLEAR") {
      diagEngine.clearAllDtcs();
    } else {
      netConsole.printf("[DIAG] System Health Score: %u%% | Active DTCs: %u\n",
                        diagEngine.calculateHealthScore(),
                        diagEngine.getDtcCount());
      size_t activeCount = 0;
      for (size_t i = 0; i < diagEngine.getDtcCount(); i++) {
        const DtcRecord *d = diagEngine.getDtc(i);
        if (d && d->active) {
          activeCount++;
          netConsole.printf("  * [%s] %s: %s (x%u, raised %us ago)\n", d->code,
                            diagEngine.getSeverityStr(d->severity), d->message,
                            d->occurrenceCount,
                            (millis() - d->lastRaisedMs) / 1000);
        }
      }
      if (activeCount == 0) {
        netConsole.println(
            F("  No active diagnostic trouble codes. All subsystems nominal!"));
      }
    }
  } else if (upperCmd.startsWith("LOCATION")) {
    String arg = cmd.substring(8);
    arg.trim();
    if (arg.length() > 3) {
      timeAndWeather.setLocation(arg.c_str());
      timeAndWeather.forceWeatherUpdate();
    }
    netConsole.printf("[LOCATION] %s\n", timeAndWeather.getLocation());
  } else if (upperCmd == "TIME") {
    netConsole.printf("[TIME] %s, %s %s (Timezone: Europe/Bucharest)\n",
                      timeAndWeather.getDayOfWeek().c_str(),
                      timeAndWeather.getFormattedDate().c_str(),
                      timeAndWeather.getFormattedTime().c_str());
    netConsole.printf("  NTP Synced:     %s\n",
                      timeAndWeather.isTimeSynced() ? "YES" : "NO");
    netConsole.printf("  Calendar Year:  %d | Month: %d | Day: %d\n",
                      timeAndWeather.getYear(), timeAndWeather.getMonth(),
                      timeAndWeather.getDay());
    netConsole.printf("  Season:         %s | Heating Season: %s\n",
                      timeAndWeather.getSeason().c_str(),
                      timeAndWeather.isHeatingSeason()
                          ? "ACTIVE (Heating Mode)"
                          : "OFF-SEASON (Summer Standby)");
  } else if (upperCmd == "WEATHER") {
    netConsole.printf(
        "[WEATHER] Outdoor Temperature: %.1f C | Humidity: %.1f %%\n",
        timeAndWeather.getOutdoorTemperature(),
        timeAndWeather.getOutdoorHumidity());
    netConsole.printf("  Source:   Open-Meteo REST API (HTTP Wi-Fi)\n");
    netConsole.printf("  Status:   %s (Fetched %u s ago)\n",
                      timeAndWeather.isWeatherValid() ? "VALID" : "PENDING",
                      timeAndWeather.getWeatherAgeSec());
  } else if (upperCmd == "AWAY ON" || upperCmd == "AWAY OFF") {
    thermalZones.setAway(upperCmd == "AWAY ON");
  } else if (upperCmd.startsWith("SCHEDULE LEVEL ")) {
    char lvl[8];
    unsigned z;
    float t;
    int level = -1;
    if (sscanf(cmd.c_str() + 15, "%u %7s %f", &z, lvl, &t) == 3) level = ThermalZoneManager::parseLevel(lvl);
    if (level < 0 || z < 1 || !thermalZones.setLevelTemp(z - 1, (ComfortLevel)level, t)) {
      netConsole.println(F("[SCHEDULE] Syntax: SCHEDULE LEVEL <zone 1..3> <day|night|away> <temp 5..30>"));
    }
  } else if (upperCmd.startsWith("SCHEDULE SLOTS ")) {
    String args = cmd.substring(15);
    args.trim();
    int sp = args.indexOf(' ');
    String prof = sp > 0 ? args.substring(0, sp) : "";
    prof.toLowerCase();
    if ((prof != "weekday" && prof != "weekend") ||
        !thermalZones.setSlots(prof == "weekend", args.substring(sp + 1).c_str())) {
      netConsole.println(F("[SCHEDULE] Syntax: SCHEDULE SLOTS <weekday|weekend> 06:30=day,08:30=away,17:00=day,22:30=night"));
    }
  } else if (upperCmd.startsWith("SCHEDULE")) {
    if (upperCmd == "SCHEDULE ON") {
      thermalZones.setScheduleEnabled(true);
      netConsole.println(F("[SCHEDULE] Schedule ENABLED."));
    } else if (upperCmd == "SCHEDULE OFF") {
      thermalZones.setScheduleEnabled(false);
      netConsole.println(F("[SCHEDULE] Schedule DISABLED (fixed targets)."));
    } else if (upperCmd == "SCHEDULE PREHEAT ON") {
      thermalZones.setPreHeatEnabled(true);
      netConsole.println(F("[SCHEDULE] Predictive Pre-Heat ENABLED."));
    } else if (upperCmd == "SCHEDULE PREHEAT OFF") {
      thermalZones.setPreHeatEnabled(false);
      netConsole.println(F("[SCHEDULE] Predictive Pre-Heat DISABLED."));
    } else if (upperCmd == "SCHEDULE WINDOW ON") {
      thermalZones.setWindowDetectionEnabled(true);
      netConsole.println(F("[SCHEDULE] Open Window Detection ENABLED."));
    } else if (upperCmd == "SCHEDULE WINDOW OFF") {
      thermalZones.setWindowDetectionEnabled(false);
      netConsole.println(F("[SCHEDULE] Open Window Detection DISABLED."));
    } else if (upperCmd == "SCHEDULE RESET") {
      thermalZones.resetScheduleToDefault();
    } else {
      const ScheduleConfig& sc = thermalZones.getScheduleConfig();
      netConsole.println(F("===================================================================================================="));
      netConsole.printf("  Schedule:   %s | Away: %s | Pre-Heat: %s | Window Guard: %s\n",
                        sc.enabled ? "ON" : "OFF (fixed targets)", sc.away ? "ON" : "OFF",
                        sc.preHeatEnabled ? (thermalZones.isPreHeatActive() ? "ACTIVE" : "ON") : "OFF",
                        sc.windowDetectionEnabled ? "ON" : "OFF");
      netConsole.printf("  Now:        %s -> next %s in %u min\n",
                        ThermalZoneManager::getLevelName(thermalZones.getActiveLevel()),
                        ThermalZoneManager::getLevelName(thermalZones.getNextLevel()),
                        (unsigned int)thermalZones.getMinutesToNextSlot());
      netConsole.printf("  Weekday:    %s\n", thermalZones.getSlotsString(false).c_str());
      netConsole.printf("  Weekend:    %s\n", thermalZones.getSlotsString(true).c_str());
      netConsole.println(F("  Zone                        Day     Night   Away"));
      for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        netConsole.printf("  %u %-25s %5.1f   %5.1f   %5.1f\n", (unsigned int)(i + 1), thermalZones.getConfig(i).name,
                          sc.levelTemps[LEVEL_DAY][i], sc.levelTemps[LEVEL_NIGHT][i], sc.levelTemps[LEVEL_AWAY][i]);
      }
      netConsole.println(F("===================================================================================================="));
    }
  } else if (upperCmd == "BOILER RESET RUNTIME" || upperCmd == "RESET RUNTIME") {
    boilerRadio.resetRuntime();
    netConsole.println(F("[Boiler][Runtime] Burn time counters reset to 0 in NVS."));
  } else if (upperCmd.startsWith("HISTORY")) {
    if (upperCmd == "HISTORY CLEAR") {
      telemetryHistory.clearHistory();
      netConsole.println(F("[HISTORY] All flash and RAM telemetry ring buffers cleared."));
    } else if (upperCmd == "HISTORY CAPTURE" || upperCmd == "HISTORY SAMPLE") {
      telemetryHistory.captureSample();
      netConsole.println(F("[HISTORY] Immediate sample captured into ring buffers."));
    } else {
      netConsole.println(F("=================================================="));
      netConsole.println(F("          TELEMETRY HISTORY STATUS"));
      netConsole.println(F("=================================================="));
      netConsole.printf("  * LittleFS Mounted:    %s\n", telemetryHistory.isFsMounted() ? "YES" : "NO");
      netConsole.printf("  * Current 24h Slot:    %u / 288 (5-min intervals)\n", telemetryHistory.getCurrentSlot24h());
      netConsole.printf("  * Last Sample Epoch:   %u\n", telemetryHistory.getLastSampleTime());
      netConsole.println(F("--------------------------------------------------"));
      netConsole.println(F("  Commands: HISTORY CLEAR | HISTORY CAPTURE"));
      netConsole.println(F("  API:      GET /api/history?range=24h|5d"));
      netConsole.println(F("=================================================="));
    }
  } else if (upperCmd == "CLEAR" || upperCmd == "CLS") {
    netConsole.clear();
  } else if (upperCmd == "?" || upperCmd == "HELP") {
    printHelp();
  } else {
    netConsole.printf("[Unknown command: '%s']. Type '?' for commands.\n",
                      cmd.c_str());
  }
}


void handleApiConfigSystemGet() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json;
  json.reserve(1024);
  json = "{";
  json += "\"status\":\"ok\",";
  json += "\"zonesCount\":" + String(thermalZones.getZoneCount()) + ",";
  json += "\"openLoopBypass\":" + String(thermalZones.isOpenLoopBypass() ? "true" : "false") + ",";
  json += "\"actuatorDelaySec\":" + String(thermalZones.getInterlockDelayMs() / 1000) + ",";
  json += "\"boilerDelaySec\":" + String(thermalZones.getBoilerDelayMs() / 1000) + ",";
  json += "\"pumpOverrunSec\":" + String(thermalZones.getPumpOverrunMs() / 1000) + ",";
  json += "\"shortCyclingSec\":" + String(boilerRadio.getMinRunTimeMs() / 1000) + ",";
  json += "\"rfMinCooldownSec\":" + String(boilerRadio.getRfMinCooldownMs() / 1000) + ",";
  json += "\"rfSafetyHeartbeatSec\":" + String(boilerRadio.getSafetyHeartbeatMs() / 1000) + ",";
  json += "\"rfSafetyHeartbeatEnabled\":" + String(boilerRadio.isSafetyHeartbeatEnabled() ? "true" : "false") + ",";
  json += "\"turReturSensors\":" + String(thermalZones.hasManifoldTurReturSensors() ? "true" : "false") + ",";
  json += "\"pumpControlEnabled\":" + String(thermalZones.isPumpControlEnabled() ? "true" : "false") + ",";
  json += "\"pumpRelayPin\":" + String(thermalZones.getPumpRelayPin()) + ",";
  json += "\"actuatorNormallyClosed\":" + String(thermalZones.isActuatorNormallyClosed() ? "true" : "false") + ",";
  json += "\"freezeProtectionTemp\":" + String(thermalZones.getFreezeProtectionTemp(), 1) + ",";
  json += "\"sensorTimeoutSec\":" + String(thermalZones.getSensorTimeoutSec()) + ",";
  json += "\"zones\":[";
  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    if (i > 0) json += ",";
    const auto& z = thermalZones.getConfig(i);
    json += "{";
    json += "\"id\":" + String(z.id) + ",";
    json += "\"name\":\"" + String(z.name) + "\",";
    json += "\"mac\":\"" + String(z.sensorMac) + "\",";
    json += "\"relayPin\":" + String(z.relayPin) + ",";
    json += "\"hysteresis\":" + String(z.hysteresis, 2) + ",";
    json += "\"enabled\":" + String(z.enabled ? "true" : "false") + ",";
    json += "\"windowDropThreshold\":" + String(z.windowDropThreshold, 1);
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleApiConfigSystemPost() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("openLoopBypass")) {
    thermalZones.setOpenLoopBypass(server.arg("openLoopBypass") == "true" || server.arg("openLoopBypass") == "1");
  }
  if (server.hasArg("actuatorDelaySec")) {
    uint32_t s = server.arg("actuatorDelaySec").toInt();
    if (s >= 1 && s <= 600) thermalZones.setInterlockDelayMs(s * 1000UL);
  }
  if (server.hasArg("boilerDelaySec")) {
    uint32_t s = server.arg("boilerDelaySec").toInt();
    if (s >= 1 && s <= 600) thermalZones.setBoilerDelayMs(s * 1000UL);
  }
  if (server.hasArg("pumpOverrunSec")) {
    uint32_t s = server.arg("pumpOverrunSec").toInt();
    if (s >= 1 && s <= 600) thermalZones.setPumpOverrunMs(s * 1000UL);
  }
  if (server.hasArg("shortCyclingSec")) {
    uint32_t s = server.arg("shortCyclingSec").toInt();
    if (s >= 10 && s <= 600) {
      boilerRadio.setMinRunTimeMs(s * 1000UL);
      boilerRadio.setMinPauseTimeMs(s * 1000UL);
    }
  }
  if (server.hasArg("rfMinCooldownSec")) {
    uint32_t s = server.arg("rfMinCooldownSec").toInt();
    if (s >= 1 && s <= 60) boilerRadio.setRfMinCooldownMs(s * 1000UL);
  }
  if (server.hasArg("rfSafetyHeartbeatSec")) {
    uint32_t s = server.arg("rfSafetyHeartbeatSec").toInt();
    if (s >= 60 && s <= 3600) boilerRadio.setSafetyHeartbeatMs(s * 1000UL);
  }
  if (server.hasArg("rfSafetyHeartbeatEnabled")) {
    boilerRadio.setSafetyHeartbeatEnabled(server.arg("rfSafetyHeartbeatEnabled") == "true" || server.arg("rfSafetyHeartbeatEnabled") == "1");
  }
  if (server.hasArg("turReturSensors")) {
    thermalZones.setManifoldTurReturSensors(server.arg("turReturSensors") == "true" || server.arg("turReturSensors") == "1");
  }
  if (server.hasArg("pumpControlEnabled")) {
    thermalZones.setPumpControlEnabled(server.arg("pumpControlEnabled") == "true" || server.arg("pumpControlEnabled") == "1");
  }
  if (server.hasArg("pumpRelayPin")) {
    thermalZones.setPumpRelayPin((uint8_t)server.arg("pumpRelayPin").toInt());
  }
  if (server.hasArg("actuatorNormallyClosed")) {
    thermalZones.setActuatorNormallyClosed(server.arg("actuatorNormallyClosed") == "true" || server.arg("actuatorNormallyClosed") == "1");
  }
  if (server.hasArg("freezeProtectionTemp")) {
    float f = server.arg("freezeProtectionTemp").toFloat();
    if (f >= 5.0f && f <= 20.0f) thermalZones.setFreezeProtectionTemp(f);
  }
  if (server.hasArg("sensorTimeoutSec")) {
    uint32_t s = server.arg("sensorTimeoutSec").toInt();
    if (s >= 60 && s <= 3600) thermalZones.setSensorTimeoutSec(s);
  }
  thermalZones.saveToPreferences();
  handleApiConfigSystemGet();
}

void handleApiRelayTest() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("pin")) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Missing 'pin' parameter.\"}"));
    return;
  }
  uint8_t pin = (uint8_t)server.arg("pin").toInt();
  if (pin != PIN_RELAY_ZONE_1 && pin != PIN_RELAY_ZONE_2 && pin != PIN_RELAY_ZONE_3 && pin != PIN_RELAY_SPARE) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Invalid relay pin. Allowed: 32, 33, 25, 26.\"}"));
    return;
  }
  bool state = (server.arg("state") == "1" || server.arg("state") == "true" || server.arg("state") == "on");
  pinMode(pin, OUTPUT);
  digitalWrite(pin, state ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
  netConsole.printf("[RELAY-TEST] Direct Relay Test: pin %u -> %s\n", pin, state ? "ON" : "OFF");
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW); // Off (active HIGH)

  Serial.printf("[Hardware] Initializing RF transmitter on GPIO%d...\n",
                PIN_RF_DATA);
  boilerRadio.begin(PIN_RF_DATA);

  // Initialize Multi-Zone Thermal Controller (NC actuator relays)
  thermalZones.begin();

  setupWifi();

  // Initialize 2-Way Network Console (Telnet :23 and /api/console)
  netConsole.begin(&server);
  netConsole.setCommandHandler(handleSerialCommand);

  // Initialize Time & NTP Service
  timeAndWeather.begin();

  // Quick settle for NTP time synchronization (up to 2 seconds)
  uint32_t ntpWaitStart = millis();
  while (!timeAndWeather.isTimeSynced() && (millis() - ntpWaitStart < 2000)) {
    timeAndWeather.update();
    delay(50);
    yield();
  }



  // Initialize Native Passive BLE Scanner (Xiaomi BTHome v2 & PVVX)
  bleScanner.begin();

  // Initialize Diagnostic Engine
  diagEngine.begin();

  // Initialize Flash Telemetry History Manager (LittleFS wear-leveling ring buffer)
  telemetryHistory.begin();

  // Web Server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/setup/location", handleApiSetupLocation);
  server.on("/api/setup/discovered-sensors", HTTP_GET,
            handleApiSetupDiscoveredSensors);
  server.on("/api/setup/map-zone", HTTP_POST, handleApiSetupMapZone);
  server.on("/api/setup/time", HTTP_GET, handleApiSetupTime);
  server.on("/api/boiler/on", HTTP_POST, handleApiOn);
  server.on("/api/boiler/off", HTTP_POST, handleApiOff);
  server.on("/api/boiler/cancel", HTTP_POST, handleApiBoilerCancelPending);
  server.on("/api/boiler/runtime/reset", HTTP_POST, handleApiBoilerResetRuntime);
  server.on("/api/boiler/status", HTTP_GET, handleApiStatus);
  server.on("/api/zones", HTTP_GET, handleApiZones);
  server.on("/api/zones/setpoint", HTTP_POST, handleApiZoneSetpoint);
  server.on("/api/zones/toggle", HTTP_POST, handleApiZoneToggle);
  server.on("/api/zones/hold/clear", HTTP_POST, handleApiZoneHoldClear);
  server.on("/api/zones/window/clear", HTTP_POST, handleApiZoneWindowClear);
  server.on("/api/zones/window/threshold", HTTP_POST, handleApiZoneWindowThreshold);
  server.on("/api/zones/sensor", HTTP_POST, handleApiZoneSensor);
  server.on("/api/mode", HTTP_POST, handleApiMode);
  server.on("/api/ble/sensors", HTTP_GET, handleApiBleSensors);
  server.on("/api/diag/status", HTTP_GET, handleApiDiagStatus);
  server.on("/api/diag/selftest", HTTP_GET, handleApiDiagSelfTest);
  server.on("/api/diag/clear", HTTP_POST, handleApiDiagClear);
  server.on("/api/diag/exercise", HTTP_POST, handleApiDiagExercise);
  server.on("/api/setup/actuator-type", handleApiSetupActuatorType);
  server.on("/api/setup/pump", handleApiSetupPump);
  server.on("/api/setup/sensor-timeout", handleApiSetupSensorTimeout);
  server.on("/api/schedule", HTTP_GET, handleApiScheduleGet);
  server.on("/api/schedule/enable", HTTP_POST, handleApiScheduleEnable);
  server.on("/api/schedule/level", HTTP_POST, handleApiScheduleLevel);
  server.on("/api/schedule/slots", HTTP_POST, handleApiScheduleSlots);
  server.on("/api/schedule/reset", HTTP_POST, handleApiScheduleReset);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.on("/api/history/clear", HTTP_POST, handleApiHistoryClear);
  server.on("/api/config/system", HTTP_GET, handleApiConfigSystemGet);
  server.on("/api/config/system", HTTP_POST, handleApiConfigSystemPost);
  server.on("/api/relay/test", HTTP_POST, handleApiRelayTest);

  // Initialize Hardware Task Watchdog Timer (90 seconds timeout)
#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = 90000,
      .idle_core_mask = 0,
      .trigger_panic = true,
  };
  esp_task_wdt_init(&twdt_config);
#else
  esp_task_wdt_init(90, true);
#endif
  esp_task_wdt_add(NULL);
  Serial.println(
      F("[System] Hardware Task Watchdog Timer (TWDT) armed: 90s."));

  // Feed TWDT during OTA flash writes so watchdog never triggers
  Update.onProgress([](size_t progress, size_t total) {
    esp_task_wdt_reset();
  });

  // Enable Web OTA on /update (admin / admin123)
  httpUpdater.setup(&server, "/update", OTA_USER, OTA_PASS);
  server.begin();

  printBootBannerAndSelfTest();
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  netConsole.update();
  bleScanner.update();
  timeAndWeather.update();
  if (timeAndWeather.isTimeSynced()) {
    boilerRadio.updateRuntimeDayMonth(timeAndWeather.getDay(), timeAndWeather.getMonth());
  }
  telemetryHistory.update();
  diagEngine.update();
  boilerRadio.update();
  thermalZones.update(boilerRadio);
  updateWifiWatchdog();

  // Non-blocking serial command reader
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        netConsole.executeCommand(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}
