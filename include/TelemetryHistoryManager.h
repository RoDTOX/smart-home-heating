#ifndef TELEMETRY_HISTORY_MANAGER_H
#define TELEMETRY_HISTORY_MANAGER_H

#include <Arduino.h>

// 288 points for 24 hours (1 sample every 5 minutes: 24 * 12)
#define TELEMETRY_24H_POINTS 288
// 240 points for 5 days (1 sample every 30 minutes: 5 * 48)
#define TELEMETRY_5D_POINTS  240

// Binary telemetry record struct: 10 bytes compact layout
#pragma pack(push, 1)
struct TelemetryPoint {
    int16_t z1_temp;    // Scaled * 10 (e.g. 234 = 23.4°C), -999 = invalid
    int16_t z2_temp;    // Scaled * 10
    int16_t z3_temp;    // Scaled * 10
    int16_t out_temp;   // Scaled * 10 (Outdoor weather temperature)
    uint8_t flags;      // Bit 0: Boiler ON, Bit 1: Pump ON, Bit 2: Z1 Valve, Bit 3: Z2 Valve, Bit 4: Z3 Valve, Bit 7: Valid
    uint8_t reserved;   // Future extension / padding
};

struct BootRecord {
    uint32_t epoch;     // Restore / boot time (T_ON)
    uint32_t offEpoch;  // Downtime start (T_OFF) - when board stopped running
    uint16_t slot24h;
    uint8_t  slot5d;
    uint8_t  reason;
};
#pragma pack(pop)

class TelemetryHistoryManager {
public:
    TelemetryHistoryManager();

    // Initialize LittleFS and load existing history from flash
    void begin();

    // Periodic processing loop called from main loop()
    void update();

    // Force a telemetry capture immediately
    void captureSample();

    // Live ongoing slot tracking (updates in RAM, saves to flash on 5m boundary)
    void updateLiveSample(bool forceNewSlot = false);

    // Stream history as compact JSON for REST API (AR-15)
    void streamHistoryJson(class WebServer& server, const String& range = "24h");  // chunked, FR-34

    // Time synchronization injection from client browser if NTP is offline
    void setLocalTimeFromClient(uint32_t epochSec);

    // Flash persistence
    void saveToFlash();
    void loadFromFlash();
    void clearHistory();
    void clear5dHistory();
    void sync24hTo5d();

    // Diagnostics
    bool isFsMounted() const { return _fsMounted; }
    uint32_t getLastSampleTime() const { return _lastSampleEpoch; }
    uint16_t getCurrentSlot24h() const { return _currentSlot24h; }
    uint8_t  getCurrentSlot5d() const { return _currentSlot5d; }

    // History-derived runtime counters (single source of truth from 24h LittleFS buffer)
    uint32_t getZoneRuntimeTodayFromHistory(size_t zoneIndex) const;
    uint32_t getZoneRuntimeYesterdayFromHistory(size_t zoneIndex) const;

    // FR-35.1: gas meter readings typed by the user, /gas_log.bin, 12 bytes each
    // (epoch, index in 1/1000 m3, boiler ON runtime in s when typed, GAS_NO_RUNTIME if unknown)
    static const uint32_t GAS_NO_RUNTIME = 0xFFFFFFFFUL;
    void gasReadingsBegin();
    uint32_t gasReadingCount() const;
    uint32_t gasReadingsReplaced() const;
    bool gasReadingAdd(uint32_t epoch, uint32_t indexMilli, uint32_t runtimeSec);
    bool gasReadingDelete(uint32_t epoch);
    bool gasReadingUpdate(uint32_t epoch, uint32_t newIndexMilli);
    void gasReadingsForEach(void (*cb)(uint32_t epoch, uint32_t indexMilli, uint32_t runtimeSec, void* ctx), void* ctx) const;
    // Calibrated flow rate from night intervals (zero DHW during sleep) with fallback to all intervals
    bool gasFlowMeasured(float& m3PerHour) const;
    // Estimated DHW (Apa Calda Menajera) from reading deltas minus heating runtime consumption
    bool gasDhwFromReadings(float flameRate, float& dhwToday, float& dhwTotal) const;
    // Gas of the month of `epoch` from readings: last reading of that month minus last reading of the month before
    bool gasMonthFromReadings(uint32_t epoch, float& m3) const;
    // Gas of the day of `epoch` from readings
    bool gasDayFromReadings(uint32_t dayEpoch, uint32_t currentTotalSec, float flameRate, float& m3) const;
    // Boiler ON seconds between two times from the history (FR-35.1, FR-34): 24 h ring while t1 is under 24 h old,
    // else the 5 day ring while under 5 days; false when a step of the interval was not recorded or t1 is older.
    bool boilerOnSeconds(uint32_t t1, uint32_t t2, uint32_t& onSec, bool& fiveDay) const;

    // Boot & restart tracking
    void recordBootEvent();
    void noteShutdown(const char* reason = nullptr);
    uint8_t getBootRecordCount() const { return _bootRecordCount; }
    const BootRecord* getBootRecords() const { return _bootRecords; }

private:
    bool _fsMounted;
    TelemetryPoint _livePoint24h;
    TelemetryPoint _livePoint5d;
    BootRecord _bootRecords[10];
    uint8_t  _bootRecordCount;
    bool     _currentBootRecorded;
    uint32_t _bootMs;
    uint8_t  _bootReason;
    uint32_t _savedRtcOffEpoch;
    uint32_t _savedFlashOffEpoch;
    bool     _savedRtcValid;
    uint8_t  _currentSlot5d;
    uint16_t _currentSlot24h;
    uint32_t _lastSampleMs;
    uint32_t _lastSampleEpoch;
    uint16_t _lastRecorded5mSlot;
    uint8_t  _lastRecorded30mSlot;
    bool     _dirty24h;
    bool     _dirty5d;

    void initFlashFiles();
    bool readPoint24h(uint16_t slot, TelemetryPoint& pt) const;
    bool writePoint24h(uint16_t slot, const TelemetryPoint& pt);
    bool readPoint5d(uint8_t slot, TelemetryPoint& pt) const;
    bool writePoint5d(uint8_t slot, const TelemetryPoint& pt);
    void updateSlots(uint32_t epochNow);
};

extern TelemetryHistoryManager telemetryHistory;

#endif // TELEMETRY_HISTORY_MANAGER_H
