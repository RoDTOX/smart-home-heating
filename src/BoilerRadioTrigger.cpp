#include "BoilerRadioTrigger.h"
#include "NetworkConsole.h"
#include "DiagnosticEngine.h"
#include <pgmspace.h>

#define RF_NUM_STEPS 409

// Nivelurile logice (0 sau 1) pentru fiecare din cele 409 tranzitii
static const uint8_t rf_states[RF_NUM_STEPS] PROGMEM = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    0, 1, 0, 1, 0, 1, 0, 1, 0,
};

// Duratele in microsecunde pentru comanda de PORNIRE (ON)
static const uint16_t rf_on_durations[RF_NUM_STEPS] PROGMEM = {
    65000, 200, 250, 200, 250, 200, 200, 250, 200, 200, 250, 200, 250, 200, 200, 250,
    200, 250, 200, 200, 250, 200, 250, 200, 200, 250, 200, 250, 200, 200, 250, 200,
    200, 250, 200, 250, 200, 200, 250, 200, 250, 200, 200, 250, 200, 250, 200, 200,
    250, 200, 250, 200, 200, 250, 200, 200, 250, 200, 250, 200, 250, 650, 700, 650,
    450, 200, 500, 150, 500, 200, 450, 200, 200, 450, 200, 450, 200, 450, 500, 150,
    250, 450, 450, 200, 200, 450, 200, 450, 500, 200, 200, 450, 200, 450, 450, 200,
    450, 200, 200, 450, 200, 500, 200, 450, 450, 200, 450, 200, 450, 200, 450, 200,
    500, 200, 450, 200, 450, 200, 450, 200, 500, 200, 450, 200, 450, 200, 450, 200,
    200, 450, 200, 450, 250, 450, 450, 200, 200, 450, 450, 200, 200, 450, 200, 450,
    500, 200, 200, 450, 200, 450, 450, 200, 500, 200, 200, 450, 200, 450, 200, 450,
    450, 200, 450, 200, 500, 200, 450, 200, 450, 200, 450, 200, 450, 200, 450, 200,
    450, 700, 650, 700, 450, 200, 450, 200, 450, 200, 450, 200, 250, 450, 200, 450,
    200, 450, 450, 200, 200, 500, 450, 200, 200, 449, 201, 450, 450, 200, 199, 451,
    199, 500, 451, 200, 450, 200, 200, 450, 200, 450, 200, 450, 500, 200, 450, 200,
    450, 200, 450, 200, 500, 200, 450, 200, 450, 200, 450, 200, 450, 200, 500, 150,
    500, 200, 450, 200, 200, 450, 200, 450, 200, 450, 450, 200, 250, 450, 450, 200,
    200, 450, 200, 450, 500, 200, 200, 450, 200, 450, 450, 200, 450, 200, 200, 450,
    200, 500, 200, 450, 450, 200, 450, 200, 450, 200, 450, 200, 500, 200, 450, 200,
    450, 200, 450, 200, 450, 650, 700, 650, 500, 200, 450, 200, 450, 200, 450, 200,
    200, 500, 200, 450, 200, 450, 450, 200, 200, 450, 450, 200, 200, 500, 200, 450,
    450, 200, 200, 450, 200, 450, 450, 200, 500, 200, 200, 450, 200, 450, 200, 450,
    500, 200, 450, 200, 450, 200, 450, 200, 450, 200, 450, 200, 500, 150, 500, 200,
    450, 200, 450, 200, 450, 200, 450, 200, 250, 450, 200, 450, 200, 450, 450, 200,
    200, 500, 450, 200, 200, 450, 200, 450, 450, 200, 200, 450, 200, 500, 450, 200,
    450, 200, 200, 450, 200, 450, 199, 451, 500, 200, 450, 200, 449, 201, 450, 200,
    500, 199, 451, 200, 450, 200, 450, 200, 65000,
};

