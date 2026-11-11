#ifndef CYCLE_ALIGNER_H
#define CYCLE_ALIGNER_H

#include <stddef.h>
#include <stdint.h>

// FR-63: CycleAligner (Layer 4 Advisor)
// Synchronizes TPI PWM cycle starts across zones so open valves overlap,
// and ensures small zones below minLoops circuits are grouped or extended,
// preventing boiler burner short-cycling.
class CycleAligner {
public:
    static CycleAligner& instance() {
        static CycleAligner s_inst;
        return s_inst;
    }

    CycleAligner();

    void begin(uint32_t now = 0);
    void update(uint32_t now);

    // Pure calculation helpers (host-testable without RTC/NVS/thermalZones)
    static uint32_t computeCycleElapsedSec(uint32_t nowMs, int64_t epochNow);
    static bool computeZoneExtended(bool activeDemandOrRelay, uint8_t circuits, uint8_t minLoops);

    // Query global cycle status
    uint32_t getCycleLengthSec() const { return 3600UL; }
    uint32_t getCycleElapsedSec(uint32_t nowMs = 0) const;
    uint8_t getMinLoops() const;
    uint8_t getActiveCircuits() const;

    // Alignment and grouping status
    bool isZoneAligned(size_t zoneIndex) const;
    bool isZoneExtended(size_t zoneIndex) const;

private:
    uint32_t _cycleStartEpoch;
    uint32_t _lastUpdateMs;
};

extern CycleAligner cycleAligner;

#endif // CYCLE_ALIGNER_H
