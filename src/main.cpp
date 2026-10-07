#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include "Config.h"
#include "BoilerRadioTrigger.h"
#include "MqttTelemetryClient.h"
#include "ThermalZoneManager.h"
#include "NetworkConsole.h"

WebServer server(80);
HTTPUpdateServer httpUpdater;

static bool softApActive = false;
static String serialBuffer = "";

void blinkLed(uint8_t count = 2, uint16_t delayMs = 60) {
    for (uint8_t i = 0; i < count; i++) {
        digitalWrite(PIN_STATUS_LED, HIGH);  // Active HIGH on ESP32 Relay board
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
    netConsole.println(F("  SET <z> <temp>   -> Set target temperature (e.g. SET 1 22.5)"));
    netConsole.println(F("  s or STATUS      -> Print system status, zones & sensors"));
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
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
        "background:#0f172a;color:#f8fafc;margin:0;padding:16px;display:flex;"
        "flex-direction:column;align-items:center;min-height:100vh;}"
        ".card{background:#1e293b;border-radius:16px;padding:20px;width:100%;max-width:480px;"
        "box-shadow:0 10px 25px -5px rgba(0,0,0,0.5);border:1px solid #334155;}"
        "h1{font-size:1.35rem;margin:0 0 4px 0;text-align:center;color:#38bdf8;}"
        ".subtitle{font-size:0.8rem;color:#94a3b8;text-align:center;margin-bottom:16px;}"
        ".status-box{display:flex;justify-content:space-between;align-items:center;"
        "background:#0f172a;padding:12px 16px;border-radius:12px;margin-bottom:14px;"
        "border:1px solid #334155;}"
        ".status-val{font-weight:700;font-size:1rem;padding:4px 10px;border-radius:8px;}"
        ".badge-on{background:#065f46;color:#34d399;border:1px solid #059669;}"
        ".badge-off{background:#7f1d1d;color:#f87171;border:1px solid #dc2626;}"
        ".mode-bar{display:flex;gap:8px;margin-bottom:16px;background:#0f172a;padding:4px;border-radius:12px;border:1px solid #334155;}"
        ".mode-btn{flex:1;padding:8px;border:none;border-radius:8px;font-weight:700;font-size:0.85rem;cursor:pointer;background:transparent;color:#94a3b8;transition:all 0.2s;}"
        ".mode-btn.active{background:#0284c7;color:#ffffff;box-shadow:0 2px 8px rgba(2,132,199,0.4);}"
        ".sec-title{font-size:0.85rem;font-weight:700;color:#94a3b8;margin:16px 0 8px 0;display:flex;justify-content:space-between;align-items:center;text-transform:uppercase;letter-spacing:0.5px;}"
        ".zone-card{background:#0f172a;border:1px solid #334155;border-radius:12px;padding:14px;margin-bottom:10px;display:flex;flex-direction:column;gap:10px;}"
        ".zone-header{display:flex;justify-content:space-between;align-items:center;}"
        ".zone-name{font-weight:700;font-size:0.95rem;color:#f1f5f9;}"
        ".zone-badge{font-size:0.75rem;font-weight:700;padding:3px 8px;border-radius:6px;}"
        ".badge-idle{background:#1e293b;color:#94a3b8;border:1px solid #475569;}"
        ".badge-warming{background:#78350f;color:#fcd34d;border:1px solid #b45309;}"
        ".badge-heating{background:#065f46;color:#34d399;border:1px solid #059669;}"
        ".badge-disabled{background:#334155;color:#64748b;border:1px solid #475569;}"
        ".zone-body{display:flex;justify-content:space-between;align-items:center;}"
        ".zone-temp{font-size:1.25rem;font-weight:700;color:#38bdf8;}"
        ".zone-ctrl{display:flex;align-items:center;gap:6px;}"
        ".temp-btn{background:#1e293b;color:#f8fafc;border:1px solid #475569;width:32px;height:32px;border-radius:8px;font-size:1.1rem;font-weight:700;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:background 0.1s;}"
        ".temp-btn:active{background:#334155;}"
        ".setpoint-val{font-size:1rem;font-weight:700;min-width:54px;text-align:center;}"
        ".zone-footer{display:flex;justify-content:space-between;align-items:center;font-size:0.75rem;color:#64748b;padding-top:4px;border-top:1px dashed #1e293b;}"
        ".toggle-link{color:#38bdf8;cursor:pointer;text-decoration:none;font-weight:600;}"
        ".btn-group{display:flex;gap:8px;margin-bottom:12px;}"
        ".btn-action{flex:1;border:none;border-radius:10px;padding:12px;font-size:0.9rem;font-weight:700;cursor:pointer;display:flex;justify-content:center;align-items:center;gap:6px;}"
        ".btn-on{background:linear-gradient(135deg,#10b981,#059669);color:#ffffff;}"
        ".btn-off{background:linear-gradient(135deg,#ef4444,#dc2626);color:#ffffff;}"
        ".sensor-grid{display:flex;flex-direction:column;gap:6px;}"
        ".sensor-row{display:flex;justify-content:space-between;align-items:center;background:#0f172a;padding:8px 12px;border-radius:8px;border:1px solid #334155;font-size:0.85rem;}"
        ".sensor-name{font-weight:600;color:#f1f5f9;}"
        ".sensor-temp{font-weight:700;color:#38bdf8;font-size:0.95rem;}"
        ".meta{margin-top:16px;padding-top:12px;border-top:1px solid #334155;font-size:0.75rem;color:#64748b;display:grid;grid-template-columns:1fr 1fr;gap:4px;}"
        ".footer{margin-top:14px;text-align:center;font-size:0.8rem;}"
        "a{color:#38bdf8;text-decoration:none;}"
        "</style></head><body>"
        "<div class='card'>"
        "<h1>Smart Heating Controller</h1>"
        "<div class='subtitle'>Multi-Zone &bull; RF 433.92 MHz &bull; ESP32</div>"
        "<div class='status-box'>"
        "<span>Boiler Flame:</span>"
    );

    if (isOn) {
        html += F("<span class='status-val badge-on'>ON (Active)</span>");
    } else {
        html += F("<span class='status-val badge-off'>OFF (Idle)</span>");
    }

    html += F(
        "</div>"
        "<div class='mode-bar'>"
    );
    html += "<button class='mode-btn " + String(isAuto ? "active" : "") + "' onclick=\"setMode('auto')\">&#9881; AUTOMATIC</button>";
    html += "<button class='mode-btn " + String(!isAuto ? "active" : "") + "' onclick=\"setMode('manual')\">&#9995; MANUAL</button>";
    html += F(
        "</div>"
        "<div class='btn-group'>"
        "<button class='btn-action btn-on' onclick=\"trigger('on')\">&#128293; Start Boiler</button>"
        "<button class='btn-action btn-off' onclick=\"trigger('off')\">&#10060; Stop Boiler</button>"
        "</div>"
        "<div class='sec-title'>"
        "<span>Thermal Zones (Underfloor)</span>"
        "<span>Target</span>"
        "</div>"
    );

    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        const ThermalZoneConfig& cfg = thermalZones.getConfig(i);
        const ThermalZoneStatus& st = thermalZones.getStatus(i);

        String badgeClass = "badge-idle";
        if (st.state == ZONE_HEATING) badgeClass = "badge-heating";
        else if (st.state == ZONE_WARMING_UP) badgeClass = "badge-warming";
        else if (st.state == ZONE_DISABLED) badgeClass = "badge-disabled";

        html += "<div class='zone-card'>";
        html += "<div class='zone-header'>";
        html += "<span class='zone-name'>" + String(cfg.name) + "</span>";
        html += "<span class='zone-badge " + badgeClass + "'>" + String(thermalZones.getStateStr(st.state)) + "</span>";
        html += "</div>";

        html += "<div class='zone-body'>";
        html += "<div>";
        if (st.sensorValid) {
            html += "<span class='zone-temp'>" + String(st.currentTemp, 1) + "&deg;C</span>";
        } else {
            html += "<span class='zone-temp' style='color:#64748b;'>--.-&deg;C</span>";
        }
        html += "<div style='font-size:0.75rem;color:#64748b;margin-top:2px;'>Sensor: " + String(cfg.sensorName) + "</div>";
        html += "</div>";

        html += "<div class='zone-ctrl'>";
        html += "<button class='temp-btn' onclick=\"adjTarget(" + String(i) + ", -0.5)\">-</button>";
        html += "<span class='setpoint-val'>" + String(cfg.targetTemp, 1) + "&deg;C</span>";
        html += "<button class='temp-btn' onclick=\"adjTarget(" + String(i) + ", 0.5)\">+</button>";
        html += "</div>";
        html += "</div>";

        html += "<div class='zone-footer'>";
        html += "<span>Relay GPIO" + String(cfg.relayPin) + ": <b>" + String(st.relayEngaged ? "ON (Open)" : "OFF (Closed)") + "</b></span>";
        html += "<span class='toggle-link' onclick=\"toggleZone(" + String(i) + ")\">" + String(cfg.enabled ? "Disable" : "Enable") + "</span>";
        html += "</div>";
        html += "</div>";
    }

    html += F(
        "<div class='sec-title'>"
        "<span>BLE Sensor Telemetry</span>"
    );

    if (mqttTelemetry.isConnected()) {
        html += F("<span style='color:#34d399;font-size:0.75rem;'>&bull; MQTT Connected</span>");
    } else {
        html += F("<span style='color:#f87171;font-size:0.75rem;'>&bull; MQTT Disconnected</span>");
    }

    html += F("</div><div class='sensor-grid'>");

    size_t sCount = mqttTelemetry.getSensorCount();
    if (sCount == 0) {
        html += F("<div class='sensor-row'><span style='color:#64748b;'>Waiting for telemetry from Galaxy-A6...</span></div>");
    } else {
        for (size_t i = 0; i < sCount; i++) {
            const RoomSensor* s = mqttTelemetry.getSensor(i);
            if (!s) continue;
            html += "<div class='sensor-row'>";
            html += "<span class='sensor-name'>" + String(s->name) + "</span>";
            html += "<div><span class='sensor-temp'>" + String(s->temperature, 1) + "&deg;C</span>";
            html += "<span style='font-size:0.75rem;color:#64748b;margin-left:6px;'>" + String(s->humidity, 0) + "%</span></div>";
            html += "</div>";
        }
    }

    html += F(
        "</div>"
        "<div class='meta'>"
        "<div>Last Command: <b>"
    );
    html += (lastTx == 0) ? "None" : (String(secondsAgo) + " sec.");
    html += F(
        "</b></div>"
        "<div>Free Heap: <b>"
    );
    html += String(ESP.getFreeHeap() / 1024);
    html += F(
        " KB</b></div>"
        "<div>Wi-Fi RSSI: <b>"
    );
    html += String(WiFi.RSSI());
    html += F(
        " dBm</b></div>"
        "<div>IP: <b>"
    );
    html += WiFi.localIP().toString();
    html += F(
        "</b></div></div>"
        "<div class='footer' style='display:flex;justify-content:center;gap:16px;'><a href='console'><b>&gt;_ Live Wi-Fi Console</b></a> &bull; <a href='update'>Firmware OTA</a></div>"
        "</div>"
        "<script>"
        "const base = window.location.pathname.endsWith('/') ? window.location.pathname : window.location.pathname + '/';"
        "async function trigger(action){"
        "  await fetch(base + 'api/boiler/' + action, {method:'POST'});"
        "  setTimeout(()=>location.reload(), 400);"
        "}"
        "async function setMode(m){"
        "  await fetch(base + 'api/mode?mode=' + m, {method:'POST'});"
        "  setTimeout(()=>location.reload(), 200);"
        "}"
        "async function adjTarget(idx, delta){"
        "  const res = await fetch(base + 'api/zones');"
        "  const data = await res.json();"
        "  if(data.zones && data.zones[idx]){"
        "    const cur = data.zones[idx].targetTemp;"
        "    const nxt = (cur + delta).toFixed(1);"
        "    await fetch(base + 'api/zones/setpoint?id=' + idx + '&target=' + nxt, {method:'POST'});"
        "    setTimeout(()=>location.reload(), 200);"
        "  }"
        "}"
        "async function toggleZone(idx){"
        "  await fetch(base + 'api/zones/toggle?id=' + idx, {method:'POST'});"
        "  setTimeout(()=>location.reload(), 200);"
        "}"
        "</script>"
        "</body></html>"
    );

    server.send(200, "text/html", html);
}