// Duratele in microsecunde pentru comanda de OPRIRE (OFF)
static const uint16_t rf_off_durations[RF_NUM_STEPS] PROGMEM = {
    65000, 218, 221, 219, 221, 219, 221, 219, 221, 219, 220, 219, 221, 219, 221, 218,
    221, 218, 221, 219, 221, 219, 221, 219, 221, 219, 221, 219, 221, 219, 221, 218,
    221, 219, 221, 219, 221, 219, 221, 219, 221, 219, 221, 219, 221, 218, 221, 219,
    221, 219, 221, 219, 221, 219, 221, 219, 221, 219, 221, 218, 227, 674, 675, 675,
    470, 198, 458, 198, 458, 198, 458, 198, 216, 454, 203, 454, 203, 454, 458, 198,
    216, 454, 458, 198, 203, 454, 203, 454, 471, 198, 203, 454, 203, 454, 458, 198,
    471, 198, 203, 454, 203, 454, 203, 454, 216, 454, 203, 454, 203, 454, 202, 454,
    471, 198, 458, 198, 458, 198, 458, 199, 475, 198, 458, 198, 459, 198, 458, 198,
    216, 454, 203, 454, 203, 454, 458, 198, 216, 453, 459, 198, 203, 454, 203, 454,
    471, 198, 203, 454, 203, 454, 458, 198, 471, 198, 203, 454, 203, 454, 203, 454,
    215, 454, 203, 454, 203, 454, 203, 454, 471, 198, 459, 198, 458, 198, 458, 198,
    442, 675, 675, 675, 469, 198, 459, 198, 458, 198, 458, 198, 216, 454, 203, 454,
    203, 454, 458, 198, 216, 454, 458, 198, 203, 454, 202, 454, 471, 198, 203, 454,
    203, 454, 458, 198, 471, 198, 203, 454, 203, 454, 203, 454, 216, 454, 203, 453,
    203, 454, 203, 454, 471, 198, 459, 198, 458, 198, 458, 198, 476, 198, 458, 198,
    458, 198, 459, 198, 215, 454, 203, 454, 203, 454, 459, 198, 215, 454, 459, 198,
    203, 453, 203, 454, 471, 198, 203, 454, 203, 454, 459, 198, 471, 198, 203, 454,
    203, 453, 203, 454, 216, 454, 203, 454, 203, 454, 203, 454, 471, 198, 458, 198,
    458, 198, 458, 198, 442, 675, 675, 675, 469, 198, 458, 198, 458, 198, 459, 198,
    215, 454, 203, 454, 203, 454, 458, 198, 216, 454, 458, 198, 203, 454, 203, 454,
    471, 198, 203, 454, 203, 453, 459, 198, 471, 198, 203, 453, 203, 454, 203, 454,
    216, 454, 203, 454, 203, 454, 203, 453, 471, 198, 458, 198, 459, 198, 458, 198,
    475, 198, 459, 198, 458, 198, 458, 198, 216, 454, 203, 453, 203, 454, 459, 198,
    215, 454, 458, 198, 203, 454, 203, 454, 471, 198, 203, 454, 203, 454, 458, 198,
    471, 198, 203, 454, 203, 454, 203, 453, 215, 454, 203, 454, 202, 454, 203, 454,
    471, 198, 458, 198, 459, 198, 458, 198, 65000,
};

#include "ParamStore.h"

BoilerRadioTrigger boilerRadio;

BoilerRadioTrigger::BoilerRadioTrigger()
    : _dataPin(5), _lastCommandOn(false), _hasTransmittedOnce(false),
      _lastTxTime(0), _lastStateChangeTime(0), _pendingAction(PENDING_NONE),
      _minRunTimeMs(180000UL), _minPauseTimeMs(180000UL),  // placeholders until loadState()
      _safetyHeartbeatMs(60000UL), _safetyHeartbeatEnabled(true),
      _spamAttemptCount(0), _lastSpamAttemptMs(0),
      _runtimeTotalSec(0), _runtimeTodaySec(0), _runtimeYesterdaySec(0),
      _runtimeThisMonthSec(0), _runtimeLastMonthSec(0),
      _flameStartTimeMs(0), _lastDay(0), _lastMonth(0), _lastNvsSaveMs(0) {
}

