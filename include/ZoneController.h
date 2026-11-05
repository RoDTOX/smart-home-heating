#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "Config.h"
#include "ParamStore.h"

/**
 * ZoneController (L4 Control Layer, Step 7, FR-47, FR-51, AR-12)
 *
 * Implements:
 * 1. TPI control law (60 min cycle, 1.5 C band, deficit rule >= 0.4 C -> 100% duty, min valve ON/OFF 10 min)
 * 2. Learned duty table per outdoor temperature band (7 bands of 5 C, AR-12)
 * 3. FR-51 fallback duty upon sensor loss (fixes PB-23, PB-61)
 * 4. Zone control state tracking (DISABLED, ACQUIRING, NORMAL, FAULT, WINDOW)
 */

enum class ZoneControlState : uint8_t {
    Disabled = 0,
    Acquiring,
    Normal,
    Fault,
    Window
};

class ZoneController {
public:
    static constexpr size_t MAX_ZONES = PARAM_ZONES;
    static constexpr size_t NUM_OUTDOOR_BANDS = 7;
    static constexpr uint32_t CYCLE_MS = 3600000UL;      // 60 min
    static constexpr uint32_t MIN_ON_MS = 600000UL;       // 10 min min valve ON time
    static constexpr float BAND_C = 1.5f;                 // 1.5 C proportional band
    static constexpr float KI = 0.05f;                    // Learning rate per cycle
    static constexpr float MAX_INTEGRAL = 0.60f;          // Max learned base duty 60%

    ZoneController();

    void begin();

    // Map outdoor temperature to band index 0..6 (AR-12)
    static uint8_t getOutdoorBand(float outdoorTemp);

    // Evaluate TPI for normal control with valid sensor (FR-47)
    bool evaluateTpi(size_t index, float currentTemp, float target, uint32_t now,
                     float outdoorTemp, bool weatherValid, float hysteresis);

    // Evaluate FR-51 fallback duty when sensor is lost/timeout (FR-51, PB-23, PB-61)
    bool evaluateFaultDuty(size_t index, uint32_t now, float outdoorTemp, bool weatherValid,
                           bool seasonHeating, float faultDutyPct, float faultFrostOutdoor);

    // Reset or stop cycle (e.g. overshoot or pause)
    void stopCycle(size_t index, uint32_t now);

    float getDuty(size_t index) const;
    uint32_t getOnMs(size_t index) const;
    uint32_t getCycleElapsedMs(size_t index, uint32_t now) const;
    float getLearnedDuty(size_t index, uint8_t band) const;
    void setLearnedDuty(size_t index, uint8_t band, float duty);

private:
    struct ZoneTpiData {
        uint32_t cycleStartMs = 0;
        uint32_t onMs = 0;
        float target = 21.0f;
        float duty = 0.0f;
        float savedIntegral = 0.0f;
        float learnedBands[NUM_OUTDOOR_BANDS] = {0.0f};
    };

    ZoneTpiData _zones[MAX_ZONES];
};

extern ZoneController zoneController;
