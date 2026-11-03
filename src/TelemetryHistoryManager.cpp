#include "TelemetryHistoryManager.h"
#include <LittleFS.h>
#include <time.h>
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "TimeAndWeatherService.h"
#include "NetworkConsole.h"

static const char* FILE_24H = "/telemetry_24h.bin";
static const char* FILE_5D  = "/telemetry_5d.bin";
static const char* FILE_BOOT = "/boot_events.bin";

TelemetryHistoryManager telemetryHistory;

TelemetryHistoryManager::TelemetryHistoryManager()
    : _fsMounted(false),
      _bootRecordCount(0),
      _currentBootRecorded(false),
      _bootMs(millis()),
      _bootReason((uint8_t)esp_reset_reason()),
      _currentSlot5d(0),
      _currentSlot24h(0),
      _lastSampleMs(0),
      _lastSampleEpoch(0),
      _lastRecorded5mSlot(0xFFFF),
      _lastRecorded30mSlot(255),
      _dirty24h(false),
      _dirty5d(false) {
    memset(_ring24h, 0, sizeof(_ring24h));
    memset(_ring5d, 0, sizeof(_ring5d));
    memset(_bootRecords, 0, sizeof(_bootRecords));
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

    // Load Boot Records
    if (LittleFS.exists(FILE_BOOT)) {
        File f = LittleFS.open(FILE_BOOT, "r");
        if (f) {
            size_t bytesRead = f.read((uint8_t*)_bootRecords, sizeof(_bootRecords));
            _bootRecordCount = bytesRead / sizeof(BootRecord);
            f.close();
            netConsole.printf("[HISTORY] Loaded %u boot event records\r\n", (unsigned int)_bootRecordCount);
        }
    }
}

