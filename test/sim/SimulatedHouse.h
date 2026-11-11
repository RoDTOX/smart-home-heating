#pragma once

#include <math.h>
#include <stdint.h>
#include "ZoneController.h"

namespace sim {

class MemLearnedStore final : public ILearnedDutyStore {
public:
    int16_t saved[ZoneController::MAX_ZONES] = {0};
    int saves = 0;

    int16_t learned(size_t index) const override {
        return index < ZoneController::MAX_ZONES ? saved[index] : 0;
    }

    void saveLearned(size_t index, int16_t perMille) override {
        if (index < ZoneController::MAX_ZONES) {
            saved[index] = perMille;
        }
        saves++;
    }
};

// Simple 2-node thermal model (room air + floor screed) for PC simulation tests only.
// Never compiled into ESP32 firmware.
struct SimRoom {
    float roomTemp = 20.0f;
    float screedTemp = 20.5f;
    float targetTemp = 22.0f;
    float lastReportedTemp = 20.0f;
    bool sensorSilent = false;
    bool windowOpen = false;
    bool valveOpen = false;

    // 2-node physical constants (1-second step)
    float floorAirTauSec = 3600.0f;  // heat transfer screed -> air
    float houseLossTauSec = 72000.0f; // envelope loss to outdoor
    float windowLossTauSec = 900.0f;  // fast air exchange when window open
    float screedCapRatio = 3.0f;      // screed thermal mass vs air
    float maxHeatPowerPerSec = 3.6f / 3600.0f; // degC/s (air-equivalent) when valve is open

    void step1s(float outdoorTemp) {
        float flowFloorToAir = (screedTemp - roomTemp) / floorAirTauSec;
        float envLoss = (roomTemp - outdoorTemp) / houseLossTauSec;
        float winLoss = windowOpen ? ((roomTemp - outdoorTemp) / windowLossTauSec) : 0.0f;

        float heatIn = valveOpen ? maxHeatPowerPerSec : 0.0f;
        screedTemp += (heatIn - flowFloorToAir) / screedCapRatio;
        roomTemp += (flowFloorToAir - envLoss - winLoss);

        if (!sensorSilent) {
            lastReportedTemp = roomTemp;
        }
    }
};

struct SimResult {
    float minTempAfterDay1 = 99.0f;
    float maxTempAfterDay1 = -99.0f;
    float maxAbsErrorAfterDay1 = 0.0f;
    float minTempOverall = 99.0f;
    float finalTemp = 0.0f;
    uint32_t valveOpenSec = 0;
};

// Runs the ZoneController + SimulatedHouse loop once per simulated second.
template <typename ScenarioFn>
inline SimResult runSimulation(uint32_t totalSeconds, SimRoom& room, ZoneController& zc,
                               MemLearnedStore& store, ScenarioFn&& scenario) {
    zc.begin(&store);
    SimResult res;
    constexpr uint32_t T0 = 10000UL; // non-zero start time for ZoneController cycle tracking

    for (uint32_t sec = 0; sec < totalSeconds; ++sec) {
        uint32_t nowMs = T0 + sec * 1000UL;
        float outdoorTemp = 2.0f;
        bool weatherValid = true;
        scenario(sec, room, outdoorTemp, weatherValid);

        if (room.sensorSilent) {
            room.valveOpen = zc.evaluateFaultDuty(0, nowMs, outdoorTemp, weatherValid, true, 20.0f, 5.0f);
        } else {
            room.valveOpen = zc.evaluateTpi(0, room.lastReportedTemp, room.targetTemp, nowMs,
                                            outdoorTemp, weatherValid, 0.3f);
        }

        room.step1s(outdoorTemp);

        if (room.valveOpen) res.valveOpenSec++;
        if (room.roomTemp < res.minTempOverall) res.minTempOverall = room.roomTemp;
        if (sec >= 86400UL) {
            if (room.roomTemp < res.minTempAfterDay1) res.minTempAfterDay1 = room.roomTemp;
            if (room.roomTemp > res.maxTempAfterDay1) res.maxTempAfterDay1 = room.roomTemp;
            float err = fabsf(room.roomTemp - room.targetTemp);
            if (err > res.maxAbsErrorAfterDay1) res.maxAbsErrorAfterDay1 = err;
        }
    }

    res.finalTemp = room.roomTemp;
    return res;
}

}  // namespace sim
