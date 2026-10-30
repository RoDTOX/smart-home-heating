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
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>


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
      "  OFFSET <z> <C>        -> Sensor calibration offset for zone (e.g. OFFSET 2 -0.3)\r\n"
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
  } else if (upperCmd.startsWith("OFFSET ")) {
    unsigned z;
    float ofs;
    if (sscanf(cmd.c_str() + 7, "%u %f", &z, &ofs) != 2 || z < 1 || !thermalZones.setTempOffset(z - 1, ofs)) {
      netConsole.println(F("[OFFSET] Syntax: OFFSET <zone 1..3> <-3.0..+3.0>  (added to sensor reading)"));
    }
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