void BoilerRadioTrigger::loadState() {
    _minRunTimeMs = (uint32_t)paramStore.get(Param::BurnerMinRun) * 1000UL;
    _minPauseTimeMs = (uint32_t)paramStore.get(Param::BurnerMinPause) * 1000UL;
    _safetyHeartbeatMs = (uint32_t)paramStore.get(Param::HbInterval) * 1000UL;
    _safetyHeartbeatEnabled = paramStore.get(Param::BoilerLinkHeartbeat) != 0;
    _lastCommandOn = paramStore.state(StateKey::BoilerLastOn) != 0;
    _runtimeTotalSec = paramStore.state(StateKey::RuntimeTotal);
    _runtimeTodaySec = paramStore.state(StateKey::RuntimeToday);
    _runtimeYesterdaySec = paramStore.state(StateKey::RuntimeYesterday);
    _runtimeThisMonthSec = paramStore.state(StateKey::RuntimeThisMonth);
    _runtimeLastMonthSec = paramStore.state(StateKey::RuntimeLastMonth);
    _lastDay = (uint8_t)paramStore.state(StateKey::RuntimeDay);
    _lastMonth = (uint8_t)paramStore.state(StateKey::RuntimeMonth);
}

// ParamStore writes a state value only when it changed (NFR-2.9).
void BoilerRadioTrigger::saveState() {
    paramStore.saveState(StateKey::BoilerLastOn, _lastCommandOn ? 1 : 0);
    paramStore.saveState(StateKey::RuntimeTotal, _runtimeTotalSec);
    paramStore.saveState(StateKey::RuntimeToday, _runtimeTodaySec);
    paramStore.saveState(StateKey::RuntimeYesterday, _runtimeYesterdaySec);
    paramStore.saveState(StateKey::RuntimeThisMonth, _runtimeThisMonthSec);
    paramStore.saveState(StateKey::RuntimeLastMonth, _runtimeLastMonthSec);
    paramStore.saveState(StateKey::RuntimeDay, _lastDay);
    paramStore.saveState(StateKey::RuntimeMonth, _lastMonth);
    _lastNvsSaveMs = millis();
}


void BoilerRadioTrigger::begin(uint8_t dataPin) {
    _dataPin = dataPin;
    pinMode(_dataPin, OUTPUT);
    digitalWrite(_dataPin, LOW);

    loadState();

    // Safety & Power-Loss Recovery:
    // The physical 433MHz boiler receiver retains whatever state it held before power cut.
    // At boot/reboot, the controller's safe default state is OFF (Standby).
    // An immediate forced STOP burst guarantees the physical burner is shut down.
    netConsole.printf("[RF 433MHz][BOOT-SAFE] Boot power resume (last NVS state: %s). Emitting failsafe STOP [OFF] burst...\n",
                      _lastCommandOn ? "ON" : "OFF");
    sendBoilerOff(true, RS_BOOT);
}

void BoilerRadioTrigger::setPending(PendingBoilerAction action, uint8_t why) {
    if (action == _pendingAction) return;
    eventLog.add(EV_PENDING, 0, why, 0, _pendingAction, action);
    _pendingAction = action;
}

void BoilerRadioTrigger::cancelPendingAction(uint8_t why) { setPending(PENDING_NONE, why); }

void BoilerRadioTrigger::noteRefusal(uint8_t why, uint32_t waitSec, bool targetOn) {
    eventLog.add(EV_REFUSED, 0, why, 0, waitSec > 0xFFFF ? 0xFFFF : (uint16_t)waitSec, targetOn ? 1 : 0, true);
}

void BoilerRadioTrigger::setPendingAction(PendingBoilerAction action, uint8_t why) {
    uint32_t waitSec = 0;
    uint8_t guardWhy = RS_NONE;
    canTransmit(action == PENDING_ON, &waitSec, nullptr, &guardWhy);
    setPending(action, why == RS_NONE ? guardWhy : why);
    netConsole.printf("[RF 433MHz][QUEUE] Scheduled deferred boiler %s command (cooldown: %u s remaining).\n",
                      action == PENDING_OFF ? "STOP [OFF]" : "START [ON]", waitSec);
}

const char* BoilerRadioTrigger::getPendingActionStr() const {
    switch (_pendingAction) {
        case PENDING_OFF: return "OFF";
        case PENDING_ON:  return "ON";
        default:          return "NONE";
    }
}

