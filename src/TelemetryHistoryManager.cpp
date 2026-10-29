#include "TelemetryHistoryManager.h"
#include <LittleFS.h>
#include <time.h>
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "TimeAndWeatherService.h"
#include "NetworkConsole.h"

static const char* FILE_24H    = "/telemetry_24h.bin";
static const char* FILE_7D     = "/telemetry_7d.bin";
static const char* FILE_EVENTS = "/events.bin";

TelemetryHistoryManager telemetryHistory;

TelemetryHistoryManager::TelemetryHistoryManager()
    : _fsMounted(false),
      _head7d(0),
      _currentSlot24h(0),
      _lastSampleMs(0),
      _lastSampleEpoch(0),
      _lastRecorded5mSlot(0xFFFF),
      _lastRecorded30mSlot(255),
      _dirty24h(false),
      _dirty7d(false),
      _lastBoilerState(false),
      _lastZ1State(false),
      _lastZ2State(false),
      _lastZ3State(false),
      _lastPumpState(false),
      _boilerStartEpoch(0),
      _z1StartEpoch(0),
      _z2StartEpoch(0),
      _z3StartEpoch(0),
      _pumpStartEpoch(0),
      _eventHead(0),
      _eventCount(0),
      _dirtyEvents(false) {
    memset(_ring24h, 0, sizeof(_ring24h));
    memset(_ring7d, 0, sizeof(_ring7d));
    memset(_events, 0, sizeof(_events));
}

void TelemetryHistoryManager::begin() {
    if (!LittleFS.begin(true)) {
        netConsole.println(F("[HISTORY][WARN] LittleFS mount failed!"));
        _fsMounted = false;
    } else {
        _fsMounted = true;
        netConsole.println(F("[HISTORY] LittleFS mounted successfully."));
        loadFromFlash();
    }
}

void TelemetryHistoryManager::loadFromFlash() {
    if (!_fsMounted) return;

    if (LittleFS.exists(FILE_24H)) {
        File f = LittleFS.open(FILE_24H, "r");
        if (f) {
            size_t bytesRead = f.read((uint8_t*)_ring24h, sizeof(_ring24h));
            f.close();
            netConsole.printf("[HISTORY] Loaded 24h history (%u bytes)\r\n", (unsigned int)bytesRead);
        }
    }

    if (LittleFS.exists(FILE_7D)) {
        File f = LittleFS.open(FILE_7D, "r");
        if (f) {
            size_t bytesRead = f.read((uint8_t*)_ring7d, sizeof(_ring7d));
            f.close();
            netConsole.printf("[HISTORY] Loaded 7d history (%u bytes)\r\n", (unsigned int)bytesRead);
        }
    }

    loadEventsFromFlash();
}

void TelemetryHistoryManager::loadEventsFromFlash() {
    if (!_fsMounted || !LittleFS.exists(FILE_EVENTS)) return;

    File f = LittleFS.open(FILE_EVENTS, "r");
    if (!f) return;

    size_t sz = f.size();
    if (sz > sizeof(_events)) sz = sizeof(_events);
    f.read((uint8_t*)_events, sz);
    f.close();

    _eventCount = sz / sizeof(StateEvent);
    _eventHead = _eventCount % MAX_STATE_EVENTS;
    netConsole.printf("[HISTORY] Loaded %u state transition events from LittleFS\r\n", _eventCount);
}

void TelemetryHistoryManager::saveToFlash() {
    if (!_fsMounted) return;

    if (_dirty24h) {
        File f = LittleFS.open(FILE_24H, "w");
        if (f) {
            f.write((const uint8_t*)_ring24h, sizeof(_ring24h));
            f.close();
            _dirty24h = false;
        }
    }

    if (_dirty7d) {
        File f = LittleFS.open(FILE_7D, "w");
        if (f) {
            f.write((const uint8_t*)_ring7d, sizeof(_ring7d));
            f.close();
            _dirty7d = false;
        }
    }

    saveEventsToFlash();
}

void TelemetryHistoryManager::saveEventsToFlash() {
    if (!_fsMounted || !_dirtyEvents) return;

    File f = LittleFS.open(FILE_EVENTS, "w");
    if (f) {
        size_t bytesToWrite = _eventCount * sizeof(StateEvent);
        f.write((const uint8_t*)_events, bytesToWrite);
        f.close();
        _dirtyEvents = false;
    }
}

