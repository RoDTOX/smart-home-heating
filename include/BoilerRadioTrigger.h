#ifndef BOILER_RADIO_TRIGGER_H
#define BOILER_RADIO_TRIGGER_H

#include <Arduino.h>

// Timing Thresholds & Safety Limits [SWE-REQ-006, SWE-REQ-008, SWE-REQ-009]
#define RF_MIN_COOLDOWN_MS       3000UL   // FR-17.2: Min 3s silence between any consecutive RF bursts
#define BOILER_MIN_RUN_TIME_MS   180000UL // FR-17.3: Min 180s burn duration before turning OFF allowed
#define BOILER_MIN_PAUSE_TIME_MS 180000UL // FR-17.3: Min 180s rest duration before turning ON allowed
#define BOILER_HEARTBEAT_MS      900000UL // FR-17.7: Periodic 15m safety heartbeat

enum PendingBoilerAction {
    PENDING_NONE = 0,
    PENDING_OFF  = 1,
    PENDING_ON   = 2
};

class BoilerRadioTrigger {
public:
    BoilerRadioTrigger();
    void begin(uint8_t dataPin = 5); // [SWE-REQ-010 Power-Up Failsafe Burst]
    void update();

    // [SWE-REQ-006, SWE-REQ-007, SWE-REQ-008] Returns true if burst transmitted or idempotent no-op
    bool sendBoilerOn(bool force = false);
    bool sendBoilerOff(bool force = false);

    // Pre-validation without blocking CPU or calling noInterrupts() (FR-17.6)
    bool canTransmit(bool targetOn, uint32_t* remainingSec = nullptr, bool* isIdempotent = nullptr) const;

    // Deferred execution / Pending command queueing
    PendingBoilerAction getPendingAction() const { return _pendingAction; }
    void setPendingAction(PendingBoilerAction action);
    void cancelPendingAction() { _pendingAction = PENDING_NONE; }
    const char* getPendingActionStr() const;

    bool isLastCommandOn() const { return _lastCommandOn; }
    bool hasTransmittedOnce() const { return _hasTransmittedOnce; }
    uint32_t getLastTransmissionTime() const { return _lastTxTime; }
    uint32_t getLastStateChangeTime() const { return _lastStateChangeTime; }
    void recordSpamAttempt();
    uint32_t getSpamAttemptCount() const { return _spamAttemptCount; }

    // FR-18 Boiler Runtime Tracking
    uint32_t getRuntimeTotalSec() const;
    uint32_t getRuntimeTodaySec() const;
    uint32_t getRuntimeYesterdaySec() const { return _runtimeYesterdaySec; }
    uint32_t getRuntimeThisMonthSec() const;
    uint32_t getRuntimeLastMonthSec() const { return _runtimeLastMonthSec; }
    uint32_t getCurrentFlameDurationSec() const;
    void updateRuntimeDayMonth(uint8_t day, uint8_t month);
    void resetRuntime();

    void saveStateToPreferences();
    void loadStateFromPreferences();

    // Configurable RF & Safety Timings
    uint32_t getMinRunTimeMs() const { return _minRunTimeMs; }
    void setMinRunTimeMs(uint32_t ms);
    uint32_t getMinPauseTimeMs() const { return _minPauseTimeMs; }
    void setMinPauseTimeMs(uint32_t ms);
    uint32_t getRfMinCooldownMs() const { return _rfMinCooldownMs; }
    void setRfMinCooldownMs(uint32_t ms);
    uint32_t getSafetyHeartbeatMs() const { return _safetyHeartbeatMs; }
    void setSafetyHeartbeatMs(uint32_t ms);
    bool isSafetyHeartbeatEnabled() const { return _safetyHeartbeatEnabled; }
    void setSafetyHeartbeatEnabled(bool en);

private:
    uint8_t _dataPin;
    bool _lastCommandOn;
    bool _hasTransmittedOnce;
    uint32_t _lastTxTime;
    uint32_t _lastStateChangeTime;
    PendingBoilerAction _pendingAction;

    // Configurable Timers & Safety
    uint32_t _minRunTimeMs;
    uint32_t _minPauseTimeMs;
    uint32_t _rfMinCooldownMs;
    uint32_t _safetyHeartbeatMs;
    bool     _safetyHeartbeatEnabled;

    // Spam & abuse tracking (FR-17.5)
    uint32_t _spamAttemptCount;
    uint32_t _lastSpamAttemptMs;

    // Runtime tracking (FR-18)
    uint32_t _runtimeTotalSec;
    uint32_t _runtimeTodaySec;
    uint32_t _runtimeYesterdaySec;
    uint32_t _runtimeThisMonthSec;
    uint32_t _runtimeLastMonthSec;
    uint32_t _flameStartTimeMs;
    uint8_t  _lastDay;
    uint8_t  _lastMonth;
    uint32_t _lastNvsSaveMs;

    void transmitBurst(const uint16_t* durationsTable);
};

extern BoilerRadioTrigger boilerRadio;

#endif // BOILER_RADIO_TRIGGER_H
