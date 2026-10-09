#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "Config.h"
#include "DevicePassport.h"
#include "DiagnosticEngine.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include "WebSetupPage.h"
#include <Arduino.h>
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

void printHelp() {
  netConsole.println();
  netConsole.println(F("=================================================="));
  netConsole.println(F("  Smart Heating Controller - Multi-Zone & RF 433  "));
  netConsole.println(F("=================================================="));
  netConsole.println(F("Available commands:"));
  netConsole.println(F("  1 or ON          -> Send boiler START signal"));
  netConsole.println(F("  0 or OFF         -> Send boiler STOP signal"));
  netConsole.println(F("  AUTO             -> Switch to AUTOMATIC mode"));
  netConsole.println(F("  MANUAL           -> Switch to MANUAL mode"));
  netConsole.println(
      F("  SET <z> <temp>   -> Set target temperature (e.g. SET 1 22.5)"));
  netConsole.println(
      F("  PUMP <ON|OFF>    -> Manual circulation pump control (Relay 4)"));
  netConsole.println(
      F("  ACTUATOR <NC|NO> -> Toggle actuator electrical polarity"));
  netConsole.println(
      F("  EXERCISE         -> Run 20s anti-seize cycle (pump & valves)"));
  netConsole.println(
      F("  BLE or SENSORS   -> List decoded BLE room thermometers"));
  netConsole.println(
      F("  DIAG [SELFTEST]  -> remote diagnostic report & DTCs"));
  netConsole.println(F("  PASSPORT         -> View device commissioning & "
                       "installation metadata"));
  netConsole.println(
      F("  TIME             -> View NTP clock, date, season & heating mode"));
  netConsole.println(
      F("  WEATHER          -> View outdoor weather from Open-Meteo REST API"));
  netConsole.println(
      F("  s or STATUS      -> Print system status, zones & sensors"));
  netConsole.println(F("  ? or HELP        -> Display this help menu"));
  netConsole.println(F("=================================================="));
  netConsole.println();
}

