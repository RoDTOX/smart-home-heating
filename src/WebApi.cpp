#include "App.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "ConfigTransfer.h"
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
#include <LittleFS.h>

void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", (const char*)DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
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

  // 2. Interlock (FR-2): no boiler ON, queued or not, before a zone valve has been energised for interlockDelay
  if (!thermalZones.boilerOnPermitted()) {
    boilerRadio.recordSpamAttempt();  // a rejected manual request counts for DTC-201 (FR-17.5)
    server.send(409, "application/json", F("{\"status\":\"interlock\",\"error\":\"No zone valve has been open for interlockDelay\",\"boiler\":\"OFF\"}"));
    return;
  }

  // 3. Pre-validate against anti-short-cycling guard & RF cooldown (FR-17.2, FR-17.3, FR-17.4)
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

static String getGasMeterJson() {
  float measuredFlow = 0.0f;
  bool haveMeasured = telemetryHistory.gasFlowMeasured(measuredFlow);
  float flameRate = haveMeasured ? measuredFlow : paramStore.getf(Param::GasFlameRate);
  uint32_t lastEpoch = paramStore.state(StateKey::GasMeterBaseEpoch);  // 0 = no reading typed yet (FR-35.1)
  bool haveReading = lastEpoch != 0;
  float lastIndex = (float)paramStore.state(StateKey::GasMeterBaseIndex) / 1000.0f;
  uint32_t baseSec = paramStore.state(StateKey::GasMeterBaseSec);
  uint32_t totalSec = boilerRadio.getRuntimeTotalSec();
  uint32_t elapsedSec = (totalSec >= baseSec) ? (totalSec - baseSec) : 0;
  float currentEstimatedIndex = lastIndex + (elapsedSec / 3600.0f) * flameRate;
  float gasToday = ((float)boilerRadio.getRuntimeTodaySec() / 3600.0f) * flameRate;
  float gasYesterday = ((float)boilerRadio.getRuntimeYesterdaySec() / 3600.0f) * flameRate;
  float gasThisMonth = ((float)boilerRadio.getRuntimeThisMonthSec() / 3600.0f) * flameRate;
  time_t nowT = time(nullptr);
  if (nowT >= 1700000000) {
    float fromReadings = 0.0f;
    if (telemetryHistory.gasMonthFromReadings((uint32_t)nowT, fromReadings)) gasThisMonth = fromReadings;
  }

  String j;
  j.reserve(520);
  j = "\"gasMeter\":{";
  j += "\"lastReadingIndex\":" + (haveReading ? String(lastIndex, 3) : String("null")) + ",";
  j += "\"lastReadingTime\":" + (haveReading ? String(lastEpoch) : String("null")) + ",";
  j += "\"estimatedIndex\":" + (haveReading ? String(currentEstimatedIndex, 3) : String("null")) + ",";
  j += "\"readingCount\":" + String(telemetryHistory.gasReadingCount()) + ",";
  j += "\"gasTodayM3\":" + String(gasToday, 2) + ",";
  j += "\"gasYesterdayM3\":" + String(gasYesterday, 2) + ",";
  j += "\"gasThisMonthM3\":" + String(gasThisMonth, 2) + ",";
  j += "\"gasFlowMeasured\":" + (haveMeasured ? String(measuredFlow, 3) : String("null")) + ",";
  j += "\"gasFlameRate\":" + String(paramStore.getf(Param::GasFlameRate), 2) + ",";
  j += "\"boilerSetTemp\":" + String(paramStore.get(Param::BoilerSetTemp)) + ",";
  j += "\"boilerFlowObserved\":" + String(paramStore.get(Param::BoilerFlowObserved)) + ",";
  j += "\"boilerPowerKw\":" + String(paramStore.get(Param::BoilerPowerKw)) + ",";
  j += "\"gasPrice\":" + String(paramStore.getf(Param::GasPrice), 2);
  j += "},";
  return j;
}

void handleApiBoilerResetRuntime() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  boilerRadio.resetRuntime();
  server.send(200, "application/json", F("{\"status\":\"ok\",\"message\":\"Boiler runtime counters reset to 0\"}"));
}

void handleApiStatus();