void TelemetryHistoryManager::recordBootEvent() {
    if (_currentBootRecorded || !_fsMounted) return;
    _currentBootRecorded = true;
    time_t now = time(nullptr);
    uint32_t bootEpoch = 0;
    if (now > 1700000000UL) {
        bootEpoch = (uint32_t)now - (millis() - _bootMs) / 1000UL;
    } else {
        bootEpoch = (uint32_t)(millis() / 1000UL);
    }

    if (_bootRecordCount >= 10) {
        for (size_t i = 1; i < 10; i++) _bootRecords[i - 1] = _bootRecords[i];
        _bootRecordCount = 9;
    }
    BootRecord& rec = _bootRecords[_bootRecordCount++];
    rec.epoch = bootEpoch;
    rec.slot24h = _currentSlot24h;
    rec.slot5d = _currentSlot5d;
    rec.reason = _bootReason;

    File f = LittleFS.open(FILE_BOOT, "w");
    if (f) {
        f.write((const uint8_t*)_bootRecords, _bootRecordCount * sizeof(BootRecord));
        f.close();
        netConsole.printf("[HISTORY] Recorded boot event (reason: %u, slot24: %u, slot5d: %u)\r\n", rec.reason, rec.slot24h, rec.slot5d);
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
    _dirty24h = true;
    _dirty5d = true;
    saveToFlash();
    netConsole.println(F("[HISTORY] Cleared all history buffers."));
}

void TelemetryHistoryManager::clear5dHistory() {
    memset(_ring5d, 0, sizeof(_ring5d));
    _dirty5d = true;
    saveToFlash();
    netConsole.println(F("[HISTORY] Cleared 5d history buffer."));
}

void TelemetryHistoryManager::sync24hTo5d() {
    for (uint16_t s = 0; s < TELEMETRY_24H_POINTS; s++) {
        if ((_ring24h[s].flags & 0x80) && (s % 6 == 0)) {
            uint8_t slot30m = (uint8_t)((s / 6) % TELEMETRY_5D_POINTS);
            _ring5d[slot30m] = _ring24h[s];
            _dirty5d = true;
        }
    }
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
    updateLiveSample(true);
}

void TelemetryHistoryManager::updateLiveSample(bool forceNewSlot) {
    time_t now = time(nullptr);
    uint16_t slot5m = 0;
    uint8_t slot30m = 0;

    if (now > 1700000000UL) {
        struct tm tmInfo;
        localtime_r(&now, &tmInfo);
        slot5m = (tmInfo.tm_hour * 60 + tmInfo.tm_min) / 5;
        slot30m = (uint8_t)((now / 1800UL) % TELEMETRY_5D_POINTS);
        _lastSampleEpoch = (uint32_t)now;
    } else {
        // Fallback to uptime-based slot
        slot5m = (uint16_t)((millis() / 300000UL) % TELEMETRY_24H_POINTS);
        slot30m = (uint8_t)((millis() / 1800000UL) % TELEMETRY_5D_POINTS);
        _lastSampleEpoch = (uint32_t)(millis() / 1000);
    }

    _currentSlot24h = slot5m;
    _currentSlot5d = slot30m;

    if (!_currentBootRecorded && (now > 1700000000UL || millis() > 15000UL)) {
        recordBootEvent();
    }

    bool isNew5mSlot = (slot5m != _lastRecorded5mSlot);
    if (isNew5mSlot && _lastRecorded5mSlot != 0xFFFF && _fsMounted && _dirty24h) {
        // Finalize previous slot and persist to flash
        saveToFlash();
    }

    // Build current live sample point
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

    // Within same 5-minute slot in RAM: latch active flags if boiler/valves operated at any point during this slot
    if (!isNew5mSlot && !forceNewSlot && (_ring24h[slot5m].flags & 0x80)) {
        f |= (_ring24h[slot5m].flags & 0x1F);
    }
    p.flags = f;
    p.reserved = 0;

    // 1. Update 24h slot in RAM
    _ring24h[slot5m] = p;
    _dirty24h = true;
    _lastRecorded5mSlot = slot5m;

    // 2. Update 5d slot in RAM
    bool isNew30mSlot = (slot30m != _lastRecorded30mSlot);
    uint8_t f5d = f;
    if (!isNew30mSlot && (_ring5d[slot30m].flags & 0x80)) {
        f5d |= (_ring5d[slot30m].flags & 0x1F);
    }
    TelemetryPoint p5d = p;
    p5d.flags = f5d;
    _ring5d[slot30m] = p5d;
    _dirty5d = true;
    if (isNew30mSlot) {
        _lastRecorded30mSlot = slot30m;
    }

    _lastSampleMs = millis();

    // Commit to flash ONLY on slot boundary crossing or explicit forced capture
    if (isNew5mSlot || forceNewSlot) {
        saveToFlash();
    }
}

void TelemetryHistoryManager::update() {
    // Check every 10 seconds to update ongoing slot in RAM and commit on 5-minute boundaries
    if (millis() - _lastSampleMs < 10000UL && _lastSampleMs > 0) {
        return;
    }

    updateLiveSample(false);
}

String TelemetryHistoryManager::getHistoryJson(const String& range) {
    // Ensure RAM ongoing slot reflects live real-time state before JSON export
    updateLiveSample(false);

    bool is5d = (range == "5d");
    size_t count = is5d ? TELEMETRY_5D_POINTS : TELEMETRY_24H_POINTS;
    uint32_t stepSec = is5d ? 1800 : 300;
    uint16_t curSlot = is5d ? _currentSlot5d : _currentSlot24h;

    String json;
    json.reserve(is5d ? 3500 : 4500);

    json = "{\"range\":\"";
    json += is5d ? "5d" : "24h";
    json += "\",\"stepSec\":";
    json += String(stepSec);
    json += ",\"currentSlot\":";
    json += String(curSlot);
    json += ",\"points\":[";

    size_t written = 0;
    for (size_t i = 0; i < count; i++) {
        const TelemetryPoint& pt = is5d ? _ring5d[i] : _ring24h[i];
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

    json += "],\"reboots\":[";
    for (size_t b = 0; b < _bootRecordCount; b++) {
        if (b > 0) json += ",";
        json += "{\"epoch\":";
        json += String(_bootRecords[b].epoch);
        json += ",\"slot24h\":";
        json += String(_bootRecords[b].slot24h);
        json += ",\"slot5d\":";
        json += String(_bootRecords[b].slot5d);
        json += ",\"reason\":\"";
        switch (_bootRecords[b].reason) {
            case ESP_RST_SW:       json += "Firmware Update / Software Reboot"; break;
            case ESP_RST_POWERON:  json += "Pornire alimentare"; break;
            case ESP_RST_PANIC:    json += "Eroare sistem (Crash)"; break;
            case ESP_RST_INT_WDT:
            case ESP_RST_TASK_WDT: json += "Watchdog Timeout"; break;
            default:               json += "Repornire sistem"; break;
        }
        json += "\"}";
    }
    json += "]}";
    return json;
}


uint32_t TelemetryHistoryManager::getZoneRuntimeTodayFromHistory(size_t zoneIndex) const {
    if (zoneIndex >= 3) return 0;
    uint8_t bit = 1 << (zoneIndex + 2);
    uint32_t count = 0;
    for (uint16_t s = 0; s <= _currentSlot24h && s < TELEMETRY_24H_POINTS; s++) {
        if ((_ring24h[s].flags & 0x80) && (_ring24h[s].flags & bit)) {
            count++;
        }
    }
    return count * 300UL;
}

uint32_t TelemetryHistoryManager::getZoneRuntimeYesterdayFromHistory(size_t zoneIndex) const {
    if (zoneIndex >= 3) return 0;
    uint8_t bit = 1 << (zoneIndex + 2);
    uint32_t count = 0;
    for (uint16_t s = _currentSlot24h + 1; s < TELEMETRY_24H_POINTS; s++) {
        if ((_ring24h[s].flags & 0x80) && (_ring24h[s].flags & bit)) {
            count++;
        }
    }
    return count * 300UL;
}
