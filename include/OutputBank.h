#ifndef OUTPUT_BANK_H
#define OUTPUT_BANK_H

#include <Arduino.h>
#include "EventLog.h"

// Relay outputs of the board (architecture 12.1, AR-8, R-A1): the only module that calls pinMode / digitalWrite on
// a relay pin, and the only place the NC / NO polarity of the zone valves is applied (FR-15.1).
class OutputBank {
public:
    // The actuator type (parameter `actuatorNormallyClosed`); loaded by ThermalZoneManager from ParamStore.
    void setNormallyClosed(bool nc) { _normallyClosed = nc; }
    bool normallyClosed() const { return _normallyClosed; }

    // A pin that is (or becomes) a relay output: made an output. The caller drives it closed (NFR-1.4).
    void claim(uint8_t pin);

    // Zone valve: NC energises the relay to open the valve, NO de-energises it (FR-15.1).
    // reason and zone only go into the event log (FR-62.1)
    void applyValve(uint8_t pin, bool open, uint8_t reason = RS_NONE, uint8_t zone = 0);

    // Pump, or an output no zone uses: energised = on, de-energised = off.
    void applyDirect(uint8_t pin, bool on, uint8_t reason = RS_NONE, uint8_t zone = 0);

private:
    bool _normallyClosed = true;
    uint8_t _level[4] = {255, 255, 255, 255};  // last level written to the relay pins, 255 = not yet (FR-62.1)
};

extern OutputBank outputBank;

#endif // OUTPUT_BANK_H