// FR-35.1: the only call that writes a reading. dryRun=1 validates and answers, stores nothing (test mode).
void handleApiBoilerGasMeter() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String idxArg = server.hasArg("index") ? server.arg("index") : String("");
  char* end = nullptr;
  double idx = strtod(idxArg.c_str(), &end);
  if (idxArg.length() == 0 || !end || *end != '\0' || !(idx >= 0.0 && idx <= 999999.9995)) {
    server.send(400, "application/json",
                F("{\"status\":\"error\",\"error\":\"index missing or outside 0 to 999999 (FR-35.1)\"}"));
    return;
  }
  uint32_t now = (uint32_t)time(nullptr);
  uint32_t epoch = now;
  if (server.hasArg("time")) {
    String tArg = server.arg("time");
    char* tEnd = nullptr;
    unsigned long tv = strtoul(tArg.c_str(), &tEnd, 10);
    if (tArg.length() == 0 || !tEnd || *tEnd != '\0' || tv < 1700000000UL || tv > (unsigned long)now) {
      server.send(400, "application/json",
                  F("{\"status\":\"error\",\"error\":\"time must be an epoch in the past (FR-35.1)\"}"));
      return;
    }
    epoch = (uint32_t)tv;
  }
  if (epoch < 1700000000UL) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"clock not set, give time (FR-35.1)\"}"));
    return;
  }
  uint32_t idxMilli = (uint32_t)lround(idx * 1000.0);
  if (server.hasArg("dryRun") && server.arg("dryRun") != "1" && server.arg("dryRun") != "0") {
    server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"dryRun must be 0 or 1\"}"));
    return;
  }
  bool dry = server.hasArg("dryRun") && server.arg("dryRun") == "1";
  bool stored = false;
  if (!dry) {
    // the burner runtime is known only for a reading typed now, not for a back-dated one
    uint32_t runtimeAtReading = (epoch + 120UL >= now) ? boilerRadio.getRuntimeTotalSec() : TelemetryHistoryManager::GAS_NO_RUNTIME;
    if (!telemetryHistory.gasReadingAdd(epoch, idxMilli, runtimeAtReading)) {
      server.send(500, "application/json", F("{\"status\":\"error\",\"error\":\"reading not stored\"}"));
      return;
    }
    stored = true;
    // the last reading (by time) is the base of the estimate; an older reading only joins the log
    if (epoch >= paramStore.state(StateKey::GasMeterBaseEpoch)) {
      paramStore.saveState(StateKey::GasMeterBaseIndex, idxMilli);
      paramStore.saveState(StateKey::GasMeterBaseSec, boilerRadio.getRuntimeTotalSec());
      paramStore.saveState(StateKey::GasMeterBaseEpoch, epoch);
    }
  }
  String resp = String("{\"status\":\"ok\",\"stored\":") + (stored ? "true" : "false") + ",\"reading\":{\"time\":" +
                String(epoch) + ",\"index\":" + String((double)idxMilli / 1000.0, 3) + "}," + getGasMeterJson();
  if (resp.endsWith(",")) resp.remove(resp.length() - 1);
  resp += "}";
  server.send(200, "application/json", resp);
}

struct GasListCtx {
  String chunk;
  bool first;
};

// FR-35.1: all stored readings, oldest first, streamed (up to 1000 entries)
void handleApiGasReadings() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent(String("{\"count\":") + telemetryHistory.gasReadingCount() + ",\"replaced\":" +
                     telemetryHistory.gasReadingsReplaced() + ",\"readings\":[");
  GasListCtx ctx;
  ctx.first = true;
  telemetryHistory.gasReadingsForEach(
      [](uint32_t e, uint32_t idx, uint32_t rt, void* p) {
        GasListCtx* c = (GasListCtx*)p;
        c->chunk += String(c->first ? "" : ",") + "{\"time\":" + String(e) + ",\"index\":" + String((double)idx / 1000.0, 3) +
                    ",\"runtimeSec\":" + (rt == TelemetryHistoryManager::GAS_NO_RUNTIME ? String("null") : String(rt)) + "}";
        c->first = false;
        if (c->chunk.length() > 400) {
          server.sendContent(c->chunk);
          c->chunk = "";
        }
      },
      &ctx);
  ctx.chunk += "]}";
  server.sendContent(ctx.chunk);
  server.sendContent("");
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
    json += "\"hysteresis\":" + String(thermalZones.getHysteresis(), 2) + ",";
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
        "\"windowDropThreshold\":" + String(thermalZones.getWindowThreshold(), 1) + ",";
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
    json += "\"learnedDuty\":" + String(thermalZones.getLearnedDutyPct(i), 1) + ",";
    json += "\"tempOffset\":" + String(cfg.tempOffset, 1) + ",";
    json += "\"runtimeTodaySec\":" + String(thermalZones.getZoneRuntimeTodaySec(i)) + ",";
    json += "\"runtimeYesterdaySec\":" + String(thermalZones.getZoneRuntimeYesterdaySec(i)) + ",";
    json += "\"state\":\"" + String(thermalZones.getStateCode(i)) + "\"";
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
  if (!server.hasArg("threshold")) {
    server.send(400, "application/json", F("{\"error\":\"Missing parameter threshold\"}"));
    return;
  }
  // One global `windowThreshold` for all zones (FR-40); a zone id, if sent, is ignored.
  if (!thermalZones.setWindowThreshold(server.arg("threshold").toFloat())) {
    server.send(400, "application/json",
                F("{\"error\":\"windowThreshold out of range (requirements \u00a76: 0.8 to 2.0 C)\"}"));
    return;
  }
  server.send(200, "application/json",
              String("{\"status\":\"ok\",\"windowThreshold\":") + String(thermalZones.getWindowThreshold(), 1) + "}");
}

