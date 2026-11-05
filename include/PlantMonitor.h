#ifndef PLANT_MONITOR_H
#define PLANT_MONITOR_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// FR-56, FR-57: PlantMonitor (Layer 4 Observer)
// Monitors physical heating delivery:
// - FR-56: Zone overheat (valve stuck open or wrong polarity) -> DTC-203
// - FR-57: Zone heating without effect (valve stuck closed) -> DTC-202
// - FR-57: Boiler not heating (all demanding zones stagnant) -> DTC-204
class PlantMonitor {
public:
    static PlantMonitor& instance() {
        static PlantMonitor s_inst;
        return s_inst;
    }

    PlantMonitor();

    void begin();
    void update(); // Called from ControlTask or loop() periodically

    // Status queries
    bool isZoneOverheating(size_t zoneIndex) const;
    bool isZoneNoEffect(size_t zoneIndex) const;
    bool isBoilerNotHeating() const { return _boilerNotHeating; }

    String getStatusJson() const;

private:
    struct ZoneTracking {
        uint32_t closedSinceMs;
        float closedStartTemp;
        uint32_t overheatDetectedMs;

        uint32_t highDutySinceMs;
        float highDutyStartTemp;
        uint32_t noEffectDetectedMs;
    };

    ZoneTracking _zones[32];
    bool _boilerNotHeating;
    uint32_t _boilerDemandingSinceMs;
    bool _boilerResendAttempted;
    uint32_t _lastCheckMs;
};

extern PlantMonitor plantMonitor;

#endif // PLANT_MONITOR_H
