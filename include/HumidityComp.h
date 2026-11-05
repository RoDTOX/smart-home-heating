#ifndef HUMIDITY_COMP_H
#define HUMIDITY_COMP_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// FR-71: HumidityComp (Layer 4 Advisor)
// For zones with humidityComp enabled:
// At relative humidity >= 60 %, lowers control target by up to 0.5 °C.
// Fallback: no humidity reading -> no compensation (0.0 °C reduction).
class HumidityComp {
public:
    static HumidityComp& instance() {
        static HumidityComp s_inst;
        return s_inst;
    }

    HumidityComp();

    void begin();
    void update();

    // Query compensation reduction for zone (0.0 .. 0.5 C)
    float getTargetReduction(size_t zoneIndex) const;

    String getStatusJson() const;

private:
    uint32_t _lastUpdateMs;
};

extern HumidityComp humidityComp;

#endif // HUMIDITY_COMP_H
