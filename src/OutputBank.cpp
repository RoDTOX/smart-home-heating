#include "OutputBank.h"
#include "Config.h"
#include "ParamStore.h"

OutputBank outputBank;

void OutputBank::claim(uint8_t pin) {
    if (!ParamStore::validRelayPin(pin)) return;  // a slot that is no zone has no output (pin 0), never touch a GPIO
    pinMode(pin, OUTPUT);
}

// FR-62.1: a change of the level of a relay pin is one entry (queued in RAM, never written here)
void OutputBank::applyDirect(uint8_t pin, bool on, uint8_t reason, uint8_t zone) {
    if (!ParamStore::validRelayPin(pin)) return;
    uint8_t level = on ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL;
    digitalWrite(pin, level);
    static const uint8_t PINS[4] = {PIN_RELAY_ZONE_1, PIN_RELAY_ZONE_2, PIN_RELAY_ZONE_3, PIN_RELAY_SPARE};
    for (uint8_t i = 0; i < 4; i++) {
        if (PINS[i] != pin) continue;
        if (_level[i] != level) {
            eventLog.add(EV_OUT, pin, reason, zone, _level[i], level);
            _level[i] = level;
        }
        break;
    }
}

void OutputBank::applyValve(uint8_t pin, bool open, uint8_t reason, uint8_t zone) {
    // NC (normally closed): open valve -> relay energised (HIGH)
    // NO (normally open):   open valve -> relay de-energised (LOW)
    applyDirect(pin, _normallyClosed ? open : !open, reason, zone);
}