void handleApiOn() {
    blinkLed(1, 40);
    boilerRadio.sendBoilerOn();
    blinkLed(1, 40);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"ON\"}"));
}

void handleApiOff() {
    blinkLed(1, 40);
    boilerRadio.sendBoilerOff();
    blinkLed(1, 40);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", F("{\"status\":\"ok\",\"boiler\":\"OFF\"}"));
}

void handleApiZones() {
    String json = "{";
    json += "\"autoMode\":" + String(thermalZones.isAutoMode() ? "true" : "false") + ",";
    json += "\"boiler\":\"" + String(boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\",";
    json += "\"boilerDemand\":" + String(thermalZones.isBoilerDemanded() ? "true" : "false") + ",";
    json += "\"zones\":[";

    for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
        const ThermalZoneConfig& cfg = thermalZones.getConfig(i);
        const ThermalZoneStatus& st = thermalZones.getStatus(i);
        if (i > 0) json += ",";
        json += "{";
        json += "\"id\":" + String(cfg.id) + ",";
        json += "\"name\":\"" + String(cfg.name) + "\",";
        json += "\"sensor\":\"" + String(cfg.sensorName) + "\",";
        json += "\"relayPin\":" + String(cfg.relayPin) + ",";
        json += "\"targetTemp\":" + String(cfg.targetTemp, 1) + ",";
        json += "\"hysteresis\":" + String(cfg.hysteresis, 2) + ",";
        json += "\"enabled\":" + String(cfg.enabled ? "true" : "false") + ",";
        json += "\"currentTemp\":" + String(st.currentTemp, 1) + ",";
        json += "\"sensorValid\":" + String(st.sensorValid ? "true" : "false") + ",";
        json += "\"demand\":" + String(st.demand ? "true" : "false") + ",";
        json += "\"relayEngaged\":" + String(st.relayEngaged ? "true" : "false") + ",";
        json += "\"state\":\"" + String(thermalZones.getStateStr(st.state)) + "\"";
        json += "}";
    }
    json += "]}";

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void handleApiZoneSetpoint() {
    if (!server.hasArg("id") || !server.hasArg("target")) {
        server.send(400, "application/json", F("{\"error\":\"Missing parameters (id and target)\"}"));
        return;
    }
    size_t id = server.arg("id").toInt();
    float target = server.arg("target").toFloat();
    thermalZones.setTargetTemperature(id, target);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", F("{\"status\":\"ok\"}"));
}

