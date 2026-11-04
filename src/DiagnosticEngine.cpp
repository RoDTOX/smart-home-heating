#include "Commands.h"
#include "DiagnosticEngine.h"
#include "EventLog.h"
#include "BleSensorScanner.h"
#include "BoilerRadioTrigger.h"
#include "NetworkConsole.h"
#include "ThermalZoneManager.h"
#include "TimeAndWeatherService.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_system.h>

DiagnosticEngine diagEngine;

// Requirements section 5: code text and whether the code is per zone (zone mask), same order as enum Dtc
static const struct {
  const char *code;
  bool perZone;
} DTC_TABLE[DTC_COUNT] = {
    {"DTC-101", true},  {"DTC-102", true},  {"DTC-103", true},  {"DTC-104", true},
    {"DTC-105", true},  {"DTC-201", false}, {"DTC-202", true},  {"DTC-203", true},
    {"DTC-204", false}, {"DTC-205", false}, {"DTC-206", false}, {"DTC-207", false},
    {"DTC-208", false}, {"DTC-301", false}, {"DTC-302", false}, {"DTC-303", false},
    {"DTC-304", false}, {"DTC-401", false}, {"DTC-402", false}, {"DTC-403", false}};

static const char *EVENT_FILE = "/events.bin";
static const uint16_t EVENT_MAGIC = 0x5645;  // "EV", format 1
static const size_t EVENT_HEADER = 8;
static const uint8_t EVENT_NO_CODE = 0xFF;
static const uint8_t EVENT_REPEAT_LIMIT = 6;  // FR-62: same code and zone, per hour
static const uint32_t EVENT_REPEAT_WINDOW_S = 3600;

DiagnosticEngine::DiagnosticEngine()
    : _lastEvaluationMs(0), _evHead(0), _evCount(0), _evDirty(0), _fsMounted(false) {
  for (size_t i = 0; i < DTC_COUNT; i++) {
    strlcpy(_dtcRecords[i].code, DTC_TABLE[i].code, sizeof(_dtcRecords[i].code));
    _dtcRecords[i].message[0] = '\0';
    _dtcRecords[i].severity = SEV_INFO;
    _dtcRecords[i].firstRaisedMs = 0;
    _dtcRecords[i].lastRaisedMs = 0;
    _dtcRecords[i].occurrenceCount = 0;
    _dtcRecords[i].active = false;
    _dtcRecords[i].zoneMask = 0;
  }
  memset(_events, 0, sizeof(_events));
}

void DiagnosticEngine::begin() {
  _minHeapSinceCheck = ESP.getFreeHeap();
  eventLog.begin();  // FR-62.1
  loadEvents();
  recordRestart((uint32_t)esp_reset_reason());
  Serial.println(F("[DIAG] Diagnostic Engine initialized."));
}

// FR-62: events survive a reboot; without LittleFS they stay in RAM until reboot
void DiagnosticEngine::loadEvents() {
  _fsMounted = LittleFS.begin(true);
  if (!_fsMounted) {
    netConsole.println(F("[DIAG][WARN] LittleFS not mounted, events stay in RAM until reboot"));
    return;
  }
  File f = LittleFS.open(EVENT_FILE, "r");
  if (!f)
    return;
  uint8_t hdr[EVENT_HEADER];
  if (f.read(hdr, EVENT_HEADER) == (int)EVENT_HEADER &&
      (uint16_t)(hdr[0] | (hdr[1] << 8)) == EVENT_MAGIC && hdr[2] < EVENT_RING_SIZE &&
      hdr[3] <= EVENT_RING_SIZE &&
      f.read((uint8_t *)_events, sizeof(_events)) == (int)sizeof(_events)) {
    _evHead = hdr[2];
    _evCount = hdr[3];
  } else {
    memset(_events, 0, sizeof(_events));
  }
  f.close();
}

// One record per event; the header (head, count) is written with it. Called from loop().
void DiagnosticEngine::flushEvents() {
  if (!_evDirty || !_fsMounted)
    return;
  File f = LittleFS.open(EVENT_FILE, LittleFS.exists(EVENT_FILE) ? "r+" : "w");
  if (!f) {
    netConsole.println(F("[DIAG][WARN] event file not writable, events stay in RAM"));
    _evDirty = 0;
    return;
  }
  if (f.size() < EVENT_HEADER + sizeof(_events)) {
    uint8_t zero[EVENT_HEADER + sizeof(_events)] = {0};
    f.seek(0);
    f.write(zero, sizeof(zero));
  }
  for (uint8_t slot = 0; slot < EVENT_RING_SIZE; slot++) {
    if (_evDirty & ((uint64_t)1 << slot)) {
      f.seek(EVENT_HEADER + slot * sizeof(EventRecord));
      if (f.write((const uint8_t *)&_events[slot], sizeof(EventRecord)) != sizeof(EventRecord))
        netConsole.println(F("[DIAG][WARN] event write failed, events stay in RAM"));
    }
  }
  uint8_t hdr[EVENT_HEADER] = {(uint8_t)(EVENT_MAGIC & 0xFF), (uint8_t)(EVENT_MAGIC >> 8),
                               _evHead, _evCount, 0, 0, 0, 0};
  f.seek(0);
  f.write(hdr, EVENT_HEADER);
  f.close();
  _evDirty = 0;
}