void TelemetryHistoryManager::clearHistory() {
    memset(_ring24h, 0, sizeof(_ring24h));
    memset(_ring7d, 0, sizeof(_ring7d));
    memset(_events, 0, sizeof(_events));
    _head7d = 0;
    _eventHead = 0;
    _eventCount = 0;
    _dirty24h = true;
    _dirty7d = true;
    _dirtyEvents = true;
    saveToFlash();
    netConsole.println(F("[HISTORY] Cleared all history buffers & event logs."));
}

void TelemetryHistoryManager::setLocalTimeFromClient(uint32_t epochSec) {
    if (epochSec > 1700000000UL) {
        struct timeval tv = { (time_t)epochSec, 0 };
        settimeofday(&tv, nullptr);
        netConsole.printf("[HISTORY] Local time injected from client: %u\r\n", (unsigned int)epochSec);
        captureSample();
    }
}

void TelemetryHistoryManager::recordEvent(uint8_t device, uint8_t state, uint16_t durationSec) {
    time_t now = time(nullptr);
    uint32_t epoch = (now > 1700000000UL) ? (uint32_t)now : (uint32_t)(millis() / 1000);

    StateEvent ev;
    ev.epoch = epoch;
    ev.device = device;
    ev.state = state;
    ev.durationSec = durationSec;

    _events[_eventHead] = ev;
    _eventHead = (_eventHead + 1) % MAX_STATE_EVENTS;
    if (_eventCount < MAX_STATE_EVENTS) {
        _eventCount++;
    }
    _dirtyEvents = true;

    static const char* devNames[] = { "Boiler", "Living (Z1)", "Copil (Z2)", "Matrimonial (Z3)", "Pump" };
    const char* dName = (device <= 4) ? devNames[device] : "Unknown";
    if (state == 1) {
        netConsole.printf("[EVENT] %s -> STARTED [ON]\r\n", dName);
    } else {
        netConsole.printf("[EVENT] %s -> STOPPED [OFF] (Ran for %u s)\r\n", dName, durationSec);
    }
}

void TelemetryHistoryManager::checkStateTransitions() {
    time_t now = time(nullptr);
    uint32_t epoch = (now > 1700000000UL) ? (uint32_t)now : (uint32_t)(millis() / 1000);

    // 1. Boiler state check
    bool boilerNow = boilerRadio.isLastCommandOn();
    if (boilerNow != _lastBoilerState) {
        if (boilerNow) {
            _boilerStartEpoch = epoch;
            recordEvent(0, 1, 0);
        } else {
            uint32_t dur = (epoch >= _boilerStartEpoch && _boilerStartEpoch > 0) ? (epoch - _boilerStartEpoch) : 0;
            recordEvent(0, 0, (dur > 65535) ? 65535 : (uint16_t)dur);
        }
        _lastBoilerState = boilerNow;
    }

    // 2. Zone Relays check
    bool z1Now = (thermalZones.getZoneCount() > 0 && thermalZones.getStatus(0).relayEngaged);
    if (z1Now != _lastZ1State) {
        if (z1Now) {
            _z1StartEpoch = epoch;
            recordEvent(1, 1, 0);
        } else {
            uint32_t dur = (epoch >= _z1StartEpoch && _z1StartEpoch > 0) ? (epoch - _z1StartEpoch) : 0;
            recordEvent(1, 0, (dur > 65535) ? 65535 : (uint16_t)dur);
        }
        _lastZ1State = z1Now;
    }

    bool z2Now = (thermalZones.getZoneCount() > 1 && thermalZones.getStatus(1).relayEngaged);
    if (z2Now != _lastZ2State) {
        if (z2Now) {
            _z2StartEpoch = epoch;
            recordEvent(2, 1, 0);
        } else {
            uint32_t dur = (epoch >= _z2StartEpoch && _z2StartEpoch > 0) ? (epoch - _z2StartEpoch) : 0;
            recordEvent(2, 0, (dur > 65535) ? 65535 : (uint16_t)dur);
        }
        _lastZ2State = z2Now;
    }

    bool z3Now = (thermalZones.getZoneCount() > 2 && thermalZones.getStatus(2).relayEngaged);
    if (z3Now != _lastZ3State) {
        if (z3Now) {
            _z3StartEpoch = epoch;
            recordEvent(3, 1, 0);
        } else {
            uint32_t dur = (epoch >= _z3StartEpoch && _z3StartEpoch > 0) ? (epoch - _z3StartEpoch) : 0;
            recordEvent(3, 0, (dur > 65535) ? 65535 : (uint16_t)dur);
        }
        _lastZ3State = z3Now;
    }

    // 3. Pump check
    bool pumpNow = thermalZones.isPumpActive();
    if (pumpNow != _lastPumpState) {
        if (pumpNow) {
            _pumpStartEpoch = epoch;
            recordEvent(4, 1, 0);
        } else {
            uint32_t dur = (epoch >= _pumpStartEpoch && _pumpStartEpoch > 0) ? (epoch - _pumpStartEpoch) : 0;
            recordEvent(4, 0, (dur > 65535) ? 65535 : (uint16_t)dur);
        }
        _lastPumpState = pumpNow;
    }
}