void handleApiZoneToggle() {
    if (!server.hasArg("id")) {
        server.send(400, "application/json", F("{\"error\":\"Missing parameter (id)\"}"));
        return;
    }
    size_t id = server.arg("id").toInt();
    if (id < thermalZones.getZoneCount()) {
        bool current = thermalZones.getConfig(id).enabled;
        thermalZones.setZoneEnabled(id, !current);
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", F("{\"status\":\"ok\"}"));
    } else {
        server.send(404, "application/json", F("{\"error\":\"Zone not found\"}"));
    }
}

void handleApiMode() {
    if (!server.hasArg("mode")) {
        server.send(400, "application/json", F("{\"error\":\"Missing parameter (mode)\"}"));
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
        server.send(400, "application/json", F("{\"error\":\"Missing parameters (id and sensor)\"}"));
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

void handleApiStatus() {
    String json = "{";
    json += "\"boiler\":\"" + String(boilerRadio.isLastCommandOn() ? "ON" : "OFF") + "\",";
    json += "\"lastTxMs\":" + String(boilerRadio.getLastTransmissionTime()) + ",";
    json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptimeSec\":" + String(millis() / 1000) + ",";
    json += "\"mqttConnected\":" + String(mqttTelemetry.isConnected() ? "true" : "false") + ",";
    json += "\"autoMode\":" + String(thermalZones.isAutoMode() ? "true" : "false") + ",";
    json += "\"boilerDemand\":" + String(thermalZones.isBoilerDemanded() ? "true" : "false") + ",";
    json += "\"sensors\":[";
    size_t count = mqttTelemetry.getSensorCount();
    for (size_t i = 0; i < count; i++) {
        const RoomSensor* s = mqttTelemetry.getSensor(i);
        if (!s) continue;
        if (i > 0) json += ",";
        json += "{";
        json += "\"name\":\"" + String(s->name) + "\",";
        json += "\"mac\":\"" + String(s->mac) + "\",";
        json += "\"temp\":" + String(s->temperature, 2) + ",";
        json += "\"hum\":" + String(s->humidity, 1) + ",";
        json += "\"bat\":" + String(s->battery) + ",";
        json += "\"rssi\":" + String(s->rssi) + ",";
        json += "\"ageSec\":" + String((millis() - s->lastSeenMs) / 1000);
        json += "}";
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
        Serial.printf("[WiFi] Connected successfully! Local IP: %s\n", WiFi.localIP().toString().c_str());
        digitalWrite(PIN_STATUS_LED, LOW); // LED off when connected
    } else {
        Serial.println(F("[WiFi] Could not connect to router within 12s. Starting Fallback Access Point..."));
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID, AP_PASS);
        softApActive = true;
        Serial.printf("[WiFi] AP active: '%s', Password: '%s', IP: %s\n", AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());
    }
}

void handleSerialCommand(String cmd) {
    cmd.trim();
    if (cmd.length() == 0) return;

    String upperCmd = cmd;
    upperCmd.toUpperCase();

    if (upperCmd == "1" || upperCmd == "ON" || upperCmd == "START") {
        blinkLed(2, 40);
        boilerRadio.sendBoilerOn();
        netConsole.println(F("[BOILER] Command ON sent successfully via RF 433.92 MHz."));
    } else if (upperCmd == "0" || upperCmd == "OFF" || upperCmd == "STOP") {
        blinkLed(2, 40);
        boilerRadio.sendBoilerOff();
        netConsole.println(F("[BOILER] Command OFF sent successfully via RF 433.92 MHz."));
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
            netConsole.printf("[ZONE] Zone %u setpoint updated to %.1f C\n", (unsigned int)(idx + 1), t);
        } else {
            netConsole.println(F("[ZONE] Syntax: SET <zone 1..3> <temp> (e.g. SET 1 22.5)"));
        }
    } else if (upperCmd == "S" || upperCmd == "STATUS") {
        netConsole.printf("[STATUS] Boiler State: %s | Heat Demand: %s | Mode: %s | IP: %s | Heap: %u B\n",
            boilerRadio.isLastCommandOn() ? "ON" : "OFF",
            thermalZones.isBoilerDemanded() ? "DEMAND" : "IDLE",
            thermalZones.isAutoMode() ? "AUTOMATIC" : "MANUAL",
            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : WiFi.softAPIP().toString().c_str(),
            ESP.getFreeHeap()
        );
        for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
            const ThermalZoneConfig& cfg = thermalZones.getConfig(i);
            const ThermalZoneStatus& st = thermalZones.getStatus(i);
            netConsole.printf("  [Zone %u] %-20s: %.1f C / %.1f C (Target) | State: %-18s | Relay GPIO%d: %s\n",
                cfg.id, cfg.name, st.currentTemp, cfg.targetTemp,
                thermalZones.getStateStr(st.state), cfg.relayPin,
                st.relayEngaged ? "ON" : "OFF"
            );
        }
        size_t sc = mqttTelemetry.getSensorCount();
        netConsole.printf("[STATUS] MQTT BLE Sensors (%u):\n", sc);
        for (size_t i = 0; i < sc; i++) {
            const RoomSensor* s = mqttTelemetry.getSensor(i);
            if (s) {
                netConsole.printf("  - %s (%s): %.1f C, %.1f %%, Bat: %d %%\n",
                    s->name, s->mac, s->temperature, s->humidity, s->battery);
            }
        }
    } else if (upperCmd == "?" || upperCmd == "HELP") {
        printHelp();
    } else {
        netConsole.printf("[Unknown command: '%s']. Type '?' for commands.\n", cmd.c_str());
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);

    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, LOW); // Off (active HIGH)

    Serial.printf("[Hardware] Initializing RF transmitter on GPIO%d...\n", PIN_RF_DATA);
    boilerRadio.begin(PIN_RF_DATA);

    // Initialize Multi-Zone Thermal Controller (NC actuator relays)
    thermalZones.begin();

    setupWifi();

    // Initialize 2-Way Network Console (Telnet :23 and Web /console)
    netConsole.begin(&server);
    netConsole.setCommandHandler(handleSerialCommand);

    // Initialize MQTT Telemetry (Galaxy-A6 Mosquitto)
    mqttTelemetry.begin();

    // Web Server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/boiler/on", HTTP_POST, handleApiOn);
    server.on("/api/boiler/off", HTTP_POST, handleApiOff);
    server.on("/api/boiler/status", HTTP_GET, handleApiStatus);
    server.on("/api/zones", HTTP_GET, handleApiZones);
    server.on("/api/zones/setpoint", HTTP_POST, handleApiZoneSetpoint);
    server.on("/api/zones/toggle", HTTP_POST, handleApiZoneToggle);
    server.on("/api/zones/sensor", HTTP_POST, handleApiZoneSensor);
    server.on("/api/mode", HTTP_POST, handleApiMode);

    // Enable Web OTA on /update (admin / admin123)
    httpUpdater.setup(&server, "/update", OTA_USER, OTA_PASS);
    server.begin();

    printHelp();
    netConsole.println(F("[HTTP] Web Server, Web Console, and Web OTA running on port 80."));
    netConsole.println(F("[Ready] System ready for commands."));
}

void loop() {
    server.handleClient();
    netConsole.update();
    mqttTelemetry.update();
    thermalZones.update(mqttTelemetry, boilerRadio);

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
