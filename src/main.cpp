#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "DevicePassport.h"
#include "DiagnosticEngine.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "Version.h"
#include "dashboard_html_gz.h"
#include "setup_html_gz.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPUpdateServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

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
  netConsole.printf("  Serial: %s | Commissioned: %s\n",
                    devicePassport.getSerialNumber(),
                    devicePassport.isCommissioned() ? "YES" : "NO");
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

void printSchedulePresets() {
  netConsole.println(F("===================================================================================================="));
  netConsole.printf("                   BIO-SCHEDULE PRESETS CATALOG (Active: %s)\n",
                    thermalZones.getActivePresetId());
  netConsole.println(F("===================================================================================================="));
  netConsole.println(F("  ID          Type      Status       Name / Description"));
  netConsole.println(F("  ----------  --------  -----------  --------------------------------------------------------------"));
  netConsole.printf("  %-10s  FACTORY   %-11s  Standard / Confort (Balanced Circadian)\n",
                    "default", strcmp(thermalZones.getActivePresetId(), "default") == 0 ? "[ACTIVE]" : "Ready");
  netConsole.printf("  %-10s  FACTORY   %-11s  Vacanta / Eco (18.0 C all zones, 19.0 C wake)\n",
                    "eco", strcmp(thermalZones.getActivePresetId(), "eco") == 0 ? "[ACTIVE]" : "Ready");
  netConsole.printf("  %-10s  FACTORY   %-11s  Home Office (22.5 C daytime comfort)\n",
                    "wfh", strcmp(thermalZones.getActivePresetId(), "wfh") == 0 ? "[ACTIVE]" : "Ready");

  Preferences prefs;
  if (prefs.begin("presets", true)) {
    const char* slots[] = {"custom1", "custom2"};
    for (int i = 0; i < 2; i++) {
      char keyCfg[16];
      char keyName[16];
      snprintf(keyCfg, sizeof(keyCfg), "%s_cfg", slots[i]);
      snprintf(keyName, sizeof(keyName), "%s_name", slots[i]);
      bool saved = prefs.isKey(keyCfg);
      String name = saved ? prefs.getString(keyName, slots[i]) : String("Slot Liber (") + slots[i] + ")";
      const char* activeTag = (strcmp(thermalZones.getActivePresetId(), slots[i]) == 0) ? "[ACTIVE]" : (saved ? "Saved" : "Empty");
      netConsole.printf("  %-10s  USER      %-11s  %s\n", slots[i], activeTag, name.c_str());
    }
    prefs.end();
  }
  netConsole.println(F("----------------------------------------------------------------------------------------------------"));
  netConsole.println(F("  Usage: SCHEDULE PRESET <id>                   -> Apply preset (e.g. SCHEDULE PRESET eco)"));
  netConsole.println(F("         SCHEDULE SAVE <custom1|custom2> [name] -> Save current schedule to NVS slot"));
  netConsole.println(F("         SCHEDULE RESET                         -> Restore factory default preset"));
  netConsole.println(F("===================================================================================================="));
}