void DiagnosticEngine::addEvent(EventType type, uint8_t code, uint8_t zone, uint32_t value) {
  if (type != EVT_RESTART)  // FR-62.1: every raise and clear, without the hourly limit of the 50-entry ring
    eventLog.add(EV_DTC, code, RS_NONE, zone, 0, type == EVT_DTC_RAISED ? 1 : 0);
  time_t epoch = time(nullptr);
  bool epochValid = epoch > 1700000000;
  uint32_t now = epochValid ? (uint32_t)epoch : millis() / 1000;
  uint8_t flags = epochValid ? 1 : 0;

  // At most EVENT_REPEAT_LIMIT records per hour for one code and zone; repeats are counted in the last one
  if (type != EVT_RESTART) {
    uint8_t seen = 0;
    int8_t lastSlot = -1;
    for (uint8_t i = 0; i < _evCount; i++) {
      uint8_t slot = (_evHead + EVENT_RING_SIZE - 1 - i) % EVENT_RING_SIZE;
      const EventRecord &e = _events[slot];
      if (e.type != type || e.code != code || e.zone != zone || e.flags != flags)
        continue;
      if (now - e.time > EVENT_REPEAT_WINDOW_S)
        break;
      if (lastSlot < 0)
        lastSlot = slot;
      seen++;
    }
    if (seen >= EVENT_REPEAT_LIMIT) {
      _events[lastSlot].value++;  // RAM only: no flash write for a repeat (written with the next record)
      return;
    }
  }

  EventRecord &r = _events[_evHead];
  memset(&r, 0, sizeof(r));
  r.time = now;
  r.value = value;
  r.type = type;
  r.code = code;
  r.zone = zone;
  r.flags = flags;
  _evDirty |= (uint64_t)1 << _evHead;
  _evHead = (_evHead + 1) % EVENT_RING_SIZE;
  if (_evCount < EVENT_RING_SIZE)
    _evCount++;
}

void DiagnosticEngine::recordRestart(uint32_t resetReason) {
  addEvent(EVT_RESTART, EVENT_NO_CODE, 0, resetReason);
}

String DiagnosticEngine::eventsJson() const {
  static const char *TYPES[] = {"dtc-raised", "dtc-cleared", "restart"};
  String json;
  json.reserve(96 + _evCount * 110);
  json = "{\"count\":" + String(_evCount) + ",\"events\":[";
  for (uint8_t i = 0; i < _evCount; i++) {
    const EventRecord &e = _events[(_evHead + EVENT_RING_SIZE - 1 - i) % EVENT_RING_SIZE];
    if (i > 0)
      json += ",";
    json += "{\"time\":" + String(e.time);
    json += String(",\"timeValid\":") + ((e.flags & 1) ? "true" : "false");
    json += String(",\"type\":\"") + TYPES[e.type < 3 ? e.type : 0] + "\"";
    json += String(",\"code\":\"") + (e.code < DTC_COUNT ? DTC_TABLE[e.code].code : "") + "\"";
    json += ",\"zone\":" + String(e.zone);
    json += ",\"value\":" + String(e.value) + "}";
  }
  json += "]}";
  return json;
}

void DiagnosticEngine::raiseDtc(Dtc dtc, const char *message, DtcSeverity severity,
                                uint8_t zone) {
  size_t idx = (size_t)dtc;
  if (idx >= DTC_COUNT)
    return;
  DtcRecord &t = _dtcRecords[idx];
  bool wasActive = t.active;
  bool newZone = false;
  if (DTC_TABLE[idx].perZone && zone >= 1 && zone <= 32) {
    uint32_t bit = (uint32_t)1 << (zone - 1);
    newZone = !(t.zoneMask & bit);
    t.zoneMask |= bit;
  } else {
    zone = 0;
  }

  if (t.occurrenceCount == 0)
    t.firstRaisedMs = millis();
  strlcpy(t.message, message ? message : "", sizeof(t.message));
  t.severity = severity;
  t.lastRaisedMs = millis();
  t.occurrenceCount++;
  t.active = true;

  if (!wasActive) {
    netConsole.printf("[DIAG-ALERT] %s [%s]: %s\n", t.code, getSeverityStr(t.severity),
                      t.message);
  }
  if (!wasActive || newZone)
    addEvent(EVT_DTC_RAISED, (uint8_t)idx, zone, 0);
}