static void sendSetupError(const String &message);

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
    const ThermalZoneConfig &cfg = thermalZones.getConfig(idx);
    bool current = cfg.enabled;
    // FR-11.3: on only with a sensor; FR-3: not off while the valve is energised (no overrun hold before step 4)
    if (!current && strlen(cfg.sensorMac) == 0 && strlen(cfg.sensorName) == 0) {
      sendSetupError("A zone is switched on only with a sensor");
      return;
    }
    if (current && thermalZones.getStatus(idx).relayEngaged) {
      sendSetupError("Valve is open; switch the zone off once the valve is closed");
      return;
    }
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
  if (!bleScanner.isSearching()) bleScanner.prune();
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
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.method() == HTTP_POST) {
    if (!timeAndWeather.setLocation(server.arg("coords").c_str())) {
      server.send(400, "application/json",
                  F("{\"status\":\"error\",\"message\":\"Use coords=lat,lon (-90..90, -180..180)\"}"));
      return;
    }
    timeAndWeather.forceWeatherUpdate();
  }
  server.send(200, "application/json",
              String("{\"coords\":\"") + timeAndWeather.getLocation() + "\"}");
}

void handleApiSetupDiscoveredSensors() {
  if (!bleScanner.isSearching()) bleScanner.prune();  // FR-11.6: right after a zone was deleted or re-mapped
  String json;
  json.reserve(1536);
  json = "{\"active\":" + String(bleScanner.isSearching() ? "true" : "false") +
         ",\"remainingSec\":" + String(bleScanner.searchRemainingSec()) + ",\"sensors\":[";
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
    int zoneNumber = 0;  // 1-based number of the zone that uses this sensor, 0 = none (FR-11.5)
    for (size_t z = 0; z < thermalZones.getZoneCount(); z++) {
      const ThermalZoneConfig &zc = thermalZones.getConfig(z);
      if ((zc.sensorMac[0] && strcasecmp(zc.sensorMac, s->mac) == 0) ||
          (zc.sensorName[0] && strcasecmp(zc.sensorName, s->name) == 0)) {
        zoneNumber = (int)z + 1;
        break;
      }
    }
    json += "\"zone\":" + String(zoneNumber);
    json += "}";
    written++;
  }
  json += "]}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

static void sendSetupError(const String &message) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(400, "application/json",
              "{\"status\":\"error\",\"message\":\"" + message + "; nothing changed\"}");
}

// Printable text that fits the record and cannot break the JSON it is written into
static bool validLabel(const String &s, size_t capacity, bool allowEmpty) {
  if (s.length() >= capacity || (!allowEmpty && s.length() == 0)) return false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if ((uint8_t)c < 0x20 || c == '"' || c == '\\') return false;
  }
  return true;
}

static bool validMac(const String &s) {
  if (s.length() == 0) return true;
  if (s.length() != 17) return false;
  for (size_t i = 0; i < 17; i++) {
    char c = s[i];
    if (i % 3 == 2 ? c != ':' : !isxdigit((unsigned char)c)) return false;
  }
  return true;
}

// FR-73: add a zone on the lowest free relay output
void handleApiSetupZoneAdd() {
  String name = server.hasArg("name") ? server.arg("name") : String();
  if (name.length() > 0 && !validLabel(name, 32, false)) {
    sendSetupError("Name must be 1-31 characters, no quotes or backslash");
    return;
  }
  size_t index = 0;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (thermalZones.addZone(name.c_str(), index) != ThermalZoneManager::ZoneResult::Ok) {
    server.send(409, "application/json", F("{\"status\":\"error\",\"message\":\"No relay output is free; nothing changed\"}"));
    return;
  }
  server.send(200, "application/json",
              "{\"status\":\"ok\",\"zone\":" + String((unsigned)(index + 1)) + ",\"relayPin\":" +
                  String(thermalZones.getConfig(index).relayPin) + "}");
}