void TelemetryHistoryManager::captureSample() {
    time_t now = time(nullptr);
    uint16_t slot5m = 0;
    uint8_t slot30m = 0;

    if (now > 1700000000UL) {
        struct tm tmInfo;
        localtime_r(&now, &tmInfo);
        slot5m = (tmInfo.tm_hour * 60 + tmInfo.tm_min) / 5;
        slot30m = (tmInfo.tm_hour * 60 + tmInfo.tm_min) / 30;
        _lastSampleEpoch = (uint32_t)now;
    } else {
        slot5m = (uint16_t)((millis() / 300000UL) % TELEMETRY_24H_POINTS);
        slot30m = (uint8_t)((millis() / 1800000UL) % 48);
        _lastSampleEpoch = (uint32_t)(millis() / 1000);
    }

    _currentSlot24h = slot5m;

    TelemetryPoint p;
    memset(&p, 0, sizeof(p));

    int16_t zt[3] = { -999, -999, -999 };
    for (size_t i = 0; i < 3 && i < thermalZones.getZoneCount(); i++) {
        const auto& st = thermalZones.getStatus(i);
        if (st.sensorValid) {
            zt[i] = (int16_t)(roundf(st.currentTemp * 10.0f));
        }
    }
    p.z1_temp  = zt[0];
    p.z2_temp  = zt[1];
    p.z3_temp  = zt[2];

    if (timeAndWeather.isWeatherValid()) {
        p.out_temp = (int16_t)(roundf(timeAndWeather.getOutdoorTemperature() * 10.0f));
    } else {
        p.out_temp = -999;
    }

    uint8_t f = 0x80;
    if (boilerRadio.isLastCommandOn()) f |= 0x01;
    if (thermalZones.isPumpActive())   f |= 0x02;
    if (thermalZones.getZoneCount() > 0 && thermalZones.getStatus(0).relayEngaged) f |= 0x04;
    if (thermalZones.getZoneCount() > 1 && thermalZones.getStatus(1).relayEngaged) f |= 0x08;
    if (thermalZones.getZoneCount() > 2 && thermalZones.getStatus(2).relayEngaged) f |= 0x10;
    p.flags = f;

    _ring24h[slot5m] = p;
    _dirty24h = true;
    _lastRecorded5mSlot = slot5m;

    if (slot30m != _lastRecorded30mSlot) {
        _ring7d[_head7d] = p;
        _head7d = (_head7d + 1) % TELEMETRY_7D_POINTS;
        _dirty7d = true;
        _lastRecorded30mSlot = slot30m;
    }

    _lastSampleMs = millis();
    saveToFlash();
}

void TelemetryHistoryManager::update() {
    checkStateTransitions();

    if (millis() - _lastSampleMs < 5000UL && _lastSampleMs > 0) {
        return;
    }

    time_t now = time(nullptr);
    uint16_t currentSlot5m = 0;

    if (now > 1700000000UL) {
        struct tm tmInfo;
        localtime_r(&now, &tmInfo);
        currentSlot5m = (tmInfo.tm_hour * 60 + tmInfo.tm_min) / 5;
    } else {
        currentSlot5m = (uint16_t)((millis() / 300000UL) % TELEMETRY_24H_POINTS);
    }

    if (currentSlot5m != _lastRecorded5mSlot || _lastSampleMs == 0) {
        captureSample();
    }
}

