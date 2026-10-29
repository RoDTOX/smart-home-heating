#ifndef DIAGNOSTIC_ENGINE_H
#define DIAGNOSTIC_ENGINE_H

#include <Arduino.h>
#include "Config.h"

#define MAX_DTC_ENTRIES 16

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
};

class DiagnosticEngine {
public:
    DiagnosticEngine();

    void begin();
    void update(); // Evaluates background health rules every 10 seconds

    // DTC Management
    void raiseDtc(const char* code, const char* message, DtcSeverity severity);
    void clearDtc(const char* code);
    void clearAllDtcs();

    size_t getDtcCount() const { return _dtcCount; }
    const DtcRecord* getDtc(size_t index) const;

    // Health & Self-Test
    uint8_t calculateHealthScore() const;
    String runSelfTest();

    const char* getSeverityStr(DtcSeverity sev) const;

private:
    DtcRecord _dtcRecords[MAX_DTC_ENTRIES];
    size_t _dtcCount;
    uint32_t _lastEvaluationMs;

    void evaluateSensors();
    void evaluateSystem();
};

extern DiagnosticEngine diagEngine;

#endif // DIAGNOSTIC_ENGINE_H
