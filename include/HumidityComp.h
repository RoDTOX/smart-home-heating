#ifndef HUMIDITY_COMP_H
#define HUMIDITY_COMP_H

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

    void begin(uint32_t now = 0);
    void update(uint32_t now);

    // Pure calculation helper (host-testable without hardware/BLE)
    static float computeTargetReduction(bool enabled, bool sensorValid,
                                        bool sensorFault, float humidityRh);

    // Query compensation reduction for zone (0.0 .. 0.5 C)
    float getTargetReduction(size_t zoneIndex) const;

private:
    uint32_t _lastUpdateMs;
};

extern HumidityComp humidityComp;

#endif // HUMIDITY_COMP_H
