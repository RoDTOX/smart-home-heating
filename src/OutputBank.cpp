#include "OutputBank.h"
#include "Config.h"
#include "ParamStore.h"

OutputBank outputBank;

void OutputBank::claim(uint8_t pin) {
    if (!ParamStore::validRelayPin(pin)) return;  // a slot that is no zone has no output (pin 0), never touch a GPIO
    pinMode(pin, OUTPUT);
}

void OutputBank::applyDirect(uint8_t pin, bool on) {
    if (!ParamStore::validRelayPin(pin)) return;
    digitalWrite(pin, on ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
}

void OutputBank::applyValve(uint8_t pin, bool open) {
    // NC (normally closed): open valve -> relay energised (HIGH)
    // NO (normally open):   open valve -> relay de-energised (LOW)
    applyDirect(pin, _normallyClosed ? open : !open);
}
