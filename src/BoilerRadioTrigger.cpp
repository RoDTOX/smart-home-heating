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

#include <Preferences.h>

BoilerRadioTrigger boilerRadio;

BoilerRadioTrigger::BoilerRadioTrigger()
    : _dataPin(5), _lastCommandOn(false), _hasTransmittedOnce(false),
      _lastTxTime(0), _lastStateChangeTime(0), _pendingAction(PENDING_NONE),
      _spamAttemptCount(0), _lastSpamAttemptMs(0) {
}

void BoilerRadioTrigger::loadStateFromPreferences() {
    Preferences prefs;
    if (prefs.begin("boiler", true)) {
        _lastCommandOn = prefs.getBool("last_cmd", false);
        prefs.end();
    }
}

void BoilerRadioTrigger::saveStateToPreferences() {
    Preferences prefs;
    if (prefs.begin("boiler", false)) {
        bool oldVal = prefs.getBool("last_cmd", false);
        if (oldVal != _lastCommandOn || !prefs.isKey("last_cmd")) {
            prefs.putBool("last_cmd", _lastCommandOn);
        }
        prefs.end();
    }
}

void BoilerRadioTrigger::begin(uint8_t dataPin) {
    _dataPin = dataPin;
    pinMode(_dataPin, OUTPUT);
    digitalWrite(_dataPin, LOW);

    loadStateFromPreferences();

    // Safety & Power-Loss Recovery:
    // The physical 433MHz boiler receiver retains whatever state it held before power cut.
    // At boot/reboot, the controller's safe default state is OFF (Standby).
    // An immediate forced STOP burst guarantees the physical burner is shut down.
    netConsole.printf("[RF 433MHz][BOOT-SAFE] Boot power resume (last NVS state: %s). Emitting failsafe STOP [OFF] burst...\n",
                      _lastCommandOn ? "ON" : "OFF");
    sendBoilerOff(true);
}

void BoilerRadioTrigger::setPendingAction(PendingBoilerAction action) {
    _pendingAction = action;
    uint32_t waitSec = 0;
    canTransmit(action == PENDING_ON, &waitSec);
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
            sendBoilerOff(false);
        }
    } else if (_pendingAction == PENDING_ON) {
        if (canTransmit(true)) {
            netConsole.println(F("[RF 433MHz][QUEUE] Minimum pause expired. Executing deferred boiler START [ON]!"));
            sendBoilerOn(false);
        }
    }

    // 2. Periodic 15-minute safety heartbeat burst (FR-17.7 redundancy)
    if (_hasTransmittedOnce && _pendingAction == PENDING_NONE && (now - _lastTxTime >= 900000UL)) {
        if (_lastCommandOn) {
            transmitBurst(rf_on_durations);
        } else {
            transmitBurst(rf_off_durations);
        }
        _lastTxTime = now;
        netConsole.printf("[RF 433MHz][HEARTBEAT] 15m safety refresh burst emitted (%s).\n",
                          _lastCommandOn ? "ON" : "OFF");
    }
}

bool BoilerRadioTrigger::canTransmit(bool targetOn, uint32_t* remainingSec, bool* isIdempotent) const {
    uint32_t now = millis();

    // 1. Check idempotency (FR-17.1)
    if (_hasTransmittedOnce && _lastCommandOn == targetOn) {
        if (isIdempotent) *isIdempotent = true;
        if (remainingSec) *remainingSec = 0;
        return true;
    }
    if (isIdempotent) *isIdempotent = false;

    // 2. Check RF hardware burst cooldown (FR-17.2: Min 3s)
    if (_lastTxTime > 0 && (now - _lastTxTime < RF_MIN_COOLDOWN_MS)) {
        if (remainingSec) {
            *remainingSec = (RF_MIN_COOLDOWN_MS - (now - _lastTxTime) + 999) / 1000;
        }
        return false;
    }

    // 3. Check Burner Anti-Short-Cycling Guard (FR-17.3 & NFR-1.6: Min 60s run / 60s pause)
    if (_hasTransmittedOnce) {
        if (_lastCommandOn && !targetOn) {
            // Switching ON -> OFF: must run at least BOILER_MIN_RUN_TIME_MS
            if (now - _lastStateChangeTime < BOILER_MIN_RUN_TIME_MS) {
                if (remainingSec) {
                    *remainingSec = (BOILER_MIN_RUN_TIME_MS - (now - _lastStateChangeTime) + 999) / 1000;
                }
                return false;
            }
        } else if (!_lastCommandOn && targetOn) {
            // Switching OFF -> ON: must rest at least BOILER_MIN_PAUSE_TIME_MS
            if (now - _lastStateChangeTime < BOILER_MIN_PAUSE_TIME_MS) {
                if (remainingSec) {
                    *remainingSec = (BOILER_MIN_PAUSE_TIME_MS - (now - _lastStateChangeTime) + 999) / 1000;
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
        diagEngine.raiseDtc("DTC-201", "Rapid boiler toggling / spam detected", SEV_WARNING);
        netConsole.printf("[RF 433MHz] WARN: Excessive boiler spam detected (%u attempts). Raised DTC-201.\n", _spamAttemptCount);
    }
}

bool BoilerRadioTrigger::sendBoilerOn(bool force) {
    bool isIdempotent = false;
    uint32_t waitSec = 0;

    if (!force) {
        if (!canTransmit(true, &waitSec, &isIdempotent)) {
            recordSpamAttempt();
            netConsole.printf("[RF 433MHz] REJECTED: Boiler ON rejected by anti-short-cycling guard. Wait %u s.\n", waitSec);
            return false;
        }

        if (isIdempotent) {
            netConsole.println(F("[RF 433MHz] Boiler already ON. Command skipped (idempotent no-op)."));
            return true;
        }
    }

    netConsole.printf("[RF 433MHz] Transmitting boiler START [ON] command on GPIO%d%s...\n",
                      _dataPin, force ? " [FORCED]" : "");
    transmitBurst(rf_on_durations);
    _lastCommandOn = true;
    _hasTransmittedOnce = true;
    _lastTxTime = millis();
    _lastStateChangeTime = _lastTxTime;
    _spamAttemptCount = 0;
    _pendingAction = PENDING_NONE;
    saveStateToPreferences();
    netConsole.println(F("[RF 433MHz] Boiler ON burst sent successfully."));
    return true;
}

bool BoilerRadioTrigger::sendBoilerOff(bool force) {
    bool isIdempotent = false;
    uint32_t waitSec = 0;

    if (!force) {
        if (!canTransmit(false, &waitSec, &isIdempotent)) {
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
    _lastCommandOn = false;
    _hasTransmittedOnce = true;
    _lastTxTime = millis();
    _lastStateChangeTime = _lastTxTime;
    _spamAttemptCount = 0;
    _pendingAction = PENDING_NONE;
    saveStateToPreferences();
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