// FR-74: delete a zone (1-based number); later zones move up
void handleApiSetupZoneDelete() {
  int number = server.hasArg("zone") ? server.arg("zone").toInt() : 0;
  if (number < 1 || number > (int)thermalZones.getZoneCount()) {
    sendSetupError("Unknown zone number");
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (thermalZones.deleteZone((size_t)number - 1) != ThermalZoneManager::ZoneResult::Ok) {
    server.send(409, "application/json",
                F("{\"status\":\"error\",\"message\":\"Valve is open; delete the zone once the valve is closed; nothing changed\"}"));
    return;
  }
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

// FR-11.5: start or stop the sensor search
void handleApiSetupSensorSearch() {
  String action = server.arg("action");
  if (action == "start") bleScanner.startSearch();
  else if (action == "stop") bleScanner.stopSearch();
  else {
    sendSetupError("action must be start or stop");
    return;
  }
  handleApiSetupDiscoveredSensors();
}

// FR-11.3: every field is checked before anything changes; a rejected setting returns 400 and changes nothing.
void handleApiSetupMapZone() {
  if (!server.hasArg("zone") || !isDigit(server.arg("zone")[0])) {
    sendSetupError("Missing or invalid zone index");
    return;
  }
  size_t z = (size_t)server.arg("zone").toInt();
  if (z >= thermalZones.getZoneCount()) {
    sendSetupError("Invalid zone index");
    return;
  }
  const ThermalZoneConfig &cur = thermalZones.getConfig(z);
  String name = server.hasArg("name") ? server.arg("name") : String(cur.name);
  String mac = server.hasArg("mac") ? server.arg("mac") : String(cur.sensorMac);
  mac.toUpperCase();
  String sname =
      server.hasArg("sname") ? server.arg("sname") : String(cur.sensorName);
  uint8_t pin =
      server.hasArg("pin") ? (uint8_t)server.arg("pin").toInt() : cur.relayPin;
  float target =
      server.hasArg("target") ? server.arg("target").toFloat() : cur.targetTemp;
  float offset =
      server.hasArg("offset") ? server.arg("offset").toFloat() : cur.tempOffset;
  String enArg = server.hasArg("enabled") ? server.arg("enabled") : String(cur.enabled ? "1" : "0");
  bool en = (enArg == "true" || enArg == "1");

  if (!(en || enArg == "false" || enArg == "0")) {
    sendSetupError("enabled must be 1/0 or true/false");
    return;
  }
  if (!validLabel(name, sizeof(cur.name), false)) {
    sendSetupError("Name must be 1-31 characters, no quotes or backslash");
    return;
  }
  if (!validMac(mac)) {
    sendSetupError("Sensor MAC must be AA:BB:CC:DD:EE:FF or empty");
    return;
  }
  if (!validLabel(sname, sizeof(cur.sensorName), true)) {
    sendSetupError("Sensor alias must be up to 31 characters, no quotes or backslash");
    return;
  }
  // FR-3: no overrun hold before migration step 4, so an energised valve is neither moved nor switched off
  if (thermalZones.getStatus(z).relayEngaged && (pin != cur.relayPin || (cur.enabled && !en))) {
    sendSetupError("Valve is open; change the relay or switch the zone off once the valve is closed");
    return;
  }
  if (!ParamStore::validRelayPin(pin)) {
    sendSetupError("Invalid relay pin. Allowed: 32, 33, 25, 26");
    return;
  }
  int other = thermalZones.zoneUsingRelay(pin, z);
  if (other >= 0) {
    sendSetupError("Relay already used by zone " + String(other + 1));
    return;
  }
  if (mac.length() > 0) {  // FR-11.3: a sensor MAC serves one zone
    for (size_t k = 0; k < thermalZones.getZoneCount(); k++) {
      if (k != z && strcasecmp(thermalZones.getConfig(k).sensorMac, mac.c_str()) == 0) {
        sendSetupError("Sensor already used by zone " + String((unsigned)(k + 1)));
        return;
      }
    }
  }
  if (thermalZones.isPumpControlEnabled() && pin == thermalZones.getPumpRelayPin()) {
    sendSetupError("Relay is the pump output");
    return;
  }
  if (en && mac.length() == 0 && sname.length() == 0) {
    sendSetupError("A zone is switched on only with a sensor");
    return;
  }
  if (server.hasArg("target") && (target < 15.0f || target > 30.0f)) {
    sendSetupError("Target out of range (15 to 30 C)");
    return;
  }
  if (offset < -3.0f || offset > 3.0f) {
    sendSetupError("Offset out of range (-3 to +3 C)");
    return;
  }
  // `hysteresis` is one global value (requirements §6); "hyst" sets it for all zones.
  if (server.hasArg("hyst") &&
      !paramStore.inRange(Param::Hysteresis,
                          ParamStore::toRaw(Param::Hysteresis, server.arg("hyst").toFloat()))) {
    sendSetupError("hysteresis out of range (0.1 to 1.0 C)");
    return;
  }

  if (server.hasArg("hyst")) thermalZones.setHysteresis(server.arg("hyst").toFloat());
  thermalZones.mapZone(z, name.c_str(), mac.c_str(), sname.c_str(), pin, target, en);
  thermalZones.setTempOffset(z, offset);
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
  json.reserve(2048);
  json = "{";
  json += "\"checks\":";
  json += diagEngine.checksJson();
  json += ",";
  json += "\"dtcCount\":" + String(diagEngine.getActiveDtcCount()) + ",";
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

void handleApiDiagEvents() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", diagEngine.eventsJson());
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
      if (!thermalZones.setPumpControlEnabled(v == "true" || v == "1")) {
        sendSetupError("Relay 4 is a zone output, the pump cannot use it");
        return;
      }
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
  if (server.method() == HTTP_POST && (server.hasArg("timeoutSec") || server.hasArg("sec"))) {
    String v = server.hasArg("timeoutSec") ? server.arg("timeoutSec") : server.arg("sec");
    if (!thermalZones.setSensorTimeoutSec((uint32_t)v.toInt())) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(400, "application/json",
                  F("{\"status\":\"error\",\"message\":\"sensorTimeout out of range (60 to 3600 s)\"}"));
      return;
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
#ifdef SH_DEV_OPEN
  json += "\"profile\":\"dev-open\",";
#else
  json += "\"profile\":\"release\",";
#endif
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
  json += getGasMeterJson();
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

void handleApiScheduleMode() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String m = server.hasArg("mode") ? server.arg("mode") : "";
  if (m.length() == 0 && server.hasArg("plain")) {
    JsonDocument doc;
    if (!deserializeJson(doc, server.arg("plain")) && doc.containsKey("mode")) {
      m = doc["mode"].as<String>();
    }
  }
  if (m.length() > 0) {
    if (!thermalZones.setScheduleModeByName(m.c_str())) {
      server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Use mode=5+2|constant|unified|7day\"}"));
      return;
    }
  }
  server.send(200, "application/json", thermalZones.getScheduleJson());
}

void handleApiScheduleMoment() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String day = server.arg("day");
  int moment = server.arg("moment").toInt();
  String timeStr = server.arg("time");

  if (server.hasArg("plain")) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (!err) {
      if (doc.containsKey("day")) day = doc["day"].as<String>();
      if (doc.containsKey("moment")) moment = doc["moment"].as<int>();
      if (doc.containsKey("time")) timeStr = doc["time"].as<String>();
      if (doc.containsKey("temps")) {
        JsonObject tempsObj = doc["temps"].as<JsonObject>();
        float tArr[MAX_THERMAL_ZONES];
        for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) tArr[i] = -1.0f;
        for (JsonPair p : tempsObj) {
          int zId = atoi(p.key().c_str());
          if (zId >= 1 && zId <= (int)thermalZones.getZoneCount()) {
            tArr[zId - 1] = p.value().as<float>();
          }
        }
        int h = 0, m = 0;
        if (sscanf(timeStr.c_str(), "%d:%d", &h, &m) == 2 && moment >= 1 && moment <= 4 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
          if (thermalZones.setMomentForGroup(day.c_str(), (uint8_t)(moment - 1), (uint8_t)h, (uint8_t)m, tArr)) {
            server.send(200, "application/json", thermalZones.getScheduleJson());
            return;
          }
        }
      }
    }
  }

  if (moment >= 1 && moment <= 4) {
    int h = 0, m = 0;
    if (sscanf(timeStr.c_str(), "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
      float tArr[MAX_THERMAL_ZONES];
      for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String key = "temp_" + String(i + 1);
        tArr[i] = server.hasArg(key) ? server.arg(key).toFloat() : -1.0f;
      }
      if (thermalZones.setMomentForGroup(day.c_str(), (uint8_t)(moment - 1), (uint8_t)h, (uint8_t)m, tArr)) {
        server.send(200, "application/json", thermalZones.getScheduleJson());
        return;
      }
    }
  }

  server.send(400, "application/json", F("{\"status\":\"error\",\"error\":\"Invalid moment parameters\"}"));
}

