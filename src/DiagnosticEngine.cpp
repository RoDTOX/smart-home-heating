#include "DiagnosticEngine.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include <WiFi.h>

DiagnosticEngine diagEngine;

DiagnosticEngine::DiagnosticEngine()
    : _dtcCount(0), _lastEvaluationMs(0) {
  for (size_t i = 0; i < MAX_DTC_ENTRIES; i++) {
    _dtcRecords[i].code[0] = '\0';
    _dtcRecords[i].message[0] = '\0';
    _dtcRecords[i].severity = SEV_INFO;
    _dtcRecords[i].firstRaisedMs = 0;
    _dtcRecords[i].lastRaisedMs = 0;
    _dtcRecords[i].occurrenceCount = 0;
    _dtcRecords[i].active = false;
  }
}

void DiagnosticEngine::begin() {
  Serial.println(F("[DIAG] Diagnostic Engine initialized."));
}

void DiagnosticEngine::raiseDtc(const char *code, const char *message,
                                DtcSeverity severity) {
  if (!code || strlen(code) == 0)
    return;

  DtcRecord *target = nullptr;
  for (size_t i = 0; i < _dtcCount; i++) {
    if (strcasecmp(_dtcRecords[i].code, code) == 0) {
      target = &_dtcRecords[i];
      break;
    }
  }

  if (!target && _dtcCount < MAX_DTC_ENTRIES) {
    target = &_dtcRecords[_dtcCount++];
    strncpy(target->code, code, sizeof(target->code) - 1);
    target->code[sizeof(target->code) - 1] = '\0';
    target->firstRaisedMs = millis();
    target->occurrenceCount = 0;
  }

  if (target) {
    strncpy(target->message, message, sizeof(target->message) - 1);
    target->message[sizeof(target->message) - 1] = '\0';
    target->severity = severity;
    target->lastRaisedMs = millis();
    target->occurrenceCount++;

    if (!target->active) {
      target->active = true;
      netConsole.printf("[DIAG-ALERT] %s [%s]: %s\n", target->code,
                        getSeverityStr(target->severity), target->message);
    }
  }
}

void DiagnosticEngine::clearDtc(const char *code) {
  if (!code)
    return;
  for (size_t i = 0; i < _dtcCount; i++) {
    if (strcasecmp(_dtcRecords[i].code, code) == 0) {
      _dtcRecords[i].active = false;
      netConsole.printf("[DIAG] Cleared code %s\n", code);
      break;
    }
  }
}

void DiagnosticEngine::clearAllDtcs() {
  for (size_t i = 0; i < _dtcCount; i++) {
    _dtcRecords[i].active = false;
  }
  netConsole.println(F("[DIAG] All active DTC codes cleared."));
}

const DtcRecord *DiagnosticEngine::getDtc(size_t index) const {
  if (index < _dtcCount) {
    return &_dtcRecords[index];
  }
  return nullptr;
}

const char *DiagnosticEngine::getSeverityStr(DtcSeverity sev) const {
  switch (sev) {
  case SEV_INFO:
    return "INFO";
  case SEV_WARNING:
    return "WARNING";
  case SEV_ERROR:
    return "ERROR";
  case SEV_CRITICAL:
    return "CRITICAL";
  default:
    return "UNKNOWN";
  }
}

void DiagnosticEngine::update() {
  uint32_t now = millis();
  if (now - _lastEvaluationMs < 10000UL) {
    return;
  }
  _lastEvaluationMs = now;

  evaluateSensors();
  evaluateSystem();
}

void DiagnosticEngine::evaluateSensors() {
  size_t count = bleScanner.getSensorCount();
  for (size_t i = 0; i < count; i++) {
    const BleSensorData *s = bleScanner.getSensor(i);
    if (!s || !s->valid)
      continue;

    uint32_t ageSec = (millis() - s->lastSeenMs) / 1000;

    // 1. Timeout (> 20 min), only for the sensors a zone uses (a neighbour's sensor in a search list is no fault)
    if (ageSec > 1200 && bleScanner.isAssignedSensor(s)) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s timeout (%u min)", s->name,
               (unsigned int)(ageSec / 60));
      raiseDtc("DTC-101", buf, SEV_ERROR);
    }

    // 2. Low battery (< 20%)
    if (s->battery > 0 && s->battery < 20) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s battery low (%u%%)", s->name, s->battery);
      raiseDtc("DTC-102", buf, SEV_WARNING);
    }

    // 3. Spurious out of range
    if (s->temperature < 5.0f || s->temperature > 45.0f) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s abnormal temp (%.1f C)", s->name,
               s->temperature);
      raiseDtc("DTC-103", buf, SEV_ERROR);
    }
  }
}

void DiagnosticEngine::evaluateSystem() {
  // Wi-Fi signal check
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    if (rssi < -82) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Wi-Fi signal degraded (%d dBm)", rssi);
      raiseDtc("DTC-401", buf, SEV_WARNING);
    } else if (rssi >= -78) {
      clearDtc("DTC-401");
    }
  } else {
    raiseDtc("DTC-401", "Wi-Fi link disconnected", SEV_WARNING);
  }

  // Free heap memory check (hysteresis: raise at <60 KB, clear at >75 KB)
  uint32_t heap = ESP.getFreeHeap();
  if (heap < 60000) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Free heap memory low (%u KB)",
             (unsigned int)(heap / 1024));
    raiseDtc("DTC-402", buf, SEV_WARNING);
  } else if (heap > 75000) {
    clearDtc("DTC-402");
  }

  // Learned value discarded at load (NFR-5.3); cleared only manually (DIAG CLEAR)
  static bool learningResetRaised = false;
  if (paramStore.learnedResets() > 0 && !learningResetRaised) {
    learningResetRaised = true;
    raiseDtc("DTC-301", "Learned duty out of bounds, reset", SEV_WARNING);
  }
}

