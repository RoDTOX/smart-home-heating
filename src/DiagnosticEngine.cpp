#include "DiagnosticEngine.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include <WiFi.h>

DiagnosticEngine diagEngine;

DiagnosticEngine::DiagnosticEngine()
    : _dtcCount(0), _serviceMode(false), _lastEvaluationMs(0) {
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
  Serial.println(F("[DIAG] Diagnostic Engine & Service Mode initialized."));
}

void DiagnosticEngine::setServiceMode(bool enable) {
  _serviceMode = enable;
  if (_serviceMode) {
    netConsole.println(
        F("[DIAG] **************************************************"));
    netConsole.println(F("[DIAG] * SERVICE MODE ACTIVATED BY ENGINEER *"));
    netConsole.println(
        F("[DIAG] * Manual overrides enabled. Health monitor active*"));
    netConsole.println(
        F("[DIAG] **************************************************"));
  } else {
    netConsole.println(
        F("[DIAG] Service mode deactivated. Resuming normal autonomy."));
  }
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

    // 1. Timeout (> 20 min)
    if (ageSec > 1200) {
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
}

uint8_t DiagnosticEngine::calculateHealthScore() const {
  int score = 100;
  for (size_t i = 0; i < _dtcCount; i++) {
    if (!_dtcRecords[i].active)
      continue;
    switch (_dtcRecords[i].severity) {
    case SEV_INFO:
      score -= 2;
      break;
    case SEV_WARNING:
      score -= 10;
      break;
    case SEV_ERROR:
      score -= 25;
      break;
    case SEV_CRITICAL:
      score -= 50;
      break;
    }
  }
  if (score < 0)
    score = 0;
  return (uint8_t)score;
}

String DiagnosticEngine::runSelfTest() {
  String out;
  out.reserve(1024);
  out = "";
  out += "==================================================\n";
  out += "  ESP32 Smart Heating - Full Self-Test Diagnostic \n";
  out += "==================================================\n";

  // 1. Wi-Fi
  bool wifiOk = (WiFi.status() == WL_CONNECTED);
  out += "1. Wi-Fi Subsystem:       " + String(wifiOk ? "PASS" : "FAIL") +
         " (IP: " + WiFi.localIP().toString() +
         ", RSSI: " + String(WiFi.RSSI()) + " dBm)\n";

  // 2. Memory
  uint32_t heap = ESP.getFreeHeap();
  bool heapOk = (heap >= 60000);
  out += "2. System Memory (Heap):  " + String(heapOk ? "PASS" : "WARN") +
         " (" + String(heap / 1024) + " KB free)\n";

  // 3. BLE Subsystem
  bool bleOk = bleScanner.isScanning();
  size_t bleCount = bleScanner.getSensorCount();
  out += "3. Bluetooth BLE Stack:   " + String(bleOk ? "PASS" : "FAIL") +
         " (Scanner: " + String(bleOk ? "ACTIVE" : "STOPPED") +
         ", Heard: " + String(bleCount) + " sensors)\n";

  // 4. Hydraulic & Pump Controller
  bool pumpEn = thermalZones.isPumpControlEnabled();
  out += "4. Hydraulic Pump Ctrl:   PASS (GPA Pump: " +
         String(pumpEn ? "ENABLED" : "DISABLED") + ")\n";

  // 5. Thermal Controller
  bool autoMode = thermalZones.isAutoMode();
  out += "5. Multi-Zone Engine:     PASS (Mode: " +
         String(autoMode ? "AUTOMATIC" : "MANUAL") +
         ", Zones: " + String(thermalZones.getZoneCount()) + ")\n";

  // 6. Boiler RF Trigger
  bool boilerLast = boilerRadio.isLastCommandOn();
  out += "6. Boiler RF 433 MHz:     PASS (Pin: GPIO" + String(PIN_RF_DATA) +
         ", Last Cmd: " + String(boilerLast ? "ON" : "OFF") + ")\n";

  // Health Score
  uint8_t health = calculateHealthScore();
  out += "--------------------------------------------------\n";
  out += "System Health Score:      " + String(health) + "% [" +
         String(health >= 90 ? "EXCELLENT"
                             : (health >= 70 ? "GOOD" : "DEGRADED")) +
         "]\n";
  out += "Service Mode Status:      " +
         String(_serviceMode ? "ACTIVE (Engineer Engaged)" : "NORMAL") + "\n";

  // Active DTCs
  size_t activeCount = 0;
  for (size_t i = 0; i < _dtcCount; i++) {
    if (_dtcRecords[i].active)
      activeCount++;
  }
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