void handleApiScheduleNames() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (server.hasArg("plain")) {
    JsonDocument doc;
    if (!deserializeJson(doc, server.arg("plain"))) {
      if (doc.containsKey("names") && doc["names"].is<JsonArray>()) {
        JsonArray arr = doc["names"].as<JsonArray>();
        for (size_t i = 0; i < arr.size() && i < MAX_SCHEDULE_MOMENTS; i++) {
          thermalZones.setMomentName((uint8_t)i, arr[i].as<const char*>());
        }
        server.send(200, "application/json", thermalZones.getScheduleJson());
        return;
      } else if (doc.containsKey("moment") && doc.containsKey("name")) {
        int m = doc["moment"].as<int>();
        if (m >= 1 && m <= MAX_SCHEDULE_MOMENTS) {
          thermalZones.setMomentName((uint8_t)(m - 1), doc["name"].as<const char*>());
          server.send(200, "application/json", thermalZones.getScheduleJson());
          return;
        }
      }
    }
  }
  for (size_t i = 0; i < MAX_SCHEDULE_MOMENTS; i++) {
    String pKey = "p" + String(i + 1);
    if (server.hasArg(pKey)) {
      thermalZones.setMomentName((uint8_t)i, server.arg(pKey).c_str());
    }
  }
  if (server.hasArg("moment") && server.hasArg("name")) {
    int m = server.arg("moment").toInt();
    if (m >= 1 && m <= MAX_SCHEDULE_MOMENTS) {
      thermalZones.setMomentName((uint8_t)(m - 1), server.arg("name").c_str());
    }
  }
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
  json += "\"pumpOverrunSec\":" + String(thermalZones.getPumpOverrunMs() / 1000) + ",";
  json += "\"burnerMinRunSec\":" + String(boilerRadio.getMinRunTimeMs() / 1000) + ",";
  json += "\"burnerMinPauseSec\":" + String(boilerRadio.getMinPauseTimeMs() / 1000) + ",";
  json += "\"rfMinCooldownSec\":" + String(boilerRadio.getRfMinCooldownMs() / 1000) + ",";
  json += "\"rfSafetyHeartbeatSec\":" + String(boilerRadio.getSafetyHeartbeatMs() / 1000) + ",";
  json += "\"rfSafetyHeartbeatEnabled\":" + String(boilerRadio.isSafetyHeartbeatEnabled() ? "true" : "false") + ",";
  json += "\"pumpControlEnabled\":" + String(thermalZones.isPumpControlEnabled() ? "true" : "false") + ",";
  json += "\"pumpRelayPin\":" + String(thermalZones.getPumpRelayPin()) + ",";
  json += "\"actuatorNormallyClosed\":" + String(thermalZones.isActuatorNormallyClosed() ? "true" : "false") + ",";
  json += "\"freezeProtectionTemp\":" + String(thermalZones.getFreezeProtectionTemp(), 1) + ",";
  json += "\"sensorTimeoutSec\":" + String(thermalZones.getSensorTimeoutSec()) + ",";
  json += "\"hysteresis\":" + String(thermalZones.getHysteresis(), 2) + ",";
  json += "\"windowThreshold\":" + String(thermalZones.getWindowThreshold(), 1) + ",";
  json += "\"boilerSetTemp\":" + String(paramStore.get(Param::BoilerSetTemp)) + ",";
  json += "\"boilerFlowObserved\":" + String(paramStore.get(Param::BoilerFlowObserved)) + ",";
  json += "\"gasFlameRate\":" + String(paramStore.getf(Param::GasFlameRate), 2) + ",";
  json += "\"boilerPowerKw\":" + String(paramStore.get(Param::BoilerPowerKw)) + ",";
  json += "\"gasPrice\":" + String(paramStore.getf(Param::GasPrice), 2) + ",";
  json += "\"zones\":[";
  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    if (i > 0) json += ",";
    const auto& z = thermalZones.getConfig(i);
    json += "{";
    json += "\"id\":" + String(z.id) + ",";
    json += "\"name\":\"" + String(z.name) + "\",";
    json += "\"mac\":\"" + String(z.sensorMac) + "\",";
    json += "\"relayPin\":" + String(z.relayPin) + ",";
    json += "\"enabled\":" + String(z.enabled ? "true" : "false");
    json += "}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