static void setCheck(DiagCheck &c, const char *id, const char *rule,
                     const char *result) {
  c.id = id;
  c.rule = rule;
  c.result = result;
  c.value[0] = '\0';
}

// FR-10.4: each check states its rule; nothing is reported as OK that was not measured
void DiagnosticEngine::runChecks(DiagCheck (&out)[DIAG_CHECK_COUNT]) const {
  bool wifiUp = (WiFi.status() == WL_CONNECTED);
  int rssi = wifiUp ? WiFi.RSSI() : 0;
  setCheck(out[0], "wifi", "Wi-Fi up and RSSI >= -82 dBm (DTC-401)",
           wifiUp && rssi >= -82 ? "OK" : "FAIL");
  if (wifiUp)
    snprintf(out[0].value, sizeof(out[0].value), "%d dBm", rssi);
  else
    strlcpy(out[0].value, "disconnected", sizeof(out[0].value));

  uint32_t heap = ESP.getFreeHeap();
  setCheck(out[1], "heap", "free heap >= 60 KB (DTC-402)",
           heap >= 60000 ? "OK" : "FAIL");
  snprintf(out[1].value, sizeof(out[1].value), "%u KB", (unsigned int)(heap / 1024));

  bool scanning = bleScanner.isScanning();
  setCheck(out[2], "ble", "BLE scan running", scanning ? "OK" : "FAIL");
  strlcpy(out[2].value, scanning ? "scanning" : "stopped", sizeof(out[2].value));

  size_t on = 0, withReading = 0;
  for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
    if (!thermalZones.getConfig(i).enabled)
      continue;
    on++;
    const ThermalZoneStatus &st = thermalZones.getStatus(i);
    if (st.sensorValid && !st.sensorFault)
      withReading++;
  }
  setCheck(out[3], "sensors",
           "every switched-on zone has a reading within sensorTimeout "
           "(DTC-101); INFO while no zone is on",
           on == 0 ? "INFO" : (withReading == on ? "OK" : "FAIL"));
  snprintf(out[3].value, sizeof(out[3].value), "%u/%u", (unsigned int)withReading,
           (unsigned int)on);

  bool synced = timeAndWeather.isTimeSynced();
  setCheck(out[4], "time", "clock synchronised", synced ? "OK" : "FAIL");
  strlcpy(out[4].value, synced ? timeAndWeather.getFormattedTime().c_str() : "not synced",
          sizeof(out[4].value));

  uint32_t age = timeAndWeather.getWeatherAgeSec();
  setCheck(out[5], "weather", "outdoor temperature not older than 6 h (FR-9)",
           age <= 6UL * 3600UL ? "OK" : "FAIL");
  if (age == 999999)
    strlcpy(out[5].value, "never received", sizeof(out[5].value));
  else
    snprintf(out[5].value, sizeof(out[5].value), "%u min old", (unsigned int)(age / 60));

  setCheck(out[6], "radio",
           "no return channel: the boiler's reaction cannot be checked", "INFO");
  uint32_t tx = boilerRadio.getLastTransmissionTime();
  if (tx == 0)
    strlcpy(out[6].value, "no frame since boot", sizeof(out[6].value));
  else
    snprintf(out[6].value, sizeof(out[6].value), "last %s, %u s ago",
             boilerRadio.isLastCommandOn() ? "ON" : "OFF",
             (unsigned int)((millis() - tx) / 1000));
}

String DiagnosticEngine::checksJson() const {
  DiagCheck checks[DIAG_CHECK_COUNT];
  runChecks(checks);
  String json;
  json.reserve(900);
  json = "[";
  for (size_t i = 0; i < DIAG_CHECK_COUNT; i++) {
    if (i > 0)
      json += ",";
    json += "{\"id\":\"";
    json += checks[i].id;
    json += "\",\"result\":\"";
    json += checks[i].result;
    json += "\",\"value\":\"";
    json += checks[i].value;
    json += "\",\"rule\":\"";
    json += checks[i].rule;
    json += "\"}";
  }
  json += "]";
  return json;
}

size_t DiagnosticEngine::getActiveDtcCount() const {
  size_t n = 0;
  for (size_t i = 0; i < _dtcCount; i++) {
    if (_dtcRecords[i].active)
      n++;
  }
  return n;
}

String DiagnosticEngine::runSelfTest() {
  String out;
  out.reserve(1024);
  out = "";
  out += "==================================================\n";
  out += "  Smart Heating - basic self-test (FR-10.4)\n";
  out += "==================================================\n";

  DiagCheck checks[DIAG_CHECK_COUNT];
  runChecks(checks);
  char line[160];
  for (size_t i = 0; i < DIAG_CHECK_COUNT; i++) {
    snprintf(line, sizeof(line), "%-8s %-5s %-20s %s\n", checks[i].id,
             checks[i].result, checks[i].value, checks[i].rule);
    out += line;
  }

  // Active DTCs
  size_t activeCount = getActiveDtcCount();
  out += "--------------------------------------------------\n";
  out += "Active Diagnostic DTCs:   " + String(activeCount) + "\n";
  for (size_t i = 0; i < _dtcCount; i++) {
    if (_dtcRecords[i].active) {
      out += "  * [" + String(_dtcRecords[i].code) + "] " +
             String(getSeverityStr(_dtcRecords[i].severity)) + ": " +
             String(_dtcRecords[i].message) + " (x" +
             String(_dtcRecords[i].occurrenceCount) + ")\n";
    }
  }
  out += "==================================================\n";
  return out;
}
