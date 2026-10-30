#include "App.h"
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
#include <WebServer.h>
#include <WiFi.h>


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
    json += "\"duty\":" + String((int)(thermalZones.getDuty(i) * 100)) + ",";
    json += "\"learnedDuty\":" + String((int)(thermalZones.getLearnedDuty(i) * 100)) + ",";
    json += "\"tempOffset\":" + String(cfg.tempOffset, 1) + ",";
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
  if (server.hasArg("offset")) {
    thermalZones.setTempOffset(z, server.arg("offset").toFloat());
  }
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

void registerWebRoutes() {
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

}
