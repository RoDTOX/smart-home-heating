#ifndef HEATUP_MODEL_H
#define HEATUP_MODEL_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// FR-48, FR-67: HeatUpModel (Layer 4 Advisor)
// Learns per-zone heat-up speed (deg C / hour) and computes pre-heat lead minutes
// Fallback: FR-48 baseline formula [120 + 9*(5 - Tout)], clamped [45..240 min]; 90 min on weather lost.
class HeatUpModel {
public:
    static HeatUpModel& instance() {
        static HeatUpModel s_inst;
        return s_inst;
    }

    HeatUpModel();

    void begin();
    void update(); // Evaluates learning transitions periodically

    uint32_t getPreHeatLeadMinutes(float outdoorTemp, bool weatherValid) const;
    float getZoneRateDegPerHour(size_t zoneIndex) const;
    bool isZoneLearned(size_t zoneIndex) const;
    uint16_t getZoneSamples(size_t zoneIndex) const;

    String getStatusJson() const;

private:
    struct ZoneHeatRate {
        float rateDegPerHour; // default ~0.5 C/h for underfloor screed
        uint16_t samples;
        bool learned;

        // In-flight learning tracker
        uint32_t heatingStartMs;
        float heatingStartTemp;
        bool tracking;
    };

    ZoneHeatRate _zones[32];
    uint32_t _lastUpdateMs;
};

extern HeatUpModel heatUpModel;

#endif // HEATUP_MODEL_H
