#ifndef BOILER_RADIO_TRIGGER_H
#define BOILER_RADIO_TRIGGER_H

#include <Arduino.h>

// FR-17 Timing Thresholds & Safety Limits
#define RF_MIN_COOLDOWN_MS       3000UL   // FR-17.2: Min 3s silence between any consecutive RF bursts
#define BOILER_MIN_RUN_TIME_MS   60000UL  // FR-17.3: Min 60s burn duration before turning OFF allowed
#define BOILER_MIN_PAUSE_TIME_MS 60000UL  // FR-17.3: Min 60s rest duration before turning ON allowed

enum PendingBoilerAction {
    PENDING_NONE = 0,
    PENDING_OFF  = 1,
    PENDING_ON   = 2
};

class BoilerRadioTrigger {
public:
    BoilerRadioTrigger();
    void begin(uint8_t dataPin = 5);
    void update();

    // FR-17: Returns true if burst transmitted or idempotent no-op; false if rate-limited
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

    void saveStateToPreferences();
    void loadStateFromPreferences();

private:
    uint8_t _dataPin;
    bool _lastCommandOn;
    bool _hasTransmittedOnce;
    uint32_t _lastTxTime;
    uint32_t _lastStateChangeTime;
    PendingBoilerAction _pendingAction;

    // Spam & abuse tracking (FR-17.5)
    uint32_t _spamAttemptCount;
    uint32_t _lastSpamAttemptMs;

    void transmitBurst(const uint16_t* durationsTable);
};

extern BoilerRadioTrigger boilerRadio;

#endif // BOILER_RADIO_TRIGGER_H