// FR-42: numeric settings of requirements §6, by their API name.
struct ConfigArg {
  const char* arg;
  Param param;
};
static const ConfigArg CONFIG_ARGS[] = {
    {"actuatorDelaySec", Param::InterlockDelay},
    {"pumpOverrunSec", Param::Overrun},
    {"burnerMinRunSec", Param::BurnerMinRun},
    {"burnerMinPauseSec", Param::BurnerMinPause},
    {"rfSafetyHeartbeatSec", Param::HbInterval},
    {"freezeProtectionTemp", Param::FrostTemp},
    {"sensorTimeoutSec", Param::SensorTimeout},
    {"hysteresis", Param::Hysteresis},
    {"windowThreshold", Param::WindowThreshold},
    {"boilerSetTemp", Param::BoilerSetTemp},
    {"boilerFlowObserved", Param::BoilerFlowObserved},
    {"gasPrice", Param::GasPrice},
};
static const char* CONFIG_FLAGS[] = {"openLoopBypass", "rfSafetyHeartbeatEnabled", "pumpControlEnabled",
                                     "actuatorNormallyClosed"};

static bool isNumber(const String& v) {
  if (v.length() == 0) return false;
  char* end = nullptr;
  strtod(v.c_str(), &end);
  return end && *end == '\0';
}