void handleRoot() {
  bool isOn = boilerRadio.isLastCommandOn();
  uint32_t lastTx = boilerRadio.getLastTransmissionTime();
  uint32_t secondsAgo = (lastTx > 0) ? (millis() - lastTx) / 1000 : 0;
  bool isAuto = thermalZones.isAutoMode();

  String html = F(
      "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Smart Heating - Boiler & Zone Controller</title>"
      "<style>"
      "*{box-sizing:border-box;}"
      "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe "
      "UI',Roboto,sans-serif;"
      "background:#0f172a;color:#f8fafc;margin:0;padding:16px;display:flex;"
      "flex-direction:column;align-items:center;min-height:100vh;}"
      ".card{background:#1e293b;border-radius:16px;padding:20px;width:100%;max-"
      "width:480px;"
      "box-shadow:0 10px 25px -5px rgba(0,0,0,0.5);border:1px solid #334155;}"
      "h1{font-size:1.35rem;margin:0 0 4px 0;text-align:center;color:#38bdf8;}"
      ".subtitle{font-size:0.8rem;color:#94a3b8;text-align:center;margin-"
      "bottom:16px;}"
      ".status-box{display:flex;justify-content:space-between;align-items:"
      "center;"
      "background:#0f172a;padding:12px "
      "16px;border-radius:12px;margin-bottom:14px;"
      "border:1px solid #334155;}"
      ".status-val{font-weight:700;font-size:1rem;padding:4px "
      "10px;border-radius:8px;}"
      ".badge-on{background:#065f46;color:#34d399;border:1px solid #059669;}"
      ".badge-off{background:#7f1d1d;color:#f87171;border:1px solid #dc2626;}"
      ".mode-bar{display:flex;gap:8px;margin-bottom:16px;background:#0f172a;"
      "padding:4px;border-radius:12px;border:1px solid #334155;}"
      ".mode-btn{flex:1;padding:8px;border:none;border-radius:8px;font-weight:"
      "700;font-size:0.85rem;cursor:pointer;background:transparent;color:#"
      "94a3b8;transition:all 0.2s;}"
      ".mode-btn.active{background:#0284c7;color:#ffffff;box-shadow:0 2px 8px "
      "rgba(2,132,199,0.4);}"
      ".sec-title{font-size:0.85rem;font-weight:700;color:#94a3b8;margin:16px "
      "0 8px "
      "0;display:flex;justify-content:space-between;align-items:center;text-"
      "transform:uppercase;letter-spacing:0.5px;}"
      ".zone-card{background:#0f172a;border:1px solid "
      "#334155;border-radius:12px;padding:14px;margin-bottom:10px;display:flex;"
      "flex-direction:column;gap:10px;}"
      ".zone-header{display:flex;justify-content:space-between;align-items:"
      "center;}"
      ".zone-name{font-weight:700;font-size:0.95rem;color:#f1f5f9;}"
      ".zone-badge{font-size:0.75rem;font-weight:700;padding:3px "
      "8px;border-radius:6px;}"
      ".badge-idle{background:#1e293b;color:#94a3b8;border:1px solid #475569;}"
      ".badge-warming{background:#78350f;color:#fcd34d;border:1px solid "
      "#b45309;}"
      ".badge-heating{background:#065f46;color:#34d399;border:1px solid "
      "#059669;}"
      ".badge-disabled{background:#334155;color:#64748b;border:1px solid "
      "#475569;}"
      ".zone-body{display:flex;justify-content:space-between;align-items:"
      "center;}"
      ".zone-temp{font-size:1.25rem;font-weight:700;color:#38bdf8;}"
      ".zone-ctrl{display:flex;align-items:center;gap:6px;}"
      ".temp-btn{background:#1e293b;color:#f8fafc;border:1px solid "
      "#475569;width:32px;height:32px;border-radius:8px;font-size:1.1rem;font-"
      "weight:700;cursor:pointer;display:flex;align-items:center;justify-"
      "content:center;transition:background 0.1s;}"
      ".temp-btn:active{background:#334155;}"
      ".setpoint-val{font-size:1rem;font-weight:700;min-width:54px;text-align:"
      "center;}"
      ".zone-footer{display:flex;justify-content:space-between;align-items:"
      "center;font-size:0.75rem;color:#64748b;padding-top:4px;border-top:1px "
      "dashed #1e293b;}"
      ".toggle-link{color:#38bdf8;cursor:pointer;text-decoration:none;font-"
      "weight:600;}"
      ".btn-group{display:flex;gap:8px;margin-bottom:12px;}"
      ".btn-action{flex:1;border:none;border-radius:10px;padding:12px;font-"
      "size:0.9rem;font-weight:700;cursor:pointer;display:flex;justify-content:"
      "center;align-items:center;gap:6px;}"
      ".btn-on{background:linear-gradient(135deg,#10b981,#059669);color:#"
      "ffffff;}"
      ".btn-off{background:linear-gradient(135deg,#ef4444,#dc2626);color:#"
      "ffffff;}"
      ".sensor-grid{display:flex;flex-direction:column;gap:6px;}"
      ".sensor-row{display:flex;justify-content:space-between;align-items:"
      "center;background:#0f172a;padding:8px 12px;border-radius:8px;border:1px "
      "solid #334155;font-size:0.85rem;}"
      ".sensor-name{font-weight:600;color:#f1f5f9;}"
      ".sensor-temp{font-weight:700;color:#38bdf8;font-size:0.95rem;}"
      ".meta{margin-top:16px;padding-top:12px;border-top:1px solid "
      "#334155;font-size:0.75rem;color:#64748b;display:grid;grid-template-"
      "columns:1fr 1fr;gap:4px;}"
      ".footer{margin-top:14px;text-align:center;font-size:0.8rem;}"
      "a{color:#38bdf8;text-decoration:none;}"
      "</style></head><body>"
      "<div class='card'>"
      "<h1>Smart Heating Controller</h1>"
      "<div class='subtitle'>Multi-Zone &bull; RF 433.92 MHz &bull; ESP32</div>"
      "<div class='status-box'>"
      "<span>Boiler Flame:</span>");

  if (isOn) {
    html += F("<span id='boiler-badge' class='status-val badge-on'>ON "
              "(Active)</span>");
  } else {
    html += F("<span id='boiler-badge' class='status-val badge-off'>OFF "
              "(Idle)</span>");
  }

  html += F("</div>"
            "<div class='mode-bar'>");
  html += "<button id='btn-mode-auto' class='mode-btn " +
          String(isAuto ? "active" : "") +
          "' onclick=\"setMode('auto')\">&#9881; AUTOMATIC</button>";
  html += "<button id='btn-mode-manual' class='mode-btn " +
          String(!isAuto ? "active" : "") +
          "' onclick=\"setMode('manual')\">&#9995; MANUAL</button>";
  html += F("</div>"
            "<div class='btn-group'>"
            "<button class='btn-action btn-on' "
            "onclick=\"trigger('on')\">&#128293; Start Boiler</button>"
            "<button class='btn-action btn-off' "
            "onclick=\"trigger('off')\">&#10060; Stop Boiler</button>"
            "</div>"
            "<div class='sec-title'>"
            "<span>Thermal Zones (Underfloor)</span>"
            "<span>Target</span>"
            "</div>");

  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    const ThermalZoneConfig &cfg = thermalZones.getConfig(i);
    const ThermalZoneStatus &st = thermalZones.getStatus(i);

    String badgeClass = "badge-idle";
    if (st.state == ZONE_HEATING)
      badgeClass = "badge-heating";
    else if (st.state == ZONE_WARMING_UP)
      badgeClass = "badge-warming";
    else if (st.state == ZONE_DISABLED)
      badgeClass = "badge-disabled";

    html += "<div class='zone-card'>";
    html += "<div class='zone-header'>";
    html += "<span class='zone-name'>" + String(cfg.name) + "</span>";
    html += "<span class='zone-badge " + badgeClass + "'>" +
            String(thermalZones.getStateStr(st.state)) + "</span>";
    html += "</div>";

    html += "<div class='zone-body'>";
    html += "<div>";
    if (st.sensorValid) {
      html += "<span class='zone-temp'>" + String(st.currentTemp, 1) +
              "&deg;C</span>";
    } else {
      html +=
          "<span class='zone-temp' style='color:#64748b;'>--.-&deg;C</span>";
    }
    html += "<div "
            "style='font-size:0.75rem;color:#64748b;margin-top:2px;'>Sensor: " +
            String(cfg.sensorName) + "</div>";
    html += "</div>";

    html += "<div class='zone-ctrl'>";
    html += "<button class='temp-btn' onclick=\"adjTarget(" + String(i) +
            ", -0.5)\">-</button>";
    html += "<span class='setpoint-val' id='sp-" + String(i) + "' data-val='" +
            String(cfg.targetTemp, 1) + "'>" + String(cfg.targetTemp, 1) +
            "&deg;C</span>";
    html += "<button class='temp-btn' onclick=\"adjTarget(" + String(i) +
            ", 0.5)\">+</button>";
    html += "</div>";
    html += "</div>";

    html += "<div class='zone-footer'>";
    html += "<span>Relay GPIO" + String(cfg.relayPin) + ": <b>" +
            String(st.relayEngaged ? "ON (Open)" : "OFF (Closed)") +
            "</b></span>";
    html += "<span class='toggle-link' onclick=\"toggleZone(" + String(i) +
            ")\">" + String(cfg.enabled ? "Disable" : "Enable") + "</span>";
    html += "</div>";
    html += "</div>";
  }

  html += F("<div class='sec-title'>"
            "<span>BLE Sensor Telemetry</span>");

  html += F("<span style='color:#34d399;font-size:0.75rem;'>&bull; BLE Active</span>");
  html += F("</div><div class='sensor-grid'>");

  size_t sCount = bleScanner.getSensorCount();
  size_t validCount = 0;
  for (size_t i = 0; i < sCount; i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s || !s->valid)
      continue;
    validCount++;
    html += "<div class='sensor-row'>";
    html += "<span class='sensor-name'>" + String(s->name) + "</span>";
    html += "<div><span class='sensor-temp'>" + String(s->temperature, 1) +
            "&deg;C</span>";
    html += "<span style='font-size:0.75rem;color:#64748b;margin-left:6px;'>" +
            String(s->humidity, 0) + "%</span></div>";
    html += "</div>";
  }
  if (validCount == 0) {
    html += F("<div class='sensor-row'><span style='color:#64748b;'>Scanning for BLE sensors...</span></div>");
  }

  html += F("</div>"
            "<div class='meta'>"
            "<div>Last Command: <b>");
  html += (lastTx == 0) ? "None" : (String(secondsAgo) + " sec.");
  html += F("</b></div>"
            "<div>Free Heap: <b>");
  html += String(ESP.getFreeHeap() / 1024);
  html += F(" KB</b></div>"
            "<div>Wi-Fi RSSI: <b>");
  html += String(WiFi.RSSI());
  html += F(" dBm</b></div>"
            "<div>IP: <b>");
  html += WiFi.localIP().toString();
  html += F("</b></div>"
            "<div>Time (NTP): <b>");
  html += timeAndWeather.getFormattedTime();
  html += F("</b></div>"
            "<div>Outdoor Temp: <b>");
  html += timeAndWeather.isWeatherValid()
              ? (String(timeAndWeather.getOutdoorTemperature(), 1) + "&deg;C")
              : String("Pending");
  html += F("</b></div></div>"
            "<div class='footer' "
            "style='display:flex;justify-content:center;gap:16px;flex-wrap:"
            "wrap;'><a href='console'><b>&gt;_ Live Console</b></a> &bull; <a "
            "href='setup'><b>⚙️ Setup Wizard</b></a> &bull; <a "
            "href='update'>Firmware OTA</a></div>"
            "</div>"
            "<script>"
            "const base = window.location.pathname.endsWith('/') ? "
            "window.location.pathname : window.location.pathname + '/';"
            "function trigger(action){"
            "  const b = document.getElementById('boiler-badge');"
            "  if (b) {"
            "    b.innerText = action === 'on' ? 'ON (Active)' : 'OFF (Idle)';"
            "    b.className = 'status-val ' + (action === 'on' ? 'badge-on' : "
            "'badge-off');"
            "  }"
            "  fetch(base + 'api/boiler/' + action, {method:'POST'});"
            "}"
            "function setMode(m){"
            "  const ba = document.getElementById('btn-mode-auto');"
            "  const bm = document.getElementById('btn-mode-manual');"
            "  if (ba && bm) {"
            "    ba.className = 'mode-btn ' + (m === 'auto' ? 'active' : '');"
            "    bm.className = 'mode-btn ' + (m === 'manual' ? 'active' : '');"
            "  }"
            "  fetch(base + 'api/mode?mode=' + m, {method:'POST'});"
            "}"
            "function adjTarget(idx, delta){"
            "  const el = document.getElementById('sp-' + idx);"
            "  if (!el) return;"
            "  let cur = parseFloat(el.dataset.val || el.innerText);"
            "  let nxt = (cur + delta).toFixed(1);"
            "  el.dataset.val = nxt;"
            "  el.innerHTML = nxt + '&deg;C';"
            "  fetch(base + 'api/zones/setpoint?id=' + idx + '&target=' + nxt, "
            "{method:'POST'});"
            "}"
            "function toggleZone(idx){"
            "  fetch(base + 'api/zones/toggle?id=' + idx, "
            "{method:'POST'}).then(() => location.reload());"
            "}"
            "</script>"
            "</body></html>");

  server.send(200, "text/html", html);
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
    json += "\"hysteresis\":" + String(cfg.hysteresis, 2) + ",";
    json += "\"enabled\":" + String(cfg.enabled ? "true" : "false") + ",";
    json += "\"currentTemp\":" + String(st.currentTemp, 1) + ",";
    json +=
        "\"sensorValid\":" + String(st.sensorValid ? "true" : "false") + ",";
    json +=
        "\"sensorFault\":" + String(st.sensorFault ? "true" : "false") + ",";
    json +=
        "\"frostActive\":" + String(st.frostActive ? "true" : "false") + ",";
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
  server.send_P(200, "text/html", SETUP_HTML);
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

