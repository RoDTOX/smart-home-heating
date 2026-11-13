#ifndef DIAGNOSTIC_ENGINE_H
#define DIAGNOSTIC_ENGINE_H

#include <Arduino.h>
#include "Config.h"

// FR-10.2, requirements section 5: the only codes the firmware can raise (R4), in the order of the table
enum class Dtc : uint8_t {
    ERR_SENSOR_TIMEOUT = 0,         // DTC-101
    WARN_BATTERY_LOW,               // DTC-102
    ERR_TEMP_OUT_OF_BOUNDS,         // DTC-103
    WARN_SENSOR_SUSPECT,            // DTC-104
    WARN_SENSOR_WEAK_SIGNAL,        // DTC-105
    WARN_BOILER_SPAM_DETECTED,      // DTC-201
    ERR_ZONE_NO_EFFECT,             // DTC-202
    ERR_ZONE_OVERHEAT,              // DTC-203
    ERR_BOILER_NOT_HEATING,         // DTC-204
    ERR_FLOW_OVER_LIMIT,            // DTC-205
    WARN_FLOW_SENSOR_FAULT,         // DTC-206
    WARN_BOILER_LINK_NOT_FAILSAFE,  // DTC-207
    ERR_OUTPUT_EXPANSION_FAULT,     // DTC-208
    WARN_LEARNING_RESET,            // DTC-301
    WARN_TIME_NOT_SET,              // DTC-302
    ERR_OTA_ROLLBACK,               // DTC-303
    ERR_SELFTEST_FAILED,            // DTC-304
    WARN_WIFI_SIGNAL_DEGRADED,      // DTC-401
    WARN_HEAP_FRAGMENTATION,        // DTC-402
    WARN_DEFAULT_CREDENTIALS,       // DTC-403
    COUNT
};
#define DTC_COUNT ((size_t)Dtc::COUNT)

// FR-62: event history, a ring of EVENT_RING_SIZE records of 16 bytes kept in one LittleFS file
#define EVENT_RING_SIZE 50
enum EventType : uint8_t { EVT_DTC_RAISED = 0, EVT_DTC_CLEARED = 1, EVT_RESTART = 2 };
struct EventRecord {
    uint32_t time;     // epoch seconds when flags bit 0 is set, else uptime seconds
    uint32_t value;    // restart: esp_reset_reason_t; DTC: repeats beyond the hourly limit
    uint8_t type;      // EventType
    uint8_t code;      // Dtc, 0xFF = none
    uint8_t zone;      // 1..32, 0 = none
    uint8_t flags;     // bit 0: time is epoch
    uint8_t reserved[4];
};

enum DtcSeverity {
    SEV_INFO = 0,
    SEV_WARNING,
    SEV_ERROR,
    SEV_CRITICAL
};

struct DtcRecord {
    char code[12];
    char message[48];
    DtcSeverity severity;
    uint32_t firstRaisedMs;
    uint32_t lastRaisedMs;
    uint16_t occurrenceCount;
    bool active;
    uint32_t zoneMask;  // per-zone codes: bit n-1 = zone n
};

// FR-10.4: one check of the basic self-test - its rule, the measured value and the result
struct DiagCheck {
    const char* id;
    const char* rule;
    const char* result;  // "OK", "FAIL", or "INFO" when the controller cannot verify it
    char value[40];
};

#define DIAG_CHECK_COUNT 7

class DiagnosticEngine {
public:
    DiagnosticEngine();

    void begin();
    void update(); // Evaluates background health rules every 10 seconds

    // DTC Management
    // zone 1..32 for the per-zone codes (DTC-101 ... DTC-105, DTC-202, DTC-203), 0 = no zone / all zones
    void raiseDtc(Dtc dtc, const char* message, DtcSeverity severity, uint8_t zone = 0);
    void clearDtc(Dtc dtc, uint8_t zone = 0);
    void removeZone(uint8_t zoneId);
    void clearAllDtcs();

    size_t getDtcCount() const { return DTC_COUNT; }
    const DtcRecord* getDtc(size_t index) const;  // one record per code, active or not

    // Event history (FR-62)
    void recordRestart(uint32_t resetReason);
    String eventsJson() const;
    void clearEvents();

    // Basic self-test (FR-10.4): checks with rules, no score
    void runChecks(DiagCheck (&out)[DIAG_CHECK_COUNT]) const;
    String checksJson() const;
    String runSelfTest();
    size_t getActiveDtcCount() const;

    const char* getSeverityStr(DtcSeverity sev) const;

private:
    DtcRecord _dtcRecords[DTC_COUNT];
    uint32_t _lastEvaluationMs;
    uint32_t _minHeapSinceCheck;

    EventRecord _events[EVENT_RING_SIZE];
    uint8_t _evHead;       // slot of the next record
    uint8_t _evCount;
    uint64_t _evDirty;     // bit n = slot n not yet written to LittleFS
    bool _fsMounted;

    void addEvent(EventType type, uint8_t code, uint8_t zone, uint32_t value);
    void loadEvents();
    void flushEvents();

    void evaluateSensors();
    void evaluateSystem();
};

extern DiagnosticEngine diagEngine;

#endif // DIAGNOSTIC_ENGINE_H