void DiagnosticEngine::clearDtc(Dtc dtc, uint8_t zone) {
  size_t idx = (size_t)dtc;
  if (idx >= DTC_COUNT || !_dtcRecords[idx].active)
    return;
  DtcRecord &t = _dtcRecords[idx];
  if (!DTC_TABLE[idx].perZone || zone > 32)
    zone = 0;
  uint32_t bit = zone ? ((uint32_t)1 << (zone - 1)) : 0;
  if (bit && (t.zoneMask & bit)) {
    t.zoneMask &= ~bit;
    addEvent(EVT_DTC_CLEARED, (uint8_t)idx, zone, 0);
    if (t.zoneMask)
      return;  // another zone still has the code
  } else if (bit && t.zoneMask) {
    return;  // this zone does not hold the code, others do
  } else {
    t.zoneMask = 0;
    addEvent(EVT_DTC_CLEARED, (uint8_t)idx, zone, 0);
  }
  t.active = false;
  netConsole.printf("[DIAG] Cleared code %s\n", t.code);
}

void DiagnosticEngine::clearAllDtcs() {
  for (size_t i = 0; i < DTC_COUNT; i++) {
    clearDtc((Dtc)i);
  }
  netConsole.println(F("[DIAG] All active DTC codes cleared."));
  evaluateSystem();
}

const DtcRecord *DiagnosticEngine::getDtc(size_t index) const {
  if (index < DTC_COUNT) {
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
  uint32_t cur = ESP.getFreeHeap();
  if (_minHeapSinceCheck == 0 || cur < _minHeapSinceCheck) _minHeapSinceCheck = cur;
  flushEvents();
  eventLog.update();
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

    // The sensor timeout (DTC-101) is raised by the zone (one `sensorTimeout`, FR-16.2, FR-18); readings outside the
    // limits and spikes (DTC-103, DTC-104) are judged when they arrive (BleSensorScanner, FR-53).

    // Low battery (< 20%)
    if (s->battery > 0 && s->battery < 20) {
      char buf[48];
      snprintf(buf, sizeof(buf), "%s battery low (%u%%)", s->name, s->battery);
      raiseDtc(Dtc::WARN_BATTERY_LOW, buf, SEV_WARNING);
    }
  }
}

void DiagnosticEngine::evaluateSystem() {
  // FR-61: without valid time the zones keep their last schedule level
  if (timeAndWeather.isTimeSynced()) {
    static bool syncLogged = false;
    if (!syncLogged) {  // FR-62.1: dates the entries written before the clock was valid
      syncLogged = true;
      uint32_t up = millis() / 1000;
      eventLog.add(EV_TIME_SYNC, 0, RS_NONE, 0, (uint16_t)(up >> 16), (uint16_t)(up & 0xFFFF));
    }
    clearDtc(Dtc::WARN_TIME_NOT_SET);
  } else if (millis() > 120000UL) {
    raiseDtc(Dtc::WARN_TIME_NOT_SET, "Time not set (NTP)", SEV_WARNING);
  }

  // Wi-Fi signal check
  if (WiFi.status() == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    if (rssi < -82) {
      char buf[48];
      snprintf(buf, sizeof(buf), "Wi-Fi signal degraded (%d dBm)", rssi);
      raiseDtc(Dtc::WARN_WIFI_SIGNAL_DEGRADED, buf, SEV_WARNING);
    } else if (rssi >= -78) {
      clearDtc(Dtc::WARN_WIFI_SIGNAL_DEGRADED);
    }
  } else {
    raiseDtc(Dtc::WARN_WIFI_SIGNAL_DEGRADED, "Wi-Fi link disconnected", SEV_WARNING);
  }

  // Free heap memory check (NFR-2.2, PB-122: hysteresis: raise at <60 KB or dip <60 KB, clear at >75 KB)
  uint32_t heap = ESP.getFreeHeap();
  if (_minHeapSinceCheck == 0 || heap < _minHeapSinceCheck) _minHeapSinceCheck = heap;
  if (heap < 60000 || _minHeapSinceCheck < 60000) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Free heap memory low (%u KB, dip %u KB)",
             (unsigned int)(heap / 1024), (unsigned int)(_minHeapSinceCheck / 1024));
    raiseDtc(Dtc::WARN_HEAP_FRAGMENTATION, buf, SEV_WARNING);
  } else if (heap > 75000) {
    clearDtc(Dtc::WARN_HEAP_FRAGMENTATION);
    _minHeapSinceCheck = heap;
  }

  // Learned value discarded at load (NFR-5.3); cleared only manually (DIAG CLEAR)
  static bool learningResetRaised = false;
  if (paramStore.learnedResets() > 0 && !learningResetRaised) {
    learningResetRaised = true;
    raiseDtc(Dtc::WARN_LEARNING_RESET, "Learned duty out of bounds, reset", SEV_WARNING);
  }

  // FR-59: DTC-403 while any default is active
  if (commands.isDefaultCredentials()) {
    raiseDtc(Dtc::WARN_DEFAULT_CREDENTIALS, "Default credentials active", SEV_WARNING);
  } else {
    clearDtc(Dtc::WARN_DEFAULT_CREDENTIALS);
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
  for (size_t i = 0; i < DTC_COUNT; i++) {
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
  for (size_t i = 0; i < DTC_COUNT; i++) {
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