void BoilerRadioTrigger::update() {
    uint32_t now = millis();

    // 1. Process deferred pending commands
    if (_pendingAction == PENDING_OFF) {
        if (canTransmit(false)) {
            netConsole.println(F("[RF 433MHz][QUEUE] Anti-short-cycling delay expired. Executing deferred boiler STOP [OFF]!"));
            sendBoilerOff(false, RS_QUEUE);
        }
    } else if (_pendingAction == PENDING_ON) {
        if (_onPermit && !_onPermit()) {  // FR-2: interlock lost, the start is cancelled, nothing is sent
            setPending(PENDING_NONE, RS_INTERLOCK);
            netConsole.println(F("[RF 433MHz][QUEUE] Deferred boiler START cancelled: no valve open for interlockDelay (FR-2)."));
        } else if (canTransmit(true)) {
            netConsole.println(F("[RF 433MHz][QUEUE] Minimum pause expired. Executing deferred boiler START [ON]!"));
            sendBoilerOn(false, RS_QUEUE);
        }
    }

    // 2. Periodic safety heartbeat burst (FR-17.7 redundancy)
    if (_safetyHeartbeatEnabled && _hasTransmittedOnce && _pendingAction == PENDING_NONE && (now - _lastTxTime >= _safetyHeartbeatMs)) {
        if (_lastCommandOn) {
            transmitBurst(rf_on_durations);
        } else {
            transmitBurst(rf_off_durations);
        }
        _lastTxTime = now;
        eventLog.add(EV_RF, _dataPin, RS_HEARTBEAT, 0, _lastCommandOn ? 1 : 0, _lastCommandOn ? 1 : 0);
        netConsole.printf("[RF 433MHz][HEARTBEAT] %u s safety refresh burst emitted (%s).\n",
                          (unsigned int)(_safetyHeartbeatMs / 1000),
                          _lastCommandOn ? "ON" : "OFF");
    }

    // 3. Periodic runtime accumulation & NVS checkpointing while flame is ON
    if (_lastCommandOn && _flameStartTimeMs > 0 && (now - _lastNvsSaveMs >= 900000UL)) {
        if (now >= _flameStartTimeMs) {
            uint32_t burnSec = (now - _flameStartTimeMs) / 1000;
            if (burnSec < 86400UL) {
                _runtimeTotalSec += burnSec;
                _runtimeTodaySec += burnSec;
                _runtimeThisMonthSec += burnSec;
            }
        }
        _flameStartTimeMs = now;
        saveState();
    }
}

bool BoilerRadioTrigger::canTransmit(bool targetOn, uint32_t* remainingSec, bool* isIdempotent, uint8_t* why) const {
    uint32_t now = millis();

    // 1. Check idempotency (FR-17.1)
    if (_hasTransmittedOnce && _lastCommandOn == targetOn) {
        if (isIdempotent) *isIdempotent = true;
        if (remainingSec) *remainingSec = 0;
        return true;
    }
    if (isIdempotent) *isIdempotent = false;

    // 2. Check RF hardware burst cooldown (Configurable, default 3s)
    if (_lastTxTime > 0 && (now - _lastTxTime < RF_MIN_COOLDOWN_MS)) {
        if (why) *why = RS_GUARD_COOLDOWN;
        if (remainingSec) {
            *remainingSec = (RF_MIN_COOLDOWN_MS - (now - _lastTxTime) + 999) / 1000;
        }
        return false;
    }

    // 3. Check Burner Anti-Short-Cycling Guard (Configurable, default 180s)
    if (_hasTransmittedOnce) {
        if (_lastCommandOn && !targetOn) {
            // Switching ON -> OFF: must run at least _minRunTimeMs
            if (now - _lastStateChangeTime < _minRunTimeMs) {
                if (why) *why = RS_GUARD_RUN;
                if (remainingSec) {
                    *remainingSec = (_minRunTimeMs - (now - _lastStateChangeTime) + 999) / 1000;
                }
                return false;
            }
        } else if (!_lastCommandOn && targetOn) {
            // Switching OFF -> ON: must rest at least _minPauseTimeMs
            if (now - _lastStateChangeTime < _minPauseTimeMs) {
                if (why) *why = RS_GUARD_PAUSE;
                if (remainingSec) {
                    *remainingSec = (_minPauseTimeMs - (now - _lastStateChangeTime) + 999) / 1000;
                }
                return false;
            }
        }
    }

    if (remainingSec) *remainingSec = 0;
    return true;
}

