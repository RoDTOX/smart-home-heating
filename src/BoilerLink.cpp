#include "BoilerLink.h"
#include "NetworkConsole.h"
#include <pgmspace.h>

#define RF_NUM_STEPS 409
#define RF_COOLDOWN_MS 3000UL

// Logical levels (0 or 1) for each of the 409 transitions (FR-5.1)
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

// Durations in microseconds for START (ON) command (FR-5.1)
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

// Durations in microseconds for STOP (OFF) command (FR-5.1)
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

// -----------------------------------------------------------------------------
// Rf433Link Implementation
// -----------------------------------------------------------------------------

Rf433Link::Rf433Link()
    : _pin(19), _lastState(false), _hasTransmitted(false),
      _lastTxMs(0), _heartbeatIntervalMs(60000UL), _heartbeatEnabled(true) {
}

void Rf433Link::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
}

void Rf433Link::transmitBurst(bool on) {
    const uint16_t* durationsTable = on ? rf_on_durations : rf_off_durations;

    // 1. Preamble silence: 65 ms LOW (FR-5.1)
    digitalWrite(_pin, LOW);
    delay(65);

    // 2. Active OOK modulated pulse train (~131 ms with interrupts disabled, FR-17.6)
    noInterrupts();
    for (uint16_t i = 1; i < RF_NUM_STEPS - 1; i++) {
        uint8_t state = pgm_read_byte_near(rf_states + i);
        uint16_t duration = pgm_read_word_near(durationsTable + i);
        digitalWrite(_pin, state ? HIGH : LOW);
        delayMicroseconds(duration);
    }
    interrupts();

    // 3. Post-amble silence: 65 ms LOW (FR-5.1)
    digitalWrite(_pin, LOW);
    delay(65);
}

SendResult Rf433Link::send(bool on, bool force) {
    uint32_t now = millis();

    // Idempotency: no frame if already in target state (FR-17.1)
    if (!force && _hasTransmitted && (_lastState == on)) {
        return SendResult::Idempotent;
    }

    // Cooldown: at least 3 s between two frames (FR-17.2)
    if (!force && _hasTransmitted && (now - _lastTxMs < RF_COOLDOWN_MS)) {
        return SendResult::Cooldown;
    }

    transmitBurst(on);
    _lastState = on;
    _hasTransmitted = true;
    _lastTxMs = millis();

    netConsole.printf("[Rf433Link] Transmitted %s burst on GPIO%d (cooldown 3s).\n",
                      on ? "START [ON]" : "STOP [OFF]", _pin);
    return SendResult::Ok;
}

bool Rf433Link::resend() {
    uint32_t now = millis();
    if (_hasTransmitted && (now - _lastTxMs < RF_COOLDOWN_MS)) {
        return false;
    }
    transmitBurst(_lastState);
    _lastTxMs = millis();
    netConsole.printf("[Rf433Link] Re-sent %s state burst on GPIO%d.\n",
                      _lastState ? "ON" : "OFF", _pin);
    return true;
}

void Rf433Link::tick(uint32_t nowMs) {
    // State repeat / safety heartbeat at hbInterval (FR-17.7, FR-55)
    if (_heartbeatEnabled && _hasTransmitted && (_heartbeatIntervalMs > 0) &&
        (nowMs - _lastTxMs >= _heartbeatIntervalMs)) {
        transmitBurst(_lastState);
        _lastTxMs = nowMs;
        netConsole.printf("[Rf433Link][HEARTBEAT] Emitted %u s safety heartbeat (%s).\n",
                          (unsigned int)(_heartbeatIntervalMs / 1000),
                          _lastState ? "ON" : "OFF");
    }
}

// -----------------------------------------------------------------------------
// ContactLink Implementation
// -----------------------------------------------------------------------------

ContactLink::ContactLink()
    : _pin(0), _lastState(false), _hasTransmitted(false), _lastTxMs(0) {
}

void ContactLink::begin(uint8_t pin) {
    _pin = pin;
    if (_pin > 0) {
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW); // Normally open: power loss opens contact -> boiler OFF (FR-55)
    }
    _lastState = false;
    _hasTransmitted = true;
    _lastTxMs = millis();
}

SendResult ContactLink::send(bool on, bool force) {
    if (!force && _hasTransmitted && (_lastState == on)) {
        return SendResult::Idempotent;
    }

    if (_pin > 0) {
        digitalWrite(_pin, on ? HIGH : LOW);
    }
    _lastState = on;
    _hasTransmitted = true;
    _lastTxMs = millis();

    netConsole.printf("[ContactLink] Switched contact relay on GPIO%d -> %s\n",
                      _pin, on ? "CLOSED (ON)" : "OPEN (OFF)");
    return SendResult::Ok;
}

bool ContactLink::resend() {
    if (_pin > 0) {
        digitalWrite(_pin, _lastState ? HIGH : LOW);
    }
    _lastTxMs = millis();
    return true;
}

void ContactLink::tick(uint32_t nowMs) {
    // Dry contact relay maintains its physical position, no RF heartbeat frame required
}
