#ifndef BOILER_RADIO_TRIGGER_H
#define BOILER_RADIO_TRIGGER_H

#include <Arduino.h>
#include "EventLog.h"

// FR-17 Timing Thresholds & Safety Limits (Defaults)
#define RF_MIN_COOLDOWN_MS       3000UL   // FR-17.2: fixed minimum silence between two radio frames
// Burner guard (FR-4) and state repeat (FR-17.7) values are requirements §6 parameters in ParamStore.

enum PendingBoilerAction {
    PENDING_NONE = 0,
    PENDING_OFF  = 1,
    PENDING_ON   = 2
};

typedef bool (*BoilerOnPermitFn)();

class BoilerRadioTrigger {
public:
    BoilerRadioTrigger();
    void begin(uint8_t dataPin = 5);
    void update();

    // FR-17: Returns true if burst transmitted or idempotent no-op; false if rate-limited
    // why: the reason that goes into the event log (FR-62.1)
    bool sendBoilerOn(bool force = false, uint8_t why = RS_USER);
    bool sendBoilerOff(bool force = false, uint8_t why = RS_USER);

    // Pre-validation without blocking CPU or calling noInterrupts() (FR-17.6)
    // why (optional): RS_GUARD_COOLDOWN / RS_GUARD_RUN / RS_GUARD_PAUSE when it returns false
    bool canTransmit(bool targetOn, uint32_t* remainingSec = nullptr, bool* isIdempotent = nullptr, uint8_t* why = nullptr) const;

    // Deferred execution / Pending command queueing
    PendingBoilerAction getPendingAction() const { return _pendingAction; }
    void setPendingAction(PendingBoilerAction action, uint8_t why = RS_NONE);  // RS_NONE: the guard that queues it
    void cancelPendingAction(uint8_t why = RS_USER);
    // FR-62.1: a command refused before it reached sendBoilerOn / Off (interlock, burner guard)
    void noteRefusal(uint8_t why, uint32_t waitSec, bool targetOn);
    const char* getPendingActionStr() const;

    // FR-2: every ON, from any source, passes this check (the interlock of the zone valves)
    void setOnPermit(BoilerOnPermitFn fn) { _onPermit = fn; }
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

    void saveState();
    void loadState();

    // Configurable RF & Safety Timings
    // Setters return false and change nothing when the value is outside its requirements §6 range
    uint32_t getMinRunTimeMs() const { return _minRunTimeMs; }
    bool setMinRunSec(uint32_t sec);
    uint32_t getMinPauseTimeMs() const { return _minPauseTimeMs; }
    bool setMinPauseSec(uint32_t sec);
    uint32_t getRfMinCooldownMs() const { return RF_MIN_COOLDOWN_MS; }
    uint32_t getSafetyHeartbeatMs() const { return _safetyHeartbeatMs; }
    bool setHeartbeatSec(uint32_t sec);
    bool isSafetyHeartbeatEnabled() const { return _safetyHeartbeatEnabled; }
    void setSafetyHeartbeatEnabled(bool en);

private:
    void setPending(PendingBoilerAction action, uint8_t why);
    uint8_t _dataPin;
    bool _lastCommandOn;
    bool _hasTransmittedOnce;
    uint32_t _lastTxTime;
    uint32_t _lastStateChangeTime;
    PendingBoilerAction _pendingAction;
    BoilerOnPermitFn _onPermit = nullptr;

    // Configurable Timers & Safety
    uint32_t _minRunTimeMs;
    uint32_t _minPauseTimeMs;
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