// A value must lie exactly on the parameter's step (1 s, 0.1 C, 0.01 C) and inside its §6 range; nothing is
// rounded into range (FR-42).
static bool validParamValue(Param p, const String& v) {
  if (!isNumber(v)) return false;
  double scaled = strtod(v.c_str(), nullptr) * ParamStore::def(p).scale;
  int32_t raw = (int32_t)lround(scaled);
  return fabs(scaled - raw) < 1e-6 && paramStore.inRange(p, raw);
}

static bool isFlag(const String& v) {
  return v == "true" || v == "false" || v == "1" || v == "0";
}

static bool flagOn(const String& v) {
  return v == "true" || v == "1";
}

// Every value is checked first; one value outside requirements §6 rejects the whole request (HTTP 400) and
// nothing is changed (FR-42).
void handleApiConfigSystemPost() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String bad;
  for (int i = 0; i < server.args(); i++) {
    String name = server.argName(i);
    if (name == "plain") continue;
    String v = server.arg(i);
    bool known = false, ok = false;
    for (const auto& c : CONFIG_ARGS) {
      if (name == c.arg) {
        known = true;
        ok = validParamValue(c.param, v);
      }
    }
    for (const char* f : CONFIG_FLAGS) {
      if (name == f) {
        known = true;
        ok = isFlag(v);
      }
    }
    // FR-11.3: the pump relay must not be a zone output
    if (name == "pumpControlEnabled" && ok && flagOn(v) && !thermalZones.isPumpControlEnabled() &&
        thermalZones.zoneUsingRelay(thermalZones.getPumpRelayPin(), SIZE_MAX) >= 0)
      ok = false;
    if (!known || !ok) bad += (bad.length() ? ",\"" : "\"") + name + "\"";
  }
  if (bad.length()) {
    server.send(400, "application/json",
                "{\"status\":\"error\",\"message\":\"Unknown, read-only or out of range (requirements \u00a76); "
                "nothing changed\",\"fields\":[" + bad + "]}");
    return;
  }

  if (server.hasArg("actuatorDelaySec")) thermalZones.setInterlockDelaySec(server.arg("actuatorDelaySec").toInt());
  if (server.hasArg("pumpOverrunSec")) thermalZones.setOverrunSec(server.arg("pumpOverrunSec").toInt());
  if (server.hasArg("burnerMinRunSec")) boilerRadio.setMinRunSec(server.arg("burnerMinRunSec").toInt());
  if (server.hasArg("burnerMinPauseSec")) boilerRadio.setMinPauseSec(server.arg("burnerMinPauseSec").toInt());
  if (server.hasArg("rfSafetyHeartbeatSec")) boilerRadio.setHeartbeatSec(server.arg("rfSafetyHeartbeatSec").toInt());
  if (server.hasArg("freezeProtectionTemp")) thermalZones.setFreezeProtectionTemp(server.arg("freezeProtectionTemp").toFloat());
  if (server.hasArg("sensorTimeoutSec")) thermalZones.setSensorTimeoutSec(server.arg("sensorTimeoutSec").toInt());
  if (server.hasArg("hysteresis")) thermalZones.setHysteresis(server.arg("hysteresis").toFloat());
  if (server.hasArg("windowThreshold")) thermalZones.setWindowThreshold(server.arg("windowThreshold").toFloat());
  if (server.hasArg("boilerSetTemp")) paramStore.set(Param::BoilerSetTemp, server.arg("boilerSetTemp").toInt());
  if (server.hasArg("boilerFlowObserved")) paramStore.set(Param::BoilerFlowObserved, server.arg("boilerFlowObserved").toInt());
  if (server.hasArg("gasPrice")) paramStore.setf(Param::GasPrice, server.arg("gasPrice").toFloat());
  if (server.hasArg("openLoopBypass")) thermalZones.setOpenLoopBypass(flagOn(server.arg("openLoopBypass")));
  if (server.hasArg("rfSafetyHeartbeatEnabled")) boilerRadio.setSafetyHeartbeatEnabled(flagOn(server.arg("rfSafetyHeartbeatEnabled")));
  if (server.hasArg("pumpControlEnabled")) thermalZones.setPumpControlEnabled(flagOn(server.arg("pumpControlEnabled")));
  if (server.hasArg("actuatorNormallyClosed")) thermalZones.setActuatorNormallyClosed(flagOn(server.arg("actuatorNormallyClosed")));
  handleApiConfigSystemGet();
}

// FR-65.1: the settings file. No Access-Control-Allow-Origin header (FR-65.7).
void handleApiConfigExport() {
  server.send(200, "application/json", configExportJson());
}