String TelemetryHistoryManager::getEventsJson(uint16_t limit) {
    if (limit == 0 || limit > _eventCount) limit = _eventCount;

    static const char* devKeys[] = { "boiler", "living", "copil", "matrimonial", "pump" };

    // Calculate today's midnight epoch to aggregate runtime seconds
    time_t now = time(nullptr);
    uint32_t todayStart = 0;
    if (now > 1700000000UL) {
        struct tm ti;
        localtime_r(&now, &ti);
        ti.tm_hour = 0; ti.tm_min = 0; ti.tm_sec = 0;
        todayStart = (uint32_t)mktime(&ti);
    }

    uint32_t todaySec[5] = { 0, 0, 0, 0, 0 };

    String json = "{\"events\":[";
    size_t written = 0;

    // Iterate backwards (newest first) or chronological
    size_t startIdx = (_eventHead + MAX_STATE_EVENTS - limit) % MAX_STATE_EVENTS;
    for (size_t i = 0; i < limit; i++) {
        size_t idx = (startIdx + i) % MAX_STATE_EVENTS;
        const StateEvent& ev = _events[idx];
        if (ev.epoch == 0) continue;

        if (written > 0) json += ",";
        json += "{\"t\":";
        json += String(ev.epoch);
        json += ",\"d\":\"";
        json += (ev.device <= 4) ? devKeys[ev.device] : "other";
        json += "\",\"s\":";
        json += String(ev.state);
        json += ",\"dur\":";
        json += String(ev.durationSec);
        json += "}";
        written++;

        if (ev.state == 0 && ev.epoch >= todayStart && ev.device <= 4) {
            todaySec[ev.device] += ev.durationSec;
        }
    }

    json += "],\"todayRuntime\":{";
    json += "\"boilerSec\":" + String(todaySec[0]) + ",";
    json += "\"livingSec\":" + String(todaySec[1]) + ",";
    json += "\"copilSec\":" + String(todaySec[2]) + ",";
    json += "\"matrimonialSec\":" + String(todaySec[3]) + ",";
    json += "\"pumpSec\":" + String(todaySec[4]);
    json += "}}";
    return json;
}

String TelemetryHistoryManager::getHistoryJson(const String& range) {
    bool isLongTerm = (range == "7d" || range == "5d");
    size_t count = isLongTerm ? TELEMETRY_7D_POINTS : TELEMETRY_24H_POINTS;
    uint32_t stepSec = isLongTerm ? 1800 : 300;

    String json;
    json.reserve(isLongTerm ? 4500 : 4000);

    json = "{\"range\":\"";
    json += isLongTerm ? range : "24h";
    json += "\",\"stepSec\":";
    json += String(stepSec);
    json += ",\"currentSlot\":";
    json += String(_currentSlot24h);
    json += ",\"points\":[";

    size_t written = 0;

    if (isLongTerm) {
        for (size_t i = 0; i < count; i++) {
            size_t idx = (_head7d + i) % TELEMETRY_7D_POINTS;
            const TelemetryPoint& pt = _ring7d[idx];
            if (!(pt.flags & 0x80)) continue;

            if (written > 0) json += ",";
            json += "[";
            json += String(i) + ",";
            json += (pt.z1_temp == -999 ? "null" : String((float)pt.z1_temp / 10.0f, 1)) + ",";
            json += (pt.z2_temp == -999 ? "null" : String((float)pt.z2_temp / 10.0f, 1)) + ",";
            json += (pt.z3_temp == -999 ? "null" : String((float)pt.z3_temp / 10.0f, 1)) + ",";
            json += (pt.out_temp == -999 ? "null" : String((float)pt.out_temp / 10.0f, 1)) + ",";
            json += String(pt.flags);
            json += "]";
            written++;
        }
    } else {
        for (size_t i = 0; i < count; i++) {
            const TelemetryPoint& pt = _ring24h[i];
            if (!(pt.flags & 0x80)) continue;

            if (written > 0) json += ",";
            json += "[";
            json += String(i) + ",";
            json += (pt.z1_temp == -999 ? "null" : String((float)pt.z1_temp / 10.0f, 1)) + ",";
            json += (pt.z2_temp == -999 ? "null" : String((float)pt.z2_temp / 10.0f, 1)) + ",";
            json += (pt.z3_temp == -999 ? "null" : String((float)pt.z3_temp / 10.0f, 1)) + ",";
            json += (pt.out_temp == -999 ? "null" : String((float)pt.out_temp / 10.0f, 1)) + ",";
            json += String(pt.flags);
            json += "]";
            written++;
        }
    }

    json += "]}";
    return json;
}
