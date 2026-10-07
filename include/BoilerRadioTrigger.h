#ifndef BOILER_RADIO_TRIGGER_H
#define BOILER_RADIO_TRIGGER_H

#include <Arduino.h>

class BoilerRadioTrigger {
public:
    BoilerRadioTrigger();
    void begin(uint8_t dataPin = 5);
    void sendBoilerOn();
    void sendBoilerOff();

    bool isLastCommandOn() const { return _lastCommandOn; }
    uint32_t getLastTransmissionTime() const { return _lastTxTime; }

private:
    uint8_t _dataPin;
    bool _lastCommandOn;
    uint32_t _lastTxTime;

    void transmitBurst(const uint16_t* durationsTable);
};

extern BoilerRadioTrigger boilerRadio;

#endif // BOILER_RADIO_TRIGGER_H
