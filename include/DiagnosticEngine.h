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
    void raiseDtc(const char* code, const char* message, DtcSeverity severity);
    void clearDtc(const char* code);
    void clearAllDtcs();

    size_t getDtcCount() const { return _dtcCount; }
    const DtcRecord* getDtc(size_t index) const;

    // Basic self-test (FR-10.4): checks with rules, no score
    void runChecks(DiagCheck (&out)[DIAG_CHECK_COUNT]) const;
    String checksJson() const;
    String runSelfTest();
    size_t getActiveDtcCount() const;

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
