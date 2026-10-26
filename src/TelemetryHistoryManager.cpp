#include "TelemetryHistoryManager.h"
#include <LittleFS.h>
#include <time.h>
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "TimeAndWeatherService.h"
#include "NetworkConsole.h"

static const char* FILE_24H = "/telemetry_24h.bin";
static const char* FILE_5D  = "/telemetry_5d.bin";

TelemetryHistoryManager telemetryHistory;

TelemetryHistoryManager::TelemetryHistoryManager()
    : _fsMounted(false),
      _head5d(0),
      _currentSlot24h(0),
      _lastSampleMs(0),
      _lastSampleEpoch(0),
      _lastRecorded5mSlot(0xFFFF),
      _lastRecorded30mSlot(255),
      _dirty24h(false),
      _dirty5d(false) {
    memset(_ring24h, 0, sizeof(_ring24h));
    memset(_ring5d, 0, sizeof(_ring5d));
}

void TelemetryHistoryManager::begin() {
    // Mount LittleFS with format-on-fail
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

    // Load 24h Ring Buffer
    if (LittleFS.exists(FILE_24H)) {
        File f = LittleFS.open(FILE_24H, "r");
        if (f) {
            size_t bytesRead = f.read((uint8_t*)_ring24h, sizeof(_ring24h));
            f.close();
            netConsole.printf("[HISTORY] Loaded 24h history (%u bytes)\r\n", (unsigned int)bytesRead);
        }
    } else {
        netConsole.println(F("[HISTORY] No existing 24h history file found. Initialized blank."));
    }

    // Load 5d Ring Buffer
    if (LittleFS.exists(FILE_5D)) {
        File f = LittleFS.open(FILE_5D, "r");
        if (f) {
            size_t bytesRead = f.read((uint8_t*)_ring5d, sizeof(_ring5d));
            f.close();
            netConsole.printf("[HISTORY] Loaded 5d history (%u bytes)\r\n", (unsigned int)bytesRead);
        }
    } else {
        netConsole.println(F("[HISTORY] No existing 5d history file found. Initialized blank."));
    }
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

    if (_dirty5d) {
        File f = LittleFS.open(FILE_5D, "w");
        if (f) {
            f.write((const uint8_t*)_ring5d, sizeof(_ring5d));
            f.close();
            _dirty5d = false;
        }
    }
}

void TelemetryHistoryManager::clearHistory() {
    memset(_ring24h, 0, sizeof(_ring24h));
    memset(_ring5d, 0, sizeof(_ring5d));
    _head5d = 0;
    _dirty24h = true;
    _dirty5d = true;
    saveToFlash();
    netConsole.println(F("[HISTORY] Cleared all history buffers."));
}

void TelemetryHistoryManager::setLocalTimeFromClient(uint32_t epochSec) {
    if (epochSec > 1700000000UL) {
        struct timeval tv = { (time_t)epochSec, 0 };
        settimeofday(&tv, nullptr);
        netConsole.printf("[HISTORY] Local time injected from client: %u\r\n", (unsigned int)epochSec);
        captureSample();
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
        // Fallback to uptime-based slot
        slot5m = (uint16_t)((millis() / 300000UL) % TELEMETRY_24H_POINTS);
        slot30m = (uint8_t)((millis() / 1800000UL) % 48);
        _lastSampleEpoch = (uint32_t)(millis() / 1000);
    }

    _currentSlot24h = slot5m;

    // Build sample point
    TelemetryPoint p;
    memset(&p, 0, sizeof(p));

    // Sample Zone Temperatures
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

    // Flags: Bit 0: Boiler ON, Bit 1: Pump ON, Bit 2: Z1 Valve, Bit 3: Z2 Valve, Bit 4: Z3 Valve, Bit 7: Valid
    uint8_t f = 0x80; // Valid bit
    if (boilerRadio.isLastCommandOn()) f |= 0x01;
    if (thermalZones.isPumpActive())   f |= 0x02;
    if (thermalZones.getZoneCount() > 0 && thermalZones.getStatus(0).relayEngaged) f |= 0x04;
    if (thermalZones.getZoneCount() > 1 && thermalZones.getStatus(1).relayEngaged) f |= 0x08;
    if (thermalZones.getZoneCount() > 2 && thermalZones.getStatus(2).relayEngaged) f |= 0x10;
    p.flags = f;

    // 1. Store into 24h slot
    _ring24h[slot5m] = p;
    _dirty24h = true;
    _lastRecorded5mSlot = slot5m;

    // 2. Store into 5d buffer on 30m boundary
    if (slot30m != _lastRecorded30mSlot) {
        _ring5d[_head5d] = p;
        _head5d = (_head5d + 1) % TELEMETRY_5D_POINTS;
        _dirty5d = true;
        _lastRecorded30mSlot = slot30m;
    }

    _lastSampleMs = millis();
    saveToFlash();
}

void TelemetryHistoryManager::update() {
    // Check every 10 seconds if a new 5-minute boundary was crossed
    if (millis() - _lastSampleMs < 10000UL && _lastSampleMs > 0) {
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

String TelemetryHistoryManager::getHistoryJson(const String& range) {
    bool is5d = (range == "5d");
    size_t count = is5d ? TELEMETRY_5D_POINTS : TELEMETRY_24H_POINTS;
    uint32_t stepSec = is5d ? 1800 : 300;

    String json;
    json.reserve(is5d ? 3500 : 4500);

    json = "{\"range\":\"";
    json += is5d ? "5d" : "24h";
    json += "\",\"stepSec\":";
    json += String(stepSec);
    json += ",\"currentSlot\":";
    json += String(_currentSlot24h);
    json += ",\"points\":[";

    size_t written = 0;

    if (is5d) {
        // Output rolling 5d points in chronological order starting from _head5d
        for (size_t i = 0; i < count; i++) {
            size_t idx = (_head5d + i) % TELEMETRY_5D_POINTS;
            const TelemetryPoint& pt = _ring5d[idx];
            if (!(pt.flags & 0x80)) continue; // Skip invalid

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
        // Output 24h slots 0 .. 287
        for (size_t i = 0; i < count; i++) {
            const TelemetryPoint& pt = _ring24h[i];
            if (!(pt.flags & 0x80)) continue; // Skip invalid

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
