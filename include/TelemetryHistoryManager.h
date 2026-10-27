#ifndef TELEMETRY_HISTORY_MANAGER_H
#define TELEMETRY_HISTORY_MANAGER_H

#include <Arduino.h>

// 288 points for 24 hours (1 sample every 5 minutes: 24 * 12)
#define TELEMETRY_24H_POINTS 288
// 336 points for 7 days (1 sample every 30 minutes: 7 * 48)
#define TELEMETRY_7D_POINTS  336
// Max event log entries (16 KB flash ring buffer for exact start/stop intervals)
#define MAX_STATE_EVENTS     2048

// Compact 10-byte analog sample
#pragma pack(push, 1)
struct TelemetryPoint {
    int16_t z1_temp;    // Scaled * 10 (e.g. 234 = 23.4°C), -999 = invalid
    int16_t z2_temp;    // Scaled * 10
    int16_t z3_temp;    // Scaled * 10
    int16_t out_temp;   // Scaled * 10 (Outdoor weather temperature)
    uint8_t flags;      // Bit 0: Boiler ON, Bit 1: Pump ON, Bit 2: Z1 Valve, Bit 3: Z2 Valve, Bit 4: Z3 Valve, Bit 7: Valid
    uint8_t reserved;   // Padding / extension
};

// 8-byte state transition event (exact start/stop interval)
struct StateEvent {
    uint32_t epoch;       // Exact UNIX second
    uint8_t  device;      // 0: Boiler, 1: Z1 (Living), 2: Z2 (Copil), 3: Z3 (Matrimonial), 4: Pump
    uint8_t  state;       // 0: STOP/OFF, 1: START/ON
    uint16_t durationSec; // If state==0 (STOP), how long it was ON
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

    // Record an edge-triggered state transition event (Boiler / Relay ON/OFF)
    void recordEvent(uint8_t device, uint8_t state, uint16_t durationSec);

    // Export history as compact JSON for REST API
    String getHistoryJson(const String& range = "24h");

    // Export state transition events for timeline / Gantt chart
    String getEventsJson(uint16_t limit = 100);

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
    TelemetryPoint _ring7d[TELEMETRY_7D_POINTS];
    uint16_t _head7d;
    uint16_t _currentSlot24h;
    uint32_t _lastSampleMs;
    uint32_t _lastSampleEpoch;
    uint16_t _lastRecorded5mSlot;
    uint8_t  _lastRecorded30mSlot;
    bool     _dirty24h;
    bool     _dirty7d;

    // State change tracking
    bool     _lastBoilerState;
    bool     _lastZ1State;
    bool     _lastZ2State;
    bool     _lastZ3State;
    bool     _lastPumpState;
    uint32_t _boilerStartEpoch;
    uint32_t _z1StartEpoch;
    uint32_t _z2StartEpoch;
    uint32_t _z3StartEpoch;
    uint32_t _pumpStartEpoch;

    // Events ring buffer in RAM (mirrored in LittleFS /events.bin)
    StateEvent _events[MAX_STATE_EVENTS];
    uint16_t   _eventHead;
    uint16_t   _eventCount;
    bool       _dirtyEvents;

    void updateSlots(uint32_t epochNow);
    void checkStateTransitions();
    void loadEventsFromFlash();
    void saveEventsToFlash();
};

extern TelemetryHistoryManager telemetryHistory;

#endif // TELEMETRY_HISTORY_MANAGER_H