void BoilerRadioTrigger::recordSpamAttempt() {
    uint32_t now = millis();
    if (now - _lastSpamAttemptMs < 10000UL) {
        _spamAttemptCount++;
    } else {
        _spamAttemptCount = 1;
    }
    _lastSpamAttemptMs = now;

    if (_spamAttemptCount >= 3) {
        diagEngine.raiseDtc(Dtc::WARN_BOILER_SPAM_DETECTED, "Rapid boiler toggling / spam detected", SEV_WARNING);
        netConsole.printf("[RF 433MHz] WARN: Excessive boiler spam detected (%u attempts). Raised DTC-201.\n", _spamAttemptCount);
    }
}

bool BoilerRadioTrigger::sendBoilerOn(bool force, uint8_t why) {
    bool isIdempotent = false;
    uint32_t waitSec = 0;

    if (!force) {
        uint8_t guardWhy = RS_NONE;
        if (!canTransmit(true, &waitSec, &isIdempotent, &guardWhy)) {
            noteRefusal(guardWhy, waitSec, true);
            recordSpamAttempt();
            netConsole.printf("[RF 433MHz] REJECTED: Boiler ON rejected by anti-short-cycling guard. Wait %u s.\n", waitSec);
            return false;
        }

        if (isIdempotent) {
            netConsole.println(F("[RF 433MHz] Boiler already ON. Command skipped (idempotent no-op)."));
            return true;
        }
    }
    // FR-2: no ON without an open valve, from any source
    if (_onPermit && !_onPermit()) {
        noteRefusal(RS_INTERLOCK, 0, true);
        netConsole.println(F("[RF 433MHz] REJECTED: Boiler ON needs a valve energised for interlockDelay (FR-2)."));
        return false;
    }

    netConsole.printf("[RF 433MHz] Transmitting boiler START [ON] command on GPIO%d%s...\n",
                      _dataPin, force ? " [FORCED]" : "");
    transmitBurst(rf_on_durations);
    bool wasOn = _lastCommandOn;
    _lastCommandOn = true;
    _flameStartTimeMs = millis();
    _hasTransmittedOnce = true;
    _lastTxTime = millis();
    _lastStateChangeTime = _lastTxTime;
    _spamAttemptCount = 0;
    setPending(PENDING_NONE, why);
    eventLog.add(EV_RF, _dataPin, why, 0, wasOn ? 1 : 0, 1);
    saveState();
    netConsole.println(F("[RF 433MHz] Boiler ON burst sent successfully."));
    return true;
}

bool BoilerRadioTrigger::sendBoilerOff(bool force, uint8_t why) {
    bool isIdempotent = false;
    uint32_t waitSec = 0;

    if (!force) {
        uint8_t guardWhy = RS_NONE;
        if (!canTransmit(false, &waitSec, &isIdempotent, &guardWhy)) {
            noteRefusal(guardWhy, waitSec, false);
            recordSpamAttempt();
            netConsole.printf("[RF 433MHz] REJECTED: Boiler OFF rejected by anti-short-cycling guard. Wait %u s.\n", waitSec);
            return false;
        }

        if (isIdempotent) {
            netConsole.println(F("[RF 433MHz] Boiler already OFF. Command skipped (idempotent no-op)."));
            return true;
        }
    }

    netConsole.printf("[RF 433MHz] Transmitting boiler STOP [OFF] command on GPIO%d%s...\n",
                      _dataPin, force ? " [FORCED]" : "");
    transmitBurst(rf_off_durations);

    if (_lastCommandOn && _flameStartTimeMs > 0) {
        uint32_t now = millis();
        if (now >= _flameStartTimeMs) {
            uint32_t burnSec = (now - _flameStartTimeMs) / 1000;
            if (burnSec < 86400UL) {
                _runtimeTotalSec += burnSec;
                _runtimeTodaySec += burnSec;
                _runtimeThisMonthSec += burnSec;
                netConsole.printf("[Boiler][Runtime] Burn session finished (%u s). Today total: %u s (%u min).\n",
                                  burnSec, _runtimeTodaySec, _runtimeTodaySec / 60);
            } else {
                netConsole.printf("[Boiler][Runtime] Ignored corrupt burnSec: %u s\n", burnSec);
            }
        }
        _flameStartTimeMs = 0;
    }
    bool wasOn = _lastCommandOn;
    _lastCommandOn = false;
    _hasTransmittedOnce = true;
    _lastTxTime = millis();
    _lastStateChangeTime = _lastTxTime;
    _spamAttemptCount = 0;
    setPending(PENDING_NONE, why);
    eventLog.add(EV_RF, _dataPin, why, 0, wasOn ? 1 : 0, 0);
    saveState();
    netConsole.println(F("[RF 433MHz] Boiler OFF burst sent successfully."));
    return true;
}