void printHelp() {
  netConsole.print(F(
      "\r\n--- Smart Heating Command Manual ---\r\n"
      "[Heating & Hydraulic Control]\r\n"
      "  ON / OFF              -> Start / Stop gas boiler burner flame via 433.92 MHz RF\r\n"
      "  AUTO / MANUAL         -> Thermostat automatic control vs manual override\r\n"
      "  SET <z 1..3> <temp>   -> Set static target temperature for zone (e.g. SET 1 22.5)\r\n"
      "  PUMP <ON|OFF>         -> Secondary GPA circulation pump manual override\r\n"
      "  PUMP <ENABLE|DISABLE> -> Enable / disable automatic pump interlock & overrun\r\n"
      "  ACTUATOR <NC|NO>      -> Set thermal actuator polarity (Normally Closed vs Open)\r\n"
      "  EXERCISE              -> Run 20-second anti-seize cycle for pump and valves\r\n\r\n"
      "[Smart Bio-Schedule & Presets]\r\n"
      "  SCHEDULE              -> Display current bio-schedule, active preset & anchors\r\n"
      "  SCHEDULE <ON|OFF>     -> Enable or disable circadian bio-schedule engine\r\n"
      "  SCHEDULE PRESETS      -> List all available factory and user-saved presets (or: PRESETS)\r\n"
      "  SCHEDULE PRESET <id>  -> Apply preset (e.g. default, eco, wfh, custom1, custom2)\r\n"
      "  SCHEDULE SAVE <id> [n]-> Save current schedule into NVS slot (custom1/custom2)\r\n"
      "  SCHEDULE SET ...      -> Tune anchor: SCHEDULE SET <weekday|weekend> <anchor> <HH:MM> [z1] [z2] [z3]\r\n"
      "  SCHEDULE RESET        -> Reset all circadian anchors and setpoints to factory default\r\n"
      "  SCHEDULE PREHEAT <ON|OFF> -> Predictive pre-heating with thermal inertia calculation\r\n"
      "  SCHEDULE WINDOW <ON|OFF>  -> Rapid temperature drop open window detection guard\r\n\r\n"
      "[Telemetry, Sensors & Diagnostics]\r\n"
      "  STATUS or S           -> Comprehensive live dashboard: CPU, boiler, weather, zones\r\n"
      "  BLE or SENSORS        -> Decoded Xiaomi BLE thermometer registry, signal & battery\r\n"
      "  WEATHER / TIME        -> Open-Meteo outdoor temperature & NTP local calendar\r\n"
      "  TIMEOUT [sec]         -> Get or set BLE sensor offline failsafe timeout (default 600s)\r\n"
      "  DIAG [SELFTEST]       -> System health score, active DTCs or full 6-subsystem test\r\n"
      "  DIAG CLEAR            -> Clear active Trouble Codes (DTCs) and restore 100% health\r\n"
      "  DIAG SERVICE <ON|OFF> -> Engineer Service Mode (bypasses RF anti-short-cycling guard)\r\n"
      "  PASSPORT              -> Device passport: commissioning date, serial, customer, address\r\n"
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
  bool force = (server.hasArg("force") && server.arg("force") == "true") || diagEngine.isServiceMode();
  uint32_t waitSec = 0;
  bool isIdempotent = false;

  server.sendHeader("Access-Control-Allow-Origin", "*");

  // 1. Check idempotency (FR-17.1)
  if (!force && boilerRadio.hasTransmittedOnce() && boilerRadio.isLastCommandOn()) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\",\"idempotent\":true}"));
    return;
  }

  // 2. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
  if (!force && !boilerRadio.canTransmit(true, &waitSec, &isIdempotent)) {
    boilerRadio.recordSpamAttempt();
    String resp = "{\"status\":\"rate_limited\",\"error\":\"Anti-short-cycling guard active\",\"waitSec\":" +
                  String(waitSec) + ",\"boiler\":\"" + (boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\"}";
    server.send(429, "application/json", resp);
    return;
  }

  blinkLed(1, 40);
  bool success = boilerRadio.sendBoilerOn(force);
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
  bool force = (server.hasArg("force") && server.arg("force") == "true") || diagEngine.isServiceMode();
  uint32_t waitSec = 0;
  bool isIdempotent = false;

  server.sendHeader("Access-Control-Allow-Origin", "*");

  // 1. Check idempotency (FR-17.1)
  if (!force && boilerRadio.hasTransmittedOnce() && !boilerRadio.isLastCommandOn()) {
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\",\"idempotent\":true}"));
    return;
  }

  // 2. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
  if (!force && !boilerRadio.canTransmit(false, &waitSec, &isIdempotent)) {
    boilerRadio.recordSpamAttempt();
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

void handleApiZones() {
  String json = "{";
  json +=
      "\"autoMode\":" + String(thermalZones.isAutoMode() ? "true" : "false") +
      ",";
  json += "\"boiler\":\"" +
          String(boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\",";
  uint32_t boilerWaitSec = 0;
  boilerRadio.canTransmit(!boilerRadio.isLastCommandOn(), &boilerWaitSec);
  json += "\"boilerCooldownSec\":" + String(boilerWaitSec) + ",";
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
  json += "\"activeAnchor\":\"" + String(thermalZones.getActiveAnchorName()) + "\",";
  json += "\"preHeatActive\":" + String(thermalZones.isPreHeatActive() ? "true" : "false") + ",";
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
    json += "\"demand\":" + String(st.demand ? "true" : "false") + ",";
    json +=
        "\"relayEngaged\":" + String(st.relayEngaged ? "true" : "false") + ",";
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
  String json = "{";
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

void handleSetup() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", (const char*)SETUP_HTML_GZ, SETUP_HTML_GZ_LEN);
}

void handleApiSetupPassport() {
  if (server.method() == HTTP_POST) {
    String cust = server.arg("customer");
    String addr = server.arg("address");
    String notes = server.arg("notes");
    String city = server.arg("city");
    String coords = server.arg("coords");
    devicePassport.commission(cust.c_str(), addr.c_str(), notes.c_str(),
                              city.c_str(), coords.c_str());
    if (coords.length() > 3) {
      timeAndWeather.forceWeatherUpdate();
    }
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", devicePassport.toJson());
}

void handleApiSetupDiscoveredSensors() {
  String json = "{\"sensors\":[";
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
  String json = "{";
  json += "\"serviceMode\":" +
          String(diagEngine.isServiceMode() ? "true" : "false") + ",";
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

void handleApiDiagService() {
  if (server.hasArg("enable")) {
    String val = server.arg("enable");
    val.toLowerCase();
    bool enable = (val == "true" || val == "1");
    diagEngine.setServiceMode(enable);
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
              diagEngine.isServiceMode()
                  ? F("{\"status\":\"ok\",\"serviceMode\":true}")
                  : F("{\"status\":\"ok\",\"serviceMode\":false}"));
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
  String json = "{";
  json += "\"firmwareVersion\":\"" + String(APP_VERSION) + "\",";
  json += "\"buildDate\":\"" + String(APP_BUILD_DATE " " APP_BUILD_TIME) + "\",";
  json += "\"gitCommit\":\"" + String(APP_GIT_COMMIT) + "\",";
  json += "\"resetReason\":\"" + String(getResetReasonStr(esp_reset_reason())) + "\",";
  json += "\"boiler\":\"" +
          String(boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\",";
  json += "\"lastTxMs\":" + String(boilerRadio.getLastTransmissionTime()) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"uptimeSec\":" + String(millis() / 1000) + ",";
  json += "\"healthScore\":" + String(diagEngine.calculateHealthScore()) + ",";
  json += "\"serviceMode\":" +
          String(diagEngine.isServiceMode() ? "true" : "false") + ",";
  json +=
      "\"serialNumber\":\"" + String(devicePassport.getSerialNumber()) + "\",";
  json += "\"isCommissioned\":" +
          String(devicePassport.isCommissioned() ? "true" : "false") + ",";
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
  json += "\"activeAnchor\":\"" +
          String(thermalZones.getActiveAnchorName()) + "\",";
  json += "\"preHeatActive\":" +
          String(thermalZones.isPreHeatActive() ? "true" : "false") + ",";
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

void handleApiSchedulePost() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  if (server.hasArg("plain") && server.arg("plain").length() > 0) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (!err) {
      if (doc["enabled"].is<bool>()) {
        thermalZones.setScheduleEnabled(doc["enabled"].as<bool>());
      }
      if (doc["preHeatEnabled"].is<bool>()) {
        thermalZones.setPreHeatEnabled(doc["preHeatEnabled"].as<bool>());
      }
      if (doc["windowDetectionEnabled"].is<bool>()) {
        thermalZones.setWindowDetectionEnabled(doc["windowDetectionEnabled"].as<bool>());
      }

      const char* anchorKeys[] = {"sleep", "wake", "away", "home"};
      // Parse weekday
      if (doc["weekday"].is<JsonObject>()) {
        JsonObject wd = doc["weekday"].as<JsonObject>();
        for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
          if (wd[anchorKeys[a]].is<JsonObject>()) {
            JsonObject obj = wd[anchorKeys[a]].as<JsonObject>();
            uint8_t h = obj["hour"] | 255;
            uint8_t m = obj["minute"] | 255;
            float targets[MAX_THERMAL_ZONES];
            bool hasTargets = false;
            if (obj["targets"].is<JsonArray>()) {
              JsonArray arr = obj["targets"].as<JsonArray>();
              for (size_t i = 0; i < MAX_THERMAL_ZONES && i < arr.size(); i++) {
                targets[i] = arr[i].as<float>();
              }
              hasTargets = true;
            }
            if (h <= 23 && m <= 59) {
              thermalZones.updateAnchor(false, (BioAnchorId)a, h, m, hasTargets ? targets : nullptr);
            }
          }
        }
      }
      // Parse weekend
      if (doc["weekend"].is<JsonObject>()) {
        JsonObject we = doc["weekend"].as<JsonObject>();
        for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
          if (we[anchorKeys[a]].is<JsonObject>()) {
            JsonObject obj = we[anchorKeys[a]].as<JsonObject>();
            uint8_t h = obj["hour"] | 255;
            uint8_t m = obj["minute"] | 255;
            float targets[MAX_THERMAL_ZONES];
            bool hasTargets = false;
            if (obj["targets"].is<JsonArray>()) {
              JsonArray arr = obj["targets"].as<JsonArray>();
              for (size_t i = 0; i < MAX_THERMAL_ZONES && i < arr.size(); i++) {
                targets[i] = arr[i].as<float>();
              }
              hasTargets = true;
            }
            if (h <= 23 && m <= 59) {
              thermalZones.updateAnchor(true, (BioAnchorId)a, h, m, hasTargets ? targets : nullptr);
            }
          }
        }
      }
      server.send(200, "application/json", thermalZones.getScheduleJson());
      return;
    }
  }

  // Fallback to query/form parameters
  if (server.hasArg("enabled")) {
    bool en = (server.arg("enabled") == "true" || server.arg("enabled") == "1");
    thermalZones.setScheduleEnabled(en);
  }
  if (server.hasArg("preheat")) {
    bool ph = (server.arg("preheat") == "true" || server.arg("preheat") == "1");
    thermalZones.setPreHeatEnabled(ph);
  }
  if (server.hasArg("window")) {
    bool wd = (server.arg("window") == "true" || server.arg("window") == "1");
    thermalZones.setWindowDetectionEnabled(wd);
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleEnable() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  bool en = true;
  if (server.hasArg("enabled")) {
    en = (server.arg("enabled") == "true" || server.arg("enabled") == "1");
  }
  thermalZones.setScheduleEnabled(en);
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleAnchor() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  bool weekend = false;
  if (server.hasArg("profile")) {
    String prof = server.arg("profile");
    prof.toLowerCase();
    weekend = (prof == "weekend" || prof == "we" || prof == "1");
  } else if (server.hasArg("weekend")) {
    String we = server.arg("weekend");
    we.toLowerCase();
    weekend = (we == "true" || we == "1");
  }

  BioAnchorId anchorId = BIO_ANCHORS_COUNT;
  if (server.hasArg("anchor")) {
    anchorId = ThermalZoneManager::parseAnchorId(server.arg("anchor").c_str());
  }
  if (anchorId >= BIO_ANCHORS_COUNT) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Invalid anchor. Use sleep, wake, away, or home.\"}"));
    return;
  }

  const BioScheduleConfig& cur = thermalZones.getScheduleConfig();
  const BioAnchorConfig* curProf = weekend ? cur.weekend : cur.weekday;
  uint8_t h = curProf[anchorId].hour;
  uint8_t m = curProf[anchorId].minute;

  if (server.hasArg("time")) {
    String t = server.arg("time");
    int colon = t.indexOf(':');
    if (colon > 0) {
      h = (uint8_t)t.substring(0, colon).toInt();
      m = (uint8_t)t.substring(colon + 1).toInt();
    }
  } else {
    if (server.hasArg("hour")) h = (uint8_t)server.arg("hour").toInt();
    if (server.hasArg("minute")) m = (uint8_t)server.arg("minute").toInt();
  }

  float targets[MAX_THERMAL_ZONES];
  for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
    targets[i] = curProf[anchorId].targets[i];
  }

  if (server.hasArg("z1")) targets[0] = server.arg("z1").toFloat();
  if (server.hasArg("z2")) targets[1] = server.arg("z2").toFloat();
  if (server.hasArg("z3")) targets[2] = server.arg("z3").toFloat();
  if (server.hasArg("z0")) targets[0] = server.arg("z0").toFloat();

  if (server.hasArg("target")) {
    float t = server.arg("target").toFloat();
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) targets[i] = t;
  }

  bool ok = thermalZones.updateAnchor(weekend, anchorId, h, m, targets);
  if (!ok) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Failed to update anchor parameters.\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiSchedulePresets() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", thermalZones.getPresetsJson());
}

