#include "TelemetryHistoryManager.h"
#include <LittleFS.h>
#include <time.h>
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "TimeAndWeatherService.h"
#include "NetworkConsole.h"
#include "ParamStore.h"

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
        gasReadingsBegin();
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

    // Bit 5-6: Zone in window pause (0: none, 1: Z1, 2: Z2, 3: Z3)
    uint8_t winCode = 0;
    for (size_t i = 0; i < thermalZones.getZoneCount() && i < 3; i++) {
        if (thermalZones.isWindowPauseActive(i)) {
            winCode = (uint8_t)(i + 1);
            break;
        }
    }
    f |= ((winCode & 0x03) << 5);

    // Within same 5-minute slot in RAM: latch active flags if boiler/valves/window operated at any point during this slot
    if (!isNew5mSlot && !forceNewSlot && (_ring24h[slot5m].flags & 0x80)) {
        f |= (_ring24h[slot5m].flags & 0x7F);
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
        f5d |= (_ring5d[slot30m].flags & 0x7F);
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
    json += "],\"windows\":[";
    size_t wWritten = 0;
    for (size_t i = 0; i < thermalZones.getZoneCount() && i < 3; i++) {
        if (thermalZones.isWindowPauseActive(i)) {
            if (wWritten > 0) json += ",";
            json += "{\"id\":" + String(i + 1) + ",";
            json += "\"name\":\"" + String(thermalZones.getConfig(i).name) + "\",";
            json += "\"remainingSec\":" + String(thermalZones.getWindowPauseRemainingSec(i)) + ",";
            json += "\"lastEpoch\":" + String(thermalZones.getWindowLastDetectionEpoch(i)) + "}";
            wWritten++;
        }
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

// ---------------------------------------------------------------------------------------------------------
// FR-35.1: gas meter readings typed by the user (the owner's data: never dropped, the oldest is replaced when full)
// ---------------------------------------------------------------------------------------------------------
static const char* FILE_GAS_LOG = "/gas_log.bin";
static const char* FILE_GAS_LOG_TMP = "/gas_log.tmp";
static const char* FILE_GAS_READINGS_V1 = "/gas_readings.bin";  // 8-byte records of the first version
static const char* FILE_GAS_CALIB_OLD = "/gas_calib.json";
static const uint32_t GAS_READINGS_CAP = 1000;  // full: the oldest entry is replaced
static const size_t GAS_REC = 12;

static uint32_t gasMonthKey(uint32_t epoch) {
    time_t t = (time_t)epoch;
    struct tm tmv;
    localtime_r(&t, &tmv);
    return (uint32_t)(tmv.tm_year * 12 + tmv.tm_mon);
}

void TelemetryHistoryManager::gasReadingsBegin() {
    if (!_fsMounted) return;
    if (paramStore.state(StateKey::GasLogMigrated) == 0) {
        // First start with the reading log: the old calibration ring (test entries included) is dropped and the base
        // of the old firmware is not a reading; the log starts with the readings the owner confirms.
        LittleFS.remove(FILE_GAS_CALIB_OLD);
        paramStore.saveState(StateKey::GasMeterBaseIndex, 0);
        paramStore.saveState(StateKey::GasMeterBaseSec, 0);
        paramStore.saveState(StateKey::GasMeterBaseEpoch, 0);
        paramStore.saveState(StateKey::GasLogMigrated, 1);
        netConsole.println(F("[GAS] Old calibration log dropped; reading log started (FR-35.1)."));
    }
    // The first version of the log had 8-byte records without a runtime: convert once, keep every reading.
    if (LittleFS.exists(FILE_GAS_READINGS_V1) && !LittleFS.exists(FILE_GAS_LOG)) {
        File in = LittleFS.open(FILE_GAS_READINGS_V1, "r");
        File out = LittleFS.open(FILE_GAS_LOG_TMP, "w");
        bool ok = in && out;
        uint32_t rec8[2];
        uint32_t converted = 0;
        uint32_t expected = in ? (uint32_t)(in.size() / 8) : 0;
        while (ok && in.read((uint8_t*)rec8, 8) == 8) {
            converted++;
            uint32_t rec[3] = {rec8[0], rec8[1], GAS_NO_RUNTIME};
            if (out.write((const uint8_t*)rec, GAS_REC) != GAS_REC) ok = false;
        }
        if (in) in.close();
        if (out) out.close();
        if (ok && converted == expected && LittleFS.rename(FILE_GAS_LOG_TMP, FILE_GAS_LOG)) {
            LittleFS.remove(FILE_GAS_READINGS_V1);
            netConsole.println(F("[GAS] Reading log converted to the version with runtime."));
        } else {
            LittleFS.remove(FILE_GAS_LOG_TMP);
        }
    }
}

uint32_t TelemetryHistoryManager::gasReadingsReplaced() const {
    return paramStore.state(StateKey::GasCompacted);
}

uint32_t TelemetryHistoryManager::gasReadingCount() const {
    if (!_fsMounted) return 0;
    File f = LittleFS.open(FILE_GAS_LOG, "r");
    if (!f) return 0;
    uint32_t n = (uint32_t)(f.size() / GAS_REC);
    f.close();
    return n;
}

void TelemetryHistoryManager::gasReadingsForEach(void (*cb)(uint32_t, uint32_t, uint32_t, void*), void* ctx) const {
    if (!_fsMounted) return;
    File f = LittleFS.open(FILE_GAS_LOG, "r");
    if (!f) return;
    uint32_t rec[3];
    while (f.read((uint8_t*)rec, GAS_REC) == GAS_REC) cb(rec[0], rec[1], rec[2], ctx);
    f.close();
}

// One pass old file -> temp file: the new reading goes in at its place by time; with dropOldest the first (oldest)
// entry is left out. The temp file replaces the log only when complete.
static bool gasRewrite(const uint32_t* nr, bool dropOldest, uint32_t count) {
    File in = LittleFS.open(FILE_GAS_LOG, "r");
    if (!in && count > 0) return false;  // never replace a log that could not be read
    File out = LittleFS.open(FILE_GAS_LOG_TMP, "w");
    if (!out) {
        if (in) in.close();
        return false;
    }
    uint32_t readCount = 0;
    bool newDone = false;
    bool first = true;
    bool ok = true;
    uint32_t rec[3];
    while (in && in.read((uint8_t*)rec, GAS_REC) == GAS_REC) {
        readCount++;
        if (first && dropOldest) {
            first = false;
            continue;
        }
        first = false;
        if (!newDone && nr[0] < rec[0]) {
            if (out.write((const uint8_t*)nr, GAS_REC) != GAS_REC) ok = false;
            newDone = true;
        }
        if (out.write((const uint8_t*)rec, GAS_REC) != GAS_REC) ok = false;
    }
    if (!newDone && out.write((const uint8_t*)nr, GAS_REC) != GAS_REC) ok = false;
    if (in) in.close();
    out.close();
    if (readCount != count) ok = false;  // a short read: keep the old log
    if (!ok) {
        LittleFS.remove(FILE_GAS_LOG_TMP);
        return false;
    }
    return LittleFS.rename(FILE_GAS_LOG_TMP, FILE_GAS_LOG);
}

bool TelemetryHistoryManager::gasReadingAdd(uint32_t epoch, uint32_t indexMilli, uint32_t runtimeSec) {
    if (!_fsMounted) return false;
    if (LittleFS.exists(FILE_GAS_READINGS_V1) && !LittleFS.exists(FILE_GAS_LOG)) return false;  // conversion pending: never mix
    uint32_t count = gasReadingCount();
    uint32_t lastEpoch = 0;
    if (count > 0) {
        File f = LittleFS.open(FILE_GAS_LOG, "r");
        if (!f) return false;
        f.seek((count - 1) * GAS_REC);
        uint32_t rec[3] = {0, 0, 0};
        f.read((uint8_t*)rec, GAS_REC);
        f.close();
        lastEpoch = rec[0];
    }
    uint32_t nr[3] = {epoch, indexMilli, runtimeSec};
    bool full = count >= GAS_READINGS_CAP;
    if (epoch >= lastEpoch && !full) {
        File f = LittleFS.open(FILE_GAS_LOG, "a");
        if (!f) return false;
        bool ok = f.write((uint8_t*)nr, GAS_REC) == GAS_REC;
        f.close();
        return ok;
    }
    if (!gasRewrite(nr, full, count)) return false;
    if (full) {
        paramStore.saveState(StateKey::GasCompacted, paramStore.state(StateKey::GasCompacted) + 1);
        netConsole.println(F("[GAS] Reading log full (1000): the oldest reading was replaced."));
    }
    return true;
}

struct GasMonthCtx {
    uint32_t thisKey, prevKey;
    bool haveThis, havePrev;
    uint32_t thisIdx, prevIdx;
};

bool TelemetryHistoryManager::gasMonthFromReadings(uint32_t epoch, float& m3) const {
    GasMonthCtx c = {gasMonthKey(epoch), 0, false, false, 0, 0};
    c.prevKey = c.thisKey - 1;  // months are consecutive keys
    gasReadingsForEach([](uint32_t e, uint32_t idx, uint32_t, void* p) {
        GasMonthCtx* x = (GasMonthCtx*)p;
        uint32_t k = gasMonthKey(e);
        if (k == x->thisKey) { x->haveThis = true; x->thisIdx = idx; }
        else if (k == x->prevKey) { x->havePrev = true; x->prevIdx = idx; }
    }, &c);
    if (!c.haveThis || !c.havePrev || c.thisIdx < c.prevIdx) return false;
    m3 = (float)(c.thisIdx - c.prevIdx) / 1000.0f;
    return true;
}

struct GasFlowCtx {
    bool havePrev;
    uint32_t prevIdx, prevRt;
    uint32_t dIdx[10];  // last 10 qualifying intervals: rise in 1/1000 m3 and boiler ON seconds
    uint32_t dSec[10];
    uint8_t n;
};

bool TelemetryHistoryManager::gasFlowMeasured(float& m3PerHour) const {
    GasFlowCtx c;
    memset(&c, 0, sizeof(c));
    gasReadingsForEach([](uint32_t, uint32_t idx, uint32_t rt, void* p) {
        GasFlowCtx* x = (GasFlowCtx*)p;
        if (rt != GAS_NO_RUNTIME && x->havePrev && rt >= x->prevRt && idx > x->prevIdx && rt - x->prevRt >= 3600UL) {
            if (x->n == 10) {
                memmove(x->dIdx, x->dIdx + 1, 9 * sizeof(uint32_t));
                memmove(x->dSec, x->dSec + 1, 9 * sizeof(uint32_t));
                x->n = 9;
            }
            x->dIdx[x->n] = idx - x->prevIdx;
            x->dSec[x->n] = rt - x->prevRt;
            x->n++;
        }
        if (rt != GAS_NO_RUNTIME) {
            x->havePrev = true;
            x->prevIdx = idx;
            x->prevRt = rt;
        } else {
            x->havePrev = false;  // an interval needs two readings that both have a runtime
        }
    }, &c);
    if (c.n == 0) return false;
    double sumM3 = 0, sumH = 0;
    for (uint8_t i = 0; i < c.n; i++) {
        sumM3 += c.dIdx[i] / 1000.0;
        sumH += c.dSec[i] / 3600.0;
    }
    if (sumH <= 0) return false;
    m3PerHour = (float)(sumM3 / sumH);
    return true;
}
