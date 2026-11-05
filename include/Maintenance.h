#ifndef MAINTENANCE_H
#define MAINTENANCE_H

#include <stdint.h>
#include <stddef.h>

// L5 Maintenance module (architecture.md §3.2, FR-19, FR-60, NFR-1.8)
// Covers:
// - QuietGate: prevents/defers OTA updates while boiler is firing or in overrun
// - Rollback: auto-validates new firmware image or rolls back via ESP32 OTA partition API
// - Exercise: weekly valve anti-seize exercise (5 min with boiler OFF)
// - SelfTest: comprehensive diagnostics for relays, sensors, radio, storage
class Maintenance {
public:
    static Maintenance& instance();

    void begin();
    void update();

    // FR-60, NFR-1.8: OTA Quiet Gate
    bool canStartOta(const char** reason = nullptr) const;
    void noteOtaStarted();

    // FR-60: OTA Image Validation & Rollback
    bool isRollbackSupported() const;
    bool isAppValid() const;
    void checkAutomaticRollback();

    // FR-19: Valve Anti-Seize Exercise
    bool isExerciseRunning() const { return _exerciseRunning; }
    uint32_t getExerciseRemainingSec() const;
    uint32_t getLastExerciseEpoch() const { return _lastExerciseEpoch; }
    void triggerManualExercise();

    // FR-60: Self-Test Engine
    struct SubsystemCheck {
        const char* name;
        bool ok;
        const char* detail;
    };

    struct SelfTestReport {
        uint32_t timestamp;
        bool allPassed;
        uint8_t checkCount;
        SubsystemCheck checks[8];
    };

    const SelfTestReport& runSelfTest(bool includeBurn = false);
    const SelfTestReport& getLastReport() const { return _lastReport; }

    Maintenance();

private:
    bool _exerciseRunning;
    uint32_t _exerciseStartMs;
    uint32_t _lastExerciseEpoch;
    bool _appValidated;
    uint32_t _bootTimeMs;

    SelfTestReport _lastReport;

    void updateValveExercise();
};

extern Maintenance maintenance;

#endif // MAINTENANCE_H