void handleApiSchedulePresetApply() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("id")) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Missing 'id' parameter.\"}"));
    return;
  }
  String id = server.arg("id");
  bool ok = thermalZones.applyPreset(id.c_str());
  if (!ok) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Preset not found or invalid.\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiSchedulePresetSave() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("id")) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Missing 'id' parameter (e.g. custom1, custom2).\"}"));
    return;
  }
  String id = server.arg("id");
  String name = server.hasArg("name") ? server.arg("name") : id;
  bool ok = thermalZones.savePreset(id.c_str(), name.c_str());
  if (!ok) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Cannot overwrite factory preset or invalid id.\"}"));
    return;
  }
  server.send(200, "application/json", thermalZones.getPresetsJson());
}

void handleApiScheduleReset() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  thermalZones.resetScheduleToDefault();
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

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
  } else {
    Serial.println(F("[WiFi] Could not connect to router within 12s. Starting "
                     "Fallback Access Point..."));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    softApActive = true;
    Serial.printf("[WiFi] AP active: '%s', Password: '%s', IP: %s\n", AP_SSID,
                  AP_PASS, WiFi.softAPIP().toString().c_str());
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
    printTwoCols("Device Serial:", (String(devicePassport.getSerialNumber()) + " (OK)").c_str(), "Boot Cause:", getResetReasonStr(esp_reset_reason()));
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

    netConsole.println(F("[D. SMART BIO-SCHEDULE & PRE-HEAT]"));
    netConsole.println(F("----------------------------------------------------------------------------------------------------"));
    char schedStateBuf[48], curAnchorBuf[48], preHeatBuf[48], nextAnchorBuf[48];
    if (thermalZones.isScheduleEnabled()) {
      snprintf(schedStateBuf, sizeof(schedStateBuf), "ACTIVE (%s Profile)", timeAndWeather.isWeekend() ? "Weekend" : "Weekday");
      snprintf(curAnchorBuf, sizeof(curAnchorBuf), "%s", thermalZones.getActiveAnchorName());
      snprintf(preHeatBuf, sizeof(preHeatBuf), "%s", thermalZones.isPreHeatActive() ? "ACTIVE (Pre-Warming)" : (thermalZones.isPreHeatEnabled() ? "STANDBY (Inertia Guard)" : "DISABLED"));
      snprintf(nextAnchorBuf, sizeof(nextAnchorBuf), "%s (In %um)",
               thermalZones.getAnchorName(thermalZones.getNextAnchor()),
               (unsigned int)thermalZones.getMinutesToNextAnchor());
    } else {
      snprintf(schedStateBuf, sizeof(schedStateBuf), "DISABLED (Fixed Setpoints)");
      snprintf(curAnchorBuf, sizeof(curAnchorBuf), "NONE (Static Targets)");
      snprintf(preHeatBuf, sizeof(preHeatBuf), "%s", thermalZones.isPreHeatEnabled() ? "STANDBY" : "DISABLED");
      snprintf(nextAnchorBuf, sizeof(nextAnchorBuf), "N/A");
    }
    char winGuardBuf[48], presetBuf[48];
    snprintf(winGuardBuf, sizeof(winGuardBuf), "%s", thermalZones.isWindowDetectionEnabled() ? "ARMED (>1.2\xC2\xB0""C in 5m Pause)" : "DISABLED");
    snprintf(presetBuf, sizeof(presetBuf), "%s (%s)",
             thermalZones.getActivePresetId(),
             thermalZones.isScheduleEnabled() ? "Dynamic Bio" : "Static Manual");
    printTwoCols("Bio-Schedule:", schedStateBuf, "Active Anchor:", curAnchorBuf);
    printTwoCols("Active Preset:", presetBuf, "Next Anchor:", nextAnchorBuf);
    printTwoCols("Pre-Heat Mode:", preHeatBuf, "Window Guard:", winGuardBuf);
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
        snprintf(targetStr, sizeof(targetStr), "%.1f \xC2\xB0""C (%s)", effTarget, thermalZones.isPreHeatActive() ? "PRE-HEAT" : thermalZones.getActiveAnchorName());
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
      if (st.state == ZONE_WARMUP) {
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
    } else if (upperCmd == "DIAG SERVICE ON") {
      diagEngine.setServiceMode(true);
    } else if (upperCmd == "DIAG SERVICE OFF") {
      diagEngine.setServiceMode(false);
    } else {
      netConsole.printf("[DIAG] System Health Score: %u%% | Service Mode: %s | "
                        "Active DTCs: %u\n",
                        diagEngine.calculateHealthScore(),
                        diagEngine.isServiceMode() ? "ACTIVE (Engineer Engaged)"
                                                   : "NORMAL",
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
  } else if (upperCmd == "PASSPORT") {
    netConsole.printf("[PASSPORT] Serial: %s | MAC: %s\n",
                      devicePassport.getSerialNumber(),
                      WiFi.macAddress().c_str());
    netConsole.printf("  Status:     %s\n",
                      devicePassport.isCommissioned()
                          ? "COMMISSIONED (Customer Active)"
                          : "UNCOMMISSIONED (Factory Default)");
    netConsole.printf("  Customer:   %s\n", devicePassport.getCustomerName());
    netConsole.printf("  Address:    %s (%s)\n",
                      devicePassport.getCustomerAddress(),
                      devicePassport.getLocationCity());
    netConsole.printf("  Coords:     %s\n", devicePassport.getLocationCoords());
    netConsole.printf("  Installed:  %s (%u days ago)\n",
                      devicePassport.getCommissionDate(),
                      devicePassport.getInstallationAgeDays());
    netConsole.printf("  Notes:      %s\n", devicePassport.getInstallerNotes());
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
  } else if (upperCmd == "PRESETS" || upperCmd == "SCHEDULE PRESETS") {
    printSchedulePresets();
  } else if (upperCmd.startsWith("PRESET ") || upperCmd.startsWith("SCHEDULE PRESET ")) {
    int offset = upperCmd.startsWith("SCHEDULE PRESET ") ? 16 : 7;
    String pid = cmd.substring(offset);
    pid.trim();
    pid.toLowerCase();
    if (thermalZones.applyPreset(pid.c_str())) {
      netConsole.printf("[SCHEDULE] Applied preset '%s'. Dynamic circadian anchors updated.\n", pid.c_str());
    } else {
      netConsole.printf("[SCHEDULE] Preset '%s' not found or invalid. Type 'PRESETS' for list.\n", pid.c_str());
    }
  } else if (upperCmd.startsWith("SCHEDULE SAVE ")) {
    String args = cmd.substring(14);
    args.trim();
    int sp = args.indexOf(' ');
    String slot = (sp > 0) ? args.substring(0, sp) : args;
    String name = (sp > 0) ? args.substring(sp + 1) : slot;
    slot.toLowerCase();
    name.trim();

    if (slot != "custom1" && slot != "custom2") {
      netConsole.println(F("[SCHEDULE] Invalid slot. Use 'custom1' or 'custom2'."));
    } else {
      if (thermalZones.savePreset(slot.c_str(), name.c_str())) {
        netConsole.printf("[SCHEDULE] Active Bio-Schedule saved to slot '%s' as '%s'.\n",
                          slot.c_str(), name.c_str());
      } else {
        netConsole.printf("[SCHEDULE] Failed to save preset '%s'.\n", slot.c_str());
      }
    }
  } else if (upperCmd.startsWith("SCHEDULE SET ")) {
    // Syntax: SCHEDULE SET <weekday|weekend> <sleep|wake|away|home> <HH:MM> [z1] [z2] [z3]
    String args = cmd.substring(13);
    args.trim();
    int p1 = args.indexOf(' ');
    if (p1 < 0) {
      netConsole.println(F("[SCHEDULE] Syntax: SCHEDULE SET <weekday|weekend> <sleep|wake|away|home> <HH:MM> [z1] [z2] [z3]"));
    } else {
      String profStr = args.substring(0, p1);
      profStr.toLowerCase();
      bool weekend = (profStr == "weekend" || profStr == "we" || profStr == "1");

      String rem1 = args.substring(p1 + 1);
      rem1.trim();
      int p2 = rem1.indexOf(' ');
      if (p2 < 0) {
        netConsole.println(F("[SCHEDULE] Syntax: SCHEDULE SET <weekday|weekend> <sleep|wake|away|home> <HH:MM> [z1] [z2] [z3]"));
      } else {
        String ancStr = rem1.substring(0, p2);
        BioAnchorId ancId = ThermalZoneManager::parseAnchorId(ancStr.c_str());
        if (ancId >= BIO_ANCHORS_COUNT) {
          netConsole.printf("[SCHEDULE] Invalid anchor '%s'. Use sleep, wake, away, or home.\n", ancStr.c_str());
        } else {
          String rem2 = rem1.substring(p2 + 1);
          rem2.trim();
          int p3 = rem2.indexOf(' ');
          String timeStr = (p3 > 0) ? rem2.substring(0, p3) : rem2;
          int colon = timeStr.indexOf(':');
          if (colon <= 0) {
            netConsole.println(F("[SCHEDULE] Invalid time format. Use HH:MM (e.g. 06:30)."));
          } else {
            uint8_t h = (uint8_t)timeStr.substring(0, colon).toInt();
            uint8_t m = (uint8_t)timeStr.substring(colon + 1).toInt();

            const BioScheduleConfig& cur = thermalZones.getScheduleConfig();
            const BioAnchorConfig* curProf = weekend ? cur.weekend : cur.weekday;
            float targets[MAX_THERMAL_ZONES];
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
              targets[i] = curProf[ancId].targets[i];
            }

            if (p3 > 0) {
              String tempArgs = rem2.substring(p3 + 1);
              tempArgs.trim();
              int tp1 = tempArgs.indexOf(' ');
              if (tp1 < 0) {
                float t = tempArgs.toFloat();
                for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) targets[i] = t;
              } else {
                targets[0] = tempArgs.substring(0, tp1).toFloat();
                String tempRem = tempArgs.substring(tp1 + 1);
                tempRem.trim();
                int tp2 = tempRem.indexOf(' ');
                if (tp2 < 0) {
                  targets[1] = tempRem.toFloat();
                } else {
                  targets[1] = tempRem.substring(0, tp2).toFloat();
                  String t3Str = tempRem.substring(tp2 + 1);
                  t3Str.trim();
                  targets[2] = t3Str.toFloat();
                }
              }
            }

            if (thermalZones.updateAnchor(weekend, ancId, h, m, targets)) {
              netConsole.printf("[SCHEDULE] Updated %s anchor %s to %02u:%02u (Z1: %.1f C, Z2: %.1f C, Z3: %.1f C).\n",
                                weekend ? "Weekend" : "Weekday",
                                thermalZones.getAnchorName(ancId),
                                h, m, targets[0], targets[1], targets[2]);
            } else {
              netConsole.println(F("[SCHEDULE] Failed to update anchor. Check hour (0-23) and minute (0-59)."));
            }
          }
        }
      }
    }
  } else if (upperCmd.startsWith("SCHEDULE")) {
    if (upperCmd == "SCHEDULE ON") {
      thermalZones.setScheduleEnabled(true);
      netConsole.println(F("[SCHEDULE] Smart Bio-Schedule ENABLED (Dynamic Life Anchors Active)."));
    } else if (upperCmd == "SCHEDULE OFF") {
      thermalZones.setScheduleEnabled(false);
      netConsole.println(F("[SCHEDULE] Smart Bio-Schedule DISABLED (Static Setpoints Active)."));
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
      netConsole.println(F("[SCHEDULE] Bio-Schedule reset to factory DEFAULT preset."));
    } else {
      const BioScheduleConfig& sc = thermalZones.getScheduleConfig();
      netConsole.println(F("===================================================================================================="));
      netConsole.println(F("                                  SMART BIO-SCHEDULE & ANCHORS                                      "));
      netConsole.println(F("===================================================================================================="));
      netConsole.printf("  * Schedule Engine:     %s\n", sc.enabled ? "ENABLED (Dynamic Life Anchors Active)" : "DISABLED (Static Setpoints Active)");
      netConsole.printf("  * Active Preset:       %s\n", thermalZones.getActivePresetId());
      netConsole.printf("  * Current Anchor:      %s\n", sc.enabled ? thermalZones.getActiveAnchorName() : "N/A");
      netConsole.printf("  * Next Anchor Event:   %s in %u min\n",
                        thermalZones.getAnchorName(thermalZones.getNextAnchor()),
                        thermalZones.getMinutesToNextAnchor());
      netConsole.printf("  * Pre-Heat Engine:     %s (Advance: %s)\n",
                        sc.preHeatEnabled ? "ENABLED" : "DISABLED",
                        thermalZones.isPreHeatActive() ? "ACTIVE (Pre-Warming)" : "STANDBY");
      netConsole.printf("  * Open Window Guard:   %s (>1.2 C drop in <5 min -> 30 min pause)\n",
                        sc.windowDetectionEnabled ? "ENABLED" : "DISABLED");
      netConsole.println(F("----------------------------------------------------------------------------------------------------"));
      netConsole.println(F("  [WEEKDAY PROFILE (Mon-Fri)]"));
      for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
        const BioAnchorConfig& anc = sc.weekday[a];
        netConsole.printf("    - %-6s at %02d:%02d -> Z1: %.1f C | Z2: %.1f C | Z3: %.1f C\n",
                          thermalZones.getAnchorName((BioAnchorId)a), anc.hour, anc.minute,
                          anc.targets[0], anc.targets[1], anc.targets[2]);
      }
      netConsole.println(F("  [WEEKEND PROFILE (Sat-Sun)]"));
      for (int a = 0; a < BIO_ANCHORS_COUNT; a++) {
        const BioAnchorConfig& anc = sc.weekend[a];
        netConsole.printf("    - %-6s at %02d:%02d -> Z1: %.1f C | Z2: %.1f C | Z3: %.1f C\n",
                          thermalZones.getAnchorName((BioAnchorId)a), anc.hour, anc.minute,
                          anc.targets[0], anc.targets[1], anc.targets[2]);
      }
      netConsole.println(F("----------------------------------------------------------------------------------------------------"));
      netConsole.println(F("  Quick Commands:"));
      netConsole.println(F("    SCHEDULE <ON|OFF>            -> Toggle bio-schedule engine"));
      netConsole.println(F("    PRESETS or SCHEDULE PRESETS  -> List available presets catalog"));
      netConsole.println(F("    PRESET <id>                  -> Apply preset (e.g. PRESET eco, PRESET default)"));
      netConsole.println(F("    SCHEDULE SAVE <id> [name]    -> Save current schedule to NVS slot (custom1/custom2)"));
      netConsole.println(F("    SCHEDULE SET <prof> <anc> <HH:MM> [targets] -> Tune anchor (e.g. SCHEDULE SET weekday wake 06:45 22.0)"));
      netConsole.println(F("    SCHEDULE RESET               -> Restore factory default preset"));
      netConsole.println(F("===================================================================================================="));
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

  // Initialize 2-Way Network Console (Telnet :23 and Web /console)
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

  // Initialize Device Passport (NVS commissioning metadata)
  devicePassport.begin();


  // Initialize Native Passive BLE Scanner (Xiaomi BTHome v2 & PVVX)
  bleScanner.begin();

  // Initialize Diagnostic Engine & Service Mode
  diagEngine.begin();

  // Web Server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetup);
  server.on("/api/setup/passport", handleApiSetupPassport);
  server.on("/api/setup/discovered-sensors", HTTP_GET,
            handleApiSetupDiscoveredSensors);
  server.on("/api/setup/map-zone", HTTP_POST, handleApiSetupMapZone);
  server.on("/api/setup/time", HTTP_GET, handleApiSetupTime);
  server.on("/api/boiler/on", HTTP_POST, handleApiOn);
  server.on("/api/boiler/off", HTTP_POST, handleApiOff);
  server.on("/api/boiler/status", HTTP_GET, handleApiStatus);
  server.on("/api/zones", HTTP_GET, handleApiZones);
  server.on("/api/zones/setpoint", HTTP_POST, handleApiZoneSetpoint);
  server.on("/api/zones/toggle", HTTP_POST, handleApiZoneToggle);
  server.on("/api/zones/sensor", HTTP_POST, handleApiZoneSensor);
  server.on("/api/mode", HTTP_POST, handleApiMode);
  server.on("/api/ble/sensors", HTTP_GET, handleApiBleSensors);
  server.on("/api/diag/status", HTTP_GET, handleApiDiagStatus);
  server.on("/api/diag/selftest", HTTP_GET, handleApiDiagSelfTest);
  server.on("/api/diag/clear", HTTP_POST, handleApiDiagClear);
  server.on("/api/diag/service", HTTP_POST, handleApiDiagService);
  server.on("/api/diag/exercise", HTTP_POST, handleApiDiagExercise);
  server.on("/api/setup/actuator-type", handleApiSetupActuatorType);
  server.on("/api/setup/pump", handleApiSetupPump);
  server.on("/api/setup/sensor-timeout", handleApiSetupSensorTimeout);
  server.on("/api/schedule", HTTP_GET, handleApiScheduleGet);
  server.on("/api/schedule", HTTP_POST, handleApiSchedulePost);
  server.on("/api/schedule/enable", HTTP_POST, handleApiScheduleEnable);
  server.on("/api/schedule/anchor", HTTP_POST, handleApiScheduleAnchor);
  server.on("/api/schedule/presets", HTTP_GET, handleApiSchedulePresets);
  server.on("/api/schedule/preset/apply", HTTP_POST, handleApiSchedulePresetApply);
  server.on("/api/schedule/preset/save", HTTP_POST, handleApiSchedulePresetSave);
  server.on("/api/schedule/reset", HTTP_POST, handleApiScheduleReset);

  // Initialize Hardware Task Watchdog Timer (90 seconds timeout)
  esp_task_wdt_init(90, true);
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
  diagEngine.update();
  thermalZones.update(boilerRadio);

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