// FR-65.7: checks in this order, the first that fails answers and nothing is changed: media type (415), size (413),
// JSON object with "format": 1 (400, inside configImport). Authentication (401, FR-58) comes with migration step 9.
void handleApiConfigImport() {
  String type = server.header("Content-Type");
  type.toLowerCase();
  type.trim();
  bool json = type.startsWith("application/json") && (type.length() == 16 || type[16] == ';' || type[16] == ' ');
  if (!json) {
    server.send(415, "application/json", F("{\"status\":\"error\",\"message\":\"Content-Type must be application/json; nothing changed\"}"));
    return;
  }
  const String& body = server.arg("plain");
  if (body.length() > CONFIG_IMPORT_MAX_BYTES) {
    server.send(413, "application/json", F("{\"status\":\"error\",\"message\":\"Body over 16384 bytes; nothing changed\"}"));
    return;
  }
  String answer;
  int code = configImport(body, answer);
  server.send(code, "application/json", answer);
}

// FR-43: only in MANUAL mode, only an output of a zone or the pump, respecting NC / NO (FR-15.1)
void handleApiRelayTest() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (!server.hasArg("pin")) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Missing 'pin' parameter.\"}"));
    return;
  }
  long pin = server.arg("pin").toInt();
  if (pin < 0 || pin > 255 || !ParamStore::validRelayPin((uint8_t)pin)) {
    server.send(400, "application/json", F("{\"status\":\"error\",\"message\":\"Invalid relay pin. Allowed: 32, 33, 25, 26.\"}"));
    return;
  }
  int zone = thermalZones.zoneUsingRelay((uint8_t)pin, SIZE_MAX);
  bool isPump = zone < 0 && thermalZones.isPumpControlEnabled() && (uint8_t)pin == thermalZones.getPumpRelayPin();
  if (thermalZones.isAutoMode() || (zone < 0 && !isPump)) {
    server.send(409, "application/json",
                F("{\"status\":\"error\",\"message\":\"Relay test needs MANUAL mode and an output of a zone or the pump\"}"));
    return;
  }
  bool state = (server.arg("state") == "1" || server.arg("state") == "true" || server.arg("state") == "on");
  if (zone >= 0) thermalZones.setRelayManual((size_t)zone, state);
  else thermalZones.setPumpManual(state);
  netConsole.printf("[RELAY-TEST] Relay test: pin %ld -> %s\n", pin, state ? "ON" : "OFF");
  server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void registerWebRoutes() {
  static const char* headerKeys[] = {"Content-Type", "Authorization"};  // FR-65.7, FR-75.1
  server.collectHeaders(headerKeys, 2);
  // Web Server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/setup/location", handleApiSetupLocation);
  server.on("/api/setup/discovered-sensors", HTTP_GET,
            handleApiSetupDiscoveredSensors);
  server.on("/api/setup/map-zone", HTTP_POST, handleApiSetupMapZone);
  server.on("/api/setup/zone-add", HTTP_POST, handleApiSetupZoneAdd);
  server.on("/api/setup/zone-delete", HTTP_POST, handleApiSetupZoneDelete);
  server.on("/api/setup/sensor-search", HTTP_POST, handleApiSetupSensorSearch);
  server.on("/api/setup/time", HTTP_GET, handleApiSetupTime);
  server.on("/api/boiler/on", HTTP_POST, handleApiOn);
  server.on("/api/boiler/off", HTTP_POST, handleApiOff);
  server.on("/api/boiler/cancel", HTTP_POST, handleApiBoilerCancelPending);
  server.on("/api/boiler/runtime/reset", HTTP_POST, handleApiBoilerResetRuntime);
  server.on("/api/boiler/gas-meter", HTTP_POST, handleApiBoilerGasMeter);
  server.on("/api/boiler/gas-readings", HTTP_GET, handleApiGasReadings);
  server.on("/api/boiler/status", HTTP_GET, handleApiStatus);
  server.on("/api/status", HTTP_GET, handleApiStatus);
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
  server.on("/api/diag/events", HTTP_GET, handleApiDiagEvents);
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
  server.on("/api/schedule/mode", HTTP_POST, handleApiScheduleMode);
  server.on("/api/schedule/moment", HTTP_POST, handleApiScheduleMoment);
  server.on("/api/schedule/names", HTTP_POST, handleApiScheduleNames);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.on("/api/history/clear", HTTP_POST, handleApiHistoryClear);
  server.on("/api/config/system", HTTP_GET, handleApiConfigSystemGet);
  server.on("/api/config/system", HTTP_POST, handleApiConfigSystemPost);
  server.on("/api/relay/test", HTTP_POST, handleApiRelayTest);
  server.on("/api/config/export", HTTP_GET, handleApiConfigExport);
  server.on("/api/config/import", HTTP_POST, handleApiConfigImport);

}
