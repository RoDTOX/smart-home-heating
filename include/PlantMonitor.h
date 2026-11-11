#ifndef PLANT_MONITOR_H
#define PLANT_MONITOR_H

#include <stddef.h>
#include <stdint.h>

// FR-56, FR-57: PlantMonitor (Layer 4 Observer)
// Monitors physical heating delivery:
// - FR-56: Zone overheat (valve stuck open or wrong polarity) -> DTC-203
// - FR-57: Zone heating without effect (valve stuck closed) -> DTC-202
// - FR-57: Boiler not heating (all demanding zones stagnant) -> DTC-204
class PlantMonitor {
public:
    struct ZoneEvalResult {
        bool raiseOverheat = false;
        bool clearOverheat = false;
        bool raiseNoEffect = false;
        bool clearNoEffect = false;
        bool demandActive = false;
        bool zoneStagnant = true;
    };

    static PlantMonitor& instance() {
        static PlantMonitor s_inst;
        return s_inst;
    }

    PlantMonitor();

    void begin(uint32_t now = 0);
    void update(uint32_t now); // Called from ControlTask or loop() periodically

    // Pure per-zone evaluation (host-testable without DiagEngine/ParamStore/ThermalZoneManager)
    ZoneEvalResult evaluateZone(size_t zoneIndex, uint32_t now, bool enabled, bool sensorValid,
                                float curTemp, float target, bool valveOpen, float duty,
                                bool isFloor, float overheatMargin, uint32_t overheatTimeMs,
                                float noEffectDutyMin, uint32_t noEffectTimeMs, float noEffectRiseMin);

    static uint32_t effectiveNoEffectTimeMs(bool isFloor, uint32_t configuredNoEffectTimeMs);

    // Status queries
    bool isZoneOverheating(size_t zoneIndex) const;
    bool isZoneNoEffect(size_t zoneIndex) const;
    bool isBoilerNotHeating() const { return _boilerNotHeating; }
    bool isMisdirectionDetected() const { return _misdirectionDetected; }

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
    bool _misdirectionDetected;
    uint32_t _boilerDemandingSinceMs;
    bool _boilerResendAttempted;
    uint32_t _lastCheckMs;
};

extern PlantMonitor plantMonitor;

#endif // PLANT_MONITOR_H