void handleApiStatus() {
  String json = "{";
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
      if (boilerRadio.sendBoilerOn()) {
        netConsole.println(F("[BOILER] Command ON sent successfully via RF 433.92 MHz."));
      }
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
      if (boilerRadio.sendBoilerOff()) {
        netConsole.println(F("[BOILER] Command OFF sent successfully via RF 433.92 MHz."));
      }
    }
  } else if (upperCmd == "AUTO") {
    thermalZones.setAutoMode(true);
    netConsole.println(F("[MODE] Switched to AUTOMATIC mode."));
  } else if (upperCmd == "MANUAL") {
    thermalZones.setAutoMode(false);
    netConsole.println(F("[MODE] Switched to MANUAL mode."));
  } else if (upperCmd.startsWith("SET ")) {
    // Format: SET <zoneIndex 0..2 or id 1..3> <temp>
    int firstSpace = upperCmd.indexOf(' ');
    int secondSpace = upperCmd.indexOf(' ', firstSpace + 1);
    if (secondSpace > firstSpace) {
      int zoneArg = upperCmd.substring(firstSpace + 1, secondSpace).toInt();
      float t = upperCmd.substring(secondSpace + 1).toFloat();
      size_t idx = (zoneArg >= 1) ? (zoneArg - 1) : 0;
      thermalZones.setTargetTemperature(idx, t);
      netConsole.printf("[ZONE] Zone %u setpoint updated to %.1f C\n",
                        (unsigned int)(idx + 1), t);
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
  } else if (upperCmd == "S" || upperCmd == "STATUS") {
    netConsole.printf("[STATUS] Boiler: %s | Heat Demand: %s | Mode: %s | IP: "
                      "%s | Heap: %u B\n",
                      boilerRadio.isLastCommandOn() ? "ON" : "OFF",
                      thermalZones.isBoilerDemanded() ? "DEMAND" : "IDLE",
                      thermalZones.isAutoMode() ? "AUTOMATIC" : "MANUAL",
                      WiFi.status() == WL_CONNECTED
                          ? WiFi.localIP().toString().c_str()
                          : WiFi.softAPIP().toString().c_str(),
                      ESP.getFreeHeap());
    netConsole.printf(
        "[HARDWARE] Actuators: %s | Pump Relay 4 (GPIO%d): %s [%s] | Frost "
        "Guard: %s\n",
        thermalZones.isActuatorNormallyClosed() ? "NC (Normally Closed)"
                                                : "NO (Normally Open)",
        PIN_RELAY_SPARE,
        thermalZones.isPumpControlEnabled() ? "ENABLED" : "DISABLED",
        thermalZones.isPumpActive() ? "ACTIVE (Running)" : "IDLE",
        thermalZones.isFrostProtectionActive() ? "TRIGGERED (<12C)"
                                               : "STANDBY");
    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
      const ThermalZoneConfig &cfg = thermalZones.getConfig(i);
      const ThermalZoneStatus &st = thermalZones.getStatus(i);
      netConsole.printf("  [Zone %u] %-20s: %.1f C / %.1f C (Target) | State: "
                        "%-22s | Relay GPIO%d: %s\n",
                        cfg.id, cfg.name, st.currentTemp, cfg.targetTemp,
                        thermalZones.getStateStr(st.state), cfg.relayPin,
                        st.relayEngaged ? "ON" : "OFF");
    }
    size_t sc = bleScanner.getSensorCount();
    netConsole.printf(
        "[STATUS] Room Thermometers (%u) [Passive BLE Scan: %s]:\n", sc,
        bleScanner.isScanning() ? "ACTIVE" : "INACTIVE");
    for (size_t i = 0; i < sc; i++) {
      const BleSensorData *s = bleScanner.getSensor(i);
      if (s && s->valid) {
        uint32_t age = (millis() - s->lastSeenMs) / 1000;
        netConsole.printf("  - %-16s (%s): %.1f C | %.1f %% | Bat: %d%% | "
                          "RSSI: %d dBm (%us ago)\n",
                          s->name, s->mac, s->temperature, s->humidity,
                          s->battery, s->rssi, age);
      }
    }
  } else if (upperCmd == "BLE" || upperCmd == "SENSORS") {
    size_t bc = bleScanner.getSensorCount();
    netConsole.printf(
        "[BLE] Room Thermometers Registry (%u) [Passive Scan: %s]:\n", bc,
        bleScanner.isScanning() ? "RUNNING" : "STOPPED");
    for (size_t i = 0; i < bc; i++) {
      const BleSensorData *s = bleScanner.getSensor(i);
      if (s) {
        uint32_t age = (millis() - s->lastSeenMs) / 1000;
        netConsole.printf("  [%u] %-16s (%s) | %.1f C | %.1f %% | Bat: %d%% | "
                          "RSSI: %d dBm | Pkts: %u | %s (%us ago)\n",
                          (unsigned int)(i + 1), s->name, s->mac,
                          s->temperature, s->humidity, s->battery, s->rssi,
                          s->packetCount, bleScanner.getSourceStr(s->source),
                          age);
      }
    }
    if (bc == 0) {
      netConsole.println(F("  No BLE thermometers decoded yet. Waiting for "
                           "advertisements (interval ~10-20s)..."));
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
  } else if (upperCmd == "?" || upperCmd == "HELP") {
    printHelp();
  } else {
    netConsole.printf("[Unknown command: '%s']. Type '?' for commands.\n",
                      cmd.c_str());
  }
}

void setup() {
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

  // Initialize Hardware Task Watchdog Timer (30 seconds timeout to safely
  // accommodate OTA flash writes)
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);
  netConsole.println(
      F("[System] Hardware Task Watchdog Timer (TWDT) armed: 30s."));

  // Enable Web OTA on /update (admin / admin123)
  httpUpdater.setup(&server, "/update", OTA_USER, OTA_PASS);
  server.begin();

  printHelp();
  netConsole.println(F("[HTTP] Web Server, Web Console, Setup Wizard and Web "
                       "OTA running on port 80."));
  netConsole.println(F("[Ready] System ready for commands."));
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
