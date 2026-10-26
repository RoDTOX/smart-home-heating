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

    // Export history as compact JSON for REST API
    String getHistoryJson(const String& range = "24h");

    // Time synchronization injection from client browser if NTP is offline
    void setLocalTimeFromClient(uint32_t epochSec);

    // Flash persistence
    void saveToFlash();
    void loadFromFlash();
    void clearHistory();

    // Diagnostics
    bool isFsMounted() const { return _fsMounted; }
    uint32_t getLastSampleTime() const { return _lastSampleEpoch; }
    uint16_t getCurrentSlot24h() const { return _currentSlot24h; }

private:
    bool _fsMounted;
    TelemetryPoint _ring24h[TELEMETRY_24H_POINTS];
    TelemetryPoint _ring5d[TELEMETRY_5D_POINTS];
    uint16_t _head5d;
    uint16_t _currentSlot24h;
    uint32_t _lastSampleMs;
    uint32_t _lastSampleEpoch;
    uint16_t _lastRecorded5mSlot;
    uint8_t  _lastRecorded30mSlot;
    bool     _dirty24h;
    bool     _dirty5d;

    void updateSlots(uint32_t epochNow);
};

extern TelemetryHistoryManager telemetryHistory;

#endif // TELEMETRY_HISTORY_MANAGER_H