void BoilerRadioTrigger::transmitBurst(const uint16_t* durationsTable) {
    // 1. Preamble silence (step 0 is 65ms LOW)
    digitalWrite(_dataPin, LOW);
    delay(65);

    // 2. Active OOK modulated pulse train (~131ms with interrupts disabled for microsecond precision)
    noInterrupts();
    for (uint16_t i = 1; i < RF_NUM_STEPS - 1; i++) {
        uint8_t state = pgm_read_byte_near(rf_states + i);
        uint16_t duration = pgm_read_word_near(durationsTable + i);

        digitalWrite(_dataPin, state ? HIGH : LOW);
        delayMicroseconds(duration);
    }
    digitalWrite(_dataPin, LOW);
    interrupts();

    // 3. Postamble silence (final step is 65ms LOW)
    delay(65);
}

uint32_t BoilerRadioTrigger::getCurrentFlameDurationSec() const {
    if (!_lastCommandOn || _flameStartTimeMs == 0) return 0;
    uint32_t now = millis();
    if (now < _flameStartTimeMs) return 0;
    return (now - _flameStartTimeMs) / 1000;
}

uint32_t BoilerRadioTrigger::getRuntimeTotalSec() const {
    return _runtimeTotalSec + getCurrentFlameDurationSec();
}

uint32_t BoilerRadioTrigger::getRuntimeTodaySec() const {
    return _runtimeTodaySec + getCurrentFlameDurationSec();
}

uint32_t BoilerRadioTrigger::getRuntimeThisMonthSec() const {
    return _runtimeThisMonthSec + getCurrentFlameDurationSec();
}

void BoilerRadioTrigger::updateRuntimeDayMonth(uint8_t day, uint8_t month) {
    if (day == 0 || month == 0) return;
    if (_lastDay == 0) {
        _lastDay = day;
        _lastMonth = month;
        return;
    }
    bool changed = false;
    if (day != _lastDay) {
        _runtimeYesterdaySec = _runtimeTodaySec;
        _runtimeTodaySec = 0;
        _lastDay = day;
        changed = true;
        netConsole.printf("[Boiler][Runtime] Rollover to new day %u. Yesterday runtime: %u s.\n", day, _runtimeYesterdaySec);
    }
    if (month != _lastMonth) {
        _runtimeLastMonthSec = _runtimeThisMonthSec;
        _runtimeThisMonthSec = 0;
        _lastMonth = month;
        changed = true;
        netConsole.printf("[Boiler][Runtime] Rollover to new month %u. Last month runtime: %u s.\n", month, _runtimeLastMonthSec);
    }
    if (changed) {
        saveState();
    }
}

void BoilerRadioTrigger::resetRuntime() {
    _runtimeTotalSec = 0;
    _runtimeTodaySec = 0;
    _runtimeYesterdaySec = 0;
    _runtimeThisMonthSec = 0;
    _runtimeLastMonthSec = 0;
    _flameStartTimeMs = _lastCommandOn ? millis() : 0;
    saveState();
    netConsole.println(F("[Boiler][Runtime] All burner runtime counters reset to 0."));
}



bool BoilerRadioTrigger::setMinRunSec(uint32_t sec) {
    if (!paramStore.set(Param::BurnerMinRun, (int32_t)sec)) return false;
    _minRunTimeMs = sec * 1000UL;
    return true;
}

bool BoilerRadioTrigger::setMinPauseSec(uint32_t sec) {
    if (!paramStore.set(Param::BurnerMinPause, (int32_t)sec)) return false;
    _minPauseTimeMs = sec * 1000UL;
    return true;
}

bool BoilerRadioTrigger::setHeartbeatSec(uint32_t sec) {
    if (!paramStore.set(Param::HbInterval, (int32_t)sec)) return false;
    _safetyHeartbeatMs = sec * 1000UL;
    return true;
}

void BoilerRadioTrigger::setSafetyHeartbeatEnabled(bool en) {
    _safetyHeartbeatEnabled = en;
    paramStore.set(Param::BoilerLinkHeartbeat, en ? 1 : 0);
}
