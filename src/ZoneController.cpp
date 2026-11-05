#include "ZoneController.h"
#include "ParamStore.h"
#include <math.h>

ZoneController zoneController;

ZoneController::ZoneController() {
    for (size_t i = 0; i < MAX_ZONES; i++) {
        _zones[i].cycleStartMs = 0;
        _zones[i].onMs = 0;
        _zones[i].target = 21.0f;
        _zones[i].duty = 0.0f;
        _zones[i].savedIntegral = 0.0f;
        for (size_t b = 0; b < NUM_OUTDOOR_BANDS; b++) {
            _zones[i].learnedBands[b] = 0.0f;
        }
    }
}

void ZoneController::begin() {
    for (size_t i = 0; i < MAX_ZONES; i++) {
        // Load initial learned base duty from ParamStore into default band (band 3: 0..5 C)
        int16_t saved = paramStore.learned(i);
        if (saved > 0 && saved <= 600) {
            float base = (float)saved / 1000.0f;
            _zones[i].savedIntegral = base;
            for (size_t b = 0; b < NUM_OUTDOOR_BANDS; b++) {
                _zones[i].learnedBands[b] = base;
            }
        }
    }
}

uint8_t ZoneController::getOutdoorBand(float outdoorTemp) {
    if (outdoorTemp < -10.0f) return 0;
    if (outdoorTemp < -5.0f) return 1;
    if (outdoorTemp < 0.0f) return 2;
    if (outdoorTemp < 5.0f) return 3;
    if (outdoorTemp < 10.0f) return 4;
    if (outdoorTemp < 15.0f) return 5;
    return 6;
}

bool ZoneController::evaluateTpi(size_t index, float currentTemp, float target, uint32_t now,
                                 float outdoorTemp, bool weatherValid, float hysteresis) {
    if (index >= MAX_ZONES) return false;
    ZoneTpiData& z = _zones[index];

    float err = target - currentTemp;
    bool cycleDone = z.cycleStartMs != 0 && now - z.cycleStartMs >= CYCLE_MS;

    // Deficit rule (FR-47): 100% duty when error >= 0.4 C
    if (err >= 0.4f) {
        z.duty = 1.0f;
        z.onMs = CYCLE_MS;
        z.cycleStartMs = now;
        z.target = target;
        return true;
    }

    uint8_t band = weatherValid ? getOutdoorBand(outdoorTemp) : 3;

    // Start a new cycle every 60 min, when leaving deficit, or when target moves > 0.4 C
    if (z.cycleStartMs == 0 || cycleDone || z.duty >= 1.0f || fabsf(target - z.target) > 0.4f) {
        if (cycleDone && fabsf(err) < 1.0f) {
            z.learnedBands[band] = constrain(z.learnedBands[band] + KI * err, 0.0f, MAX_INTEGRAL);
            if (fabsf(z.learnedBands[band] - z.savedIntegral) >= 0.05f) {
                paramStore.saveLearned(index, (int16_t)lroundf(z.learnedBands[band] * 1000));
                z.savedIntegral = z.learnedBands[band];
            }
        }

        float outdoorComp = 0.0f;
        if (weatherValid && outdoorTemp < 15.0f) {
            outdoorComp = constrain((15.0f - outdoorTemp) * 0.015f, 0.0f, 0.30f);
        }

        float baseDuty = z.learnedBands[band];
        // If current band has no learning, use nearest band with non-zero duty
        if (baseDuty <= 0.0f) {
            for (int offset = 1; offset < (int)NUM_OUTDOOR_BANDS; offset++) {
                if (band + offset < NUM_OUTDOOR_BANDS && z.learnedBands[band + offset] > 0.0f) {
                    baseDuty = z.learnedBands[band + offset];
                    break;
                }
                if ((int)band - offset >= 0 && z.learnedBands[band - offset] > 0.0f) {
                    baseDuty = z.learnedBands[band - offset];
                    break;
                }
            }
        }

        z.duty = constrain(err / BAND_C + outdoorComp + baseDuty, 0.0f, 1.0f);
        uint32_t onMs = (uint32_t)(z.duty * CYCLE_MS);
        if (onMs < MIN_ON_MS) onMs = 0;
        else if (CYCLE_MS - onMs < MIN_ON_MS) onMs = CYCLE_MS;

        z.cycleStartMs = now;
        z.onMs = onMs;
        z.target = target;
    }

    // Overshoot cut: stop if temperature exceeds target + hysteresis
    uint32_t elapsed = now - z.cycleStartMs;
    if (currentTemp >= target + hysteresis && elapsed < z.onMs) {
        z.onMs = elapsed;
        z.duty = 0.0f;
    }

    return elapsed < z.onMs;
}

bool ZoneController::evaluateFaultDuty(size_t index, uint32_t now, float outdoorTemp, bool weatherValid,
                                       bool seasonHeating, float faultDutyPct, float faultFrostOutdoor) {
    if (index >= MAX_ZONES) return false;
    ZoneTpiData& z = _zones[index];

    // Global OFF / season OFF rule (FR-51, FR-64):
    // In global OFF, fault duty runs ONLY if outdoor < faultFrostOutdoor or outdoor is Lost
    if (!seasonHeating) {
        if (weatherValid && outdoorTemp >= faultFrostOutdoor) {
            z.duty = 0.0f;
            z.onMs = 0;
            return false;
        }
    }

    uint8_t band = weatherValid ? getOutdoorBand(outdoorTemp) : 3;
    float learned = z.learnedBands[band];
    float duty = (learned > 0.05f) ? learned : (faultDutyPct / 100.0f);
    duty = constrain(duty, 0.0f, 0.30f); // Bounded by NFR-5.3 (max 30% fault duty)

    if (z.cycleStartMs == 0 || now - z.cycleStartMs >= CYCLE_MS) {
        z.cycleStartMs = now;
    }

    uint32_t onMs = (uint32_t)(duty * CYCLE_MS);
    // PB-61: Minimum valve ON time 10 min (600s)
    if (onMs > 0 && onMs < MIN_ON_MS) {
        onMs = MIN_ON_MS;
    }

    z.onMs = onMs;
    z.duty = duty;

    uint32_t elapsed = now - z.cycleStartMs;
    return elapsed < z.onMs;
}

void ZoneController::stopCycle(size_t index, uint32_t now) {
    if (index >= MAX_ZONES) return;
    uint32_t elapsed = now - _zones[index].cycleStartMs;
    if (elapsed < _zones[index].onMs) {
        _zones[index].onMs = elapsed;
    }
    _zones[index].duty = 0.0f;
}

float ZoneController::getDuty(size_t index) const {
    if (index >= MAX_ZONES) return 0.0f;
    return _zones[index].duty;
}

uint32_t ZoneController::getOnMs(size_t index) const {
    if (index >= MAX_ZONES) return 0;
    return _zones[index].onMs;
}

uint32_t ZoneController::getCycleElapsedMs(size_t index, uint32_t now) const {
    if (index >= MAX_ZONES || _zones[index].cycleStartMs == 0) return 0;
    return now - _zones[index].cycleStartMs;
}

float ZoneController::getLearnedDuty(size_t index, uint8_t band) const {
    if (index >= MAX_ZONES || band >= NUM_OUTDOOR_BANDS) return 0.0f;
    return _zones[index].learnedBands[band];
}

void ZoneController::setLearnedDuty(size_t index, uint8_t band, float duty) {
    if (index >= MAX_ZONES || band >= NUM_OUTDOOR_BANDS) return;
    _zones[index].learnedBands[band] = constrain(duty, 0.0f, MAX_INTEGRAL);
}
