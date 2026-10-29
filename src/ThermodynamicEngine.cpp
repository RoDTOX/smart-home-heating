#include "ThermodynamicEngine.h"
#include <time.h>

ThermodynamicEngine::ThermodynamicEngine() {
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        // Defaults: Living & Kitchen tiles, Master bedroom parquet
        _roomConfigs[i].floorType = (i == 2) ? FLOOR_PARQUET : FLOOR_TILES;
        _roomConfigs[i].screedThicknessCm = 5.0f;
        _roomConfigs[i].insulation = INSULATION_MEDIUM;

        _metrics[i] = calculateDefaultPriors(_roomConfigs[i]);
        _persistedMetrics[i] = _metrics[i];

        // Initialize Window circular buffer
        for (size_t j = 0; j < WINDOW_BUFFER_SIZE; j++) {
            _windowBuffers[i].samples[j] = 0.0f;
            _windowBuffers[i].sampleTimestamps[j] = 0;
        }
        _windowBuffers[i].head = 0;
        _windowBuffers[i].count = 0;
        _windowBuffers[i].firstDerivative = 0.0f;
        _windowBuffers[i].secondDerivative = 0.0f;
        _windowBuffers[i].windowOpen = false;
        _windowBuffers[i].detectionEpoch = 0;
        _windowBuffers[i].pauseStartMs = 0;
        _windowBuffers[i].pauseMinTemp = 999.0f;
        _windowBuffers[i].recoveryCount = 0;
        _windowBuffers[i].lastDetectionDurationSec = 0;
        _windowBuffers[i].dropThreshold = 1.2f; // Default 1.2°C fallback
        _windowBuffers[i].lastSampleMs = 0;
        _windowBuffers[i].lastSampleTemp = 0.0f;

        // Step response test
        _stepTests[i].state = STEP_IDLE;
        _stepTests[i].startMs = 0;
        _stepTests[i].startTemp = 0.0f;
        _stepTests[i].detectedTdeadMin = 0.0f;
        _stepTests[i].pulseEndTemp = 0.0f;
        _stepTests[i].pulseEndMs = 0;
        _stepTests[i].peakCoastTemp = 0.0f;
        _stepTests[i].peakCoastMs = 0;
        _stepTests[i].activeRelayDemand = false;

        // Passive night calibration
        _nightCalib[i].active = false;
        _nightCalib[i].startMs = 0;
        _nightCalib[i].startEpoch = 0;
        _nightCalib[i].startTemp = 0.0f;
        _nightCalib[i].extTempSum = 0.0f;
        _nightCalib[i].extTempCount = 0;
        _nightCalib[i].lastSampleMs = 0;
        _nightCalib[i].lastValidOutdoorMs = 0;
        _nightCalib[i].aerationOccurred = false;
    }
}

void ThermodynamicEngine::begin() {
    // Initial setup if needed
}

void ThermodynamicEngine::setRoomConfig(size_t index, const RoomCalibrationConfig& config) {
    if (index >= MAX_THERMAL_ZONES) return;
    _roomConfigs[index] = config;
    // When room configuration is manually updated, update priors if uncalibrated
    CalibratedMetrics priors = calculateDefaultPriors(config);
    // Keep calibrated values if already dynamic, but adapt prior baseline
    _metrics[index].naturalLossRate = priors.naturalLossRate;
    _metrics[index].overshootDelta = priors.overshootDelta;
}

const RoomCalibrationConfig& ThermodynamicEngine::getRoomConfig(size_t index) const {
    static RoomCalibrationConfig dummy = {FLOOR_TILES, 5.0f, INSULATION_MEDIUM};
    if (index >= MAX_THERMAL_ZONES) return dummy;
    return _roomConfigs[index];
}

bool ThermodynamicEngine::setRoomConfig(size_t index, const char* floorStr, float screedCm, const char* insulationStr) {
    if (index >= MAX_THERMAL_ZONES) return false;
    RoomCalibrationConfig cfg;
    cfg.floorType = parseFloorType(floorStr);
    cfg.screedThicknessCm = (screedCm >= 3.0f && screedCm <= 10.0f) ? screedCm : 5.0f;
    cfg.insulation = parseInsulationLevel(insulationStr);
    setRoomConfig(index, cfg);
    return true;
}

const CalibratedMetrics& ThermodynamicEngine::getMetrics(size_t index) const {
    static CalibratedMetrics dummy = {30.0f, 1.0f, 0.025f, 0.35f, 45};
    if (index >= MAX_THERMAL_ZONES) return dummy;
    return _metrics[index];
}

void ThermodynamicEngine::setMetrics(size_t index, const CalibratedMetrics& metrics) {
    if (index >= MAX_THERMAL_ZONES) return;
    _metrics[index] = metrics;
}

void ThermodynamicEngine::resetZoneToPriors(size_t index) {
    if (index >= MAX_THERMAL_ZONES) return;
    _metrics[index] = calculateDefaultPriors(_roomConfigs[index]);
}

CalibratedMetrics ThermodynamicEngine::calculateDefaultPriors(const RoomCalibrationConfig& config) {
    CalibratedMetrics m;
    float screedRatio = config.screedThicknessCm / 5.0f;
    if (screedRatio < 0.6f) screedRatio = 0.6f;
    if (screedRatio > 2.0f) screedRatio = 2.0f;

    // Transport dead time
    float baseDeadTime = (config.floorType == FLOOR_TILES) ? 30.0f : 60.0f;
    m.tDeadMin = baseDeadTime * screedRatio;
    if (m.tDeadMin < 15.0f) m.tDeadMin = 15.0f;
    if (m.tDeadMin > 120.0f) m.tDeadMin = 120.0f;

    // Heating slope
    float baseSlope = (config.floorType == FLOOR_TILES) ? 1.10f : 0.75f;
    m.heatingSlope = baseSlope / sqrtf(screedRatio);
    if (m.heatingSlope < 0.3f) m.heatingSlope = 0.3f;
    if (m.heatingSlope > 2.5f) m.heatingSlope = 2.5f;

    // Envelope thermal loss rate k_loss
    switch (config.insulation) {
        case INSULATION_POOR:    m.naturalLossRate = 0.038f; break;
        case INSULATION_MEDIUM:  m.naturalLossRate = 0.025f; break;
        case INSULATION_GOOD:    m.naturalLossRate = 0.018f; break;
        case INSULATION_PASSIVE: m.naturalLossRate = 0.010f; break;
        default:                 m.naturalLossRate = 0.025f; break;
    }

    // Screed coasting thermal overshoot
    float baseOvershoot = (config.floorType == FLOOR_TILES) ? 0.35f : 0.25f;
    m.overshootDelta = baseOvershoot * screedRatio;
    if (m.overshootDelta < 0.10f) m.overshootDelta = 0.10f;
    if (m.overshootDelta > 0.90f) m.overshootDelta = 0.90f;

    // Default preheat lead time
    m.preheatLeadMin = (uint32_t)(m.tDeadMin + 30.0f);
    return m;
}

// ---------------------------------------------------------------------------
// Requirement R2: Rapid Derivative Open-Window Detection Engine
// ---------------------------------------------------------------------------

void ThermodynamicEngine::evaluateWindow(size_t index, float currentTemp, float outdoorTemp, bool outdoorValid, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return;
    WindowCircularBuffer& buf = _windowBuffers[index];

    // Push new sample every 5 minutes (or on initial boot)
    bool isNewSample = false;
    if (buf.lastSampleMs == 0 || (now - buf.lastSampleMs >= WINDOW_SAMPLE_INTERVAL_MS)) {
        buf.samples[buf.head] = currentTemp;
        buf.sampleTimestamps[buf.head] = now;
        buf.head = (buf.head + 1) % WINDOW_BUFFER_SIZE;
        if (buf.count < WINDOW_BUFFER_SIZE) buf.count++;
        buf.lastSampleMs = now;
        buf.lastSampleTemp = currentTemp;
        isNewSample = true;

        // Compute 1st derivative (dT/dt) and 2nd derivative (d²T/dt²)
        if (buf.count >= 2) {
            uint8_t idx0 = (buf.head + WINDOW_BUFFER_SIZE - 1) % WINDOW_BUFFER_SIZE; // latest sample
            uint8_t idx1 = (buf.head + WINDOW_BUFFER_SIZE - 2) % WINDOW_BUFFER_SIZE; // 1 step ago

            float dt01 = (buf.sampleTimestamps[idx0] - buf.sampleTimestamps[idx1]) / 60000.0f; // in min
            if (dt01 < 1.0f) dt01 = 5.0f;
            float v01 = (buf.samples[idx0] - buf.samples[idx1]) / dt01; // °C/min

            if (buf.count >= 3) {
                uint8_t idx2 = (buf.head + WINDOW_BUFFER_SIZE - 3) % WINDOW_BUFFER_SIZE; // 2 steps ago
                float dt12 = (buf.sampleTimestamps[idx1] - buf.sampleTimestamps[idx2]) / 60000.0f;
                if (dt12 < 1.0f) dt12 = 5.0f;
                float v12 = (buf.samples[idx1] - buf.samples[idx2]) / dt12; // °C/min

                // Two-step central 1st derivative over ~10 min window
                buf.firstDerivative = (buf.samples[idx0] - buf.samples[idx2]) / (dt01 + dt12);
                // 2nd derivative: acceleration of temperature drop
                buf.secondDerivative = (v01 - v12) / (0.5f * (dt01 + dt12));
            } else {
                buf.firstDerivative = v01;
                buf.secondDerivative = 0.0f;
            }
        }
    }

    // Base derivative strictly on the smoothed multi-sample circular buffer
    float effectiveDerivative = buf.firstDerivative;

    // Calibrated natural cooling rate for this zone given outdoor weather
    float deltaExt = outdoorValid ? (currentTemp - outdoorTemp) : 10.0f;
    if (deltaExt < 2.0f) deltaExt = 2.0f;
    float kLoss = (_metrics[index].naturalLossRate > 0.005f) ? _metrics[index].naturalLossRate : 0.025f;
    float naturalRateMin = (kLoss * deltaExt) / 60.0f; // °C/min
    if (naturalRateMin < 0.0025f) naturalRateMin = 0.0025f; // min 0.15°C/h

    if (!buf.windowOpen) {
        // Find recent baseline (maximum temp in recent 10-15m window)
        float maxRecent = currentTemp;
        uint8_t samplesToCheck = (buf.count >= 4) ? 4 : buf.count;
        for (uint8_t i = 1; i <= samplesToCheck; i++) {
            uint8_t sIdx = (buf.head + WINDOW_BUFFER_SIZE - i) % WINDOW_BUFFER_SIZE;
            if (buf.samples[sIdx] > maxRecent) maxRecent = buf.samples[sIdx];
        }
        float dropRecent = maxRecent - currentTemp;

        // Trigger condition:
        // 1. Rate of drop exceeds 2.5x natural cooling curve AND cumulative drop >= 0.35°C
        // 2. OR severe sudden drop (rate >= 4.0x natural rate AND cumulative drop >= 0.35°C)
        // 3. OR backward-compatible drop threshold (e.g. 1.2°C drop) evaluated anytime
        float minDrop = (buf.dropThreshold < 0.35f) ? (buf.dropThreshold * 0.7f) : 0.35f;
        bool rateTrigger = isNewSample && (effectiveDerivative < 0.0f) && (fabsf(effectiveDerivative) >= 2.5f * naturalRateMin) && (dropRecent >= minDrop);
        bool severeTrigger = isNewSample && (effectiveDerivative < 0.0f) && (fabsf(effectiveDerivative) >= 4.0f * naturalRateMin) && (dropRecent >= minDrop);
        bool thresholdTrigger = (dropRecent >= buf.dropThreshold);

        if (rateTrigger || severeTrigger || thresholdTrigger) {
            buf.windowOpen = true;
            buf.pauseStartMs = now;
            buf.pauseMinTemp = currentTemp;
            buf.recoveryCount = 0;
            time_t nowEpoch = time(nullptr);
            buf.detectionEpoch = (nowEpoch >= 1700000000) ? (uint32_t)nowEpoch : (now / 1000);
        }
    } else {
        // Window state is ACTIVE: evaluate automatic recovery
        if (currentTemp < buf.pauseMinTemp) {
            buf.pauseMinTemp = currentTemp;
        }

        // Automatic Recovery Criteria:
        // 1. dT/dt >= 0 for 2 consecutive 5-min samples (window closed and stabilized)
        // 2. OR temperature rebound by >= 0.6°C above pause minimum
        // 3. OR 30-minute safety timeout
        bool timeoutExpired = (now - buf.pauseStartMs >= 1800000UL); // 30 min
        bool rebounded = (currentTemp >= buf.pauseMinTemp + 0.6f);

        if (isNewSample) {
            if (buf.firstDerivative >= -0.005f) {
                buf.recoveryCount++;
            } else {
                buf.recoveryCount = 0;
            }
        }
        bool derivativeRecovered = (buf.recoveryCount >= 2);

        if (timeoutExpired || rebounded || derivativeRecovered) {
            buf.lastDetectionDurationSec = (now - buf.pauseStartMs) / 1000;
            buf.windowOpen = false;
            buf.recoveryCount = 0;

            // Re-baseline circular buffer with current temperature
            buf.lastSampleMs = now;
            buf.lastSampleTemp = currentTemp;
            buf.head = 0;
            buf.count = 1;
            buf.samples[0] = currentTemp;
            buf.sampleTimestamps[0] = now;
            buf.firstDerivative = 0.0f;
            buf.secondDerivative = 0.0f;
        }
    }
}

bool ThermodynamicEngine::isWindowOpen(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return false;
    return _windowBuffers[index].windowOpen;
}

float ThermodynamicEngine::getWindowFirstDerivative(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0.0f;
    return _windowBuffers[index].firstDerivative;
}

float ThermodynamicEngine::getWindowSecondDerivative(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0.0f;
    return _windowBuffers[index].secondDerivative;
}

uint32_t ThermodynamicEngine::getWindowPauseRemainingSec(size_t index, uint32_t now) const {
    if (index >= MAX_THERMAL_ZONES || !_windowBuffers[index].windowOpen) return 0;
    uint32_t elapsed = now - _windowBuffers[index].pauseStartMs;
    return (elapsed < 1800000UL) ? (1800000UL - elapsed) / 1000 : 0;
}

bool ThermodynamicEngine::clearWindowPause(size_t index, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return false;
    WindowCircularBuffer& buf = _windowBuffers[index];
    if (!buf.windowOpen) return false;

    buf.lastDetectionDurationSec = (now - buf.pauseStartMs) / 1000;
    buf.windowOpen = false;
    buf.recoveryCount = 0;
    buf.lastSampleMs = now;
    buf.head = 0;
    buf.count = 0;
    buf.firstDerivative = 0.0f;
    buf.secondDerivative = 0.0f;
    return true;
}

void ThermodynamicEngine::setWindowDropThreshold(size_t index, float threshold) {
    if (index >= MAX_THERMAL_ZONES) return;
    if (threshold >= 0.2f && threshold <= 3.0f) {
        _windowBuffers[index].dropThreshold = threshold;
    }
}

float ThermodynamicEngine::getWindowDropThreshold(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 1.2f;
    return _windowBuffers[index].dropThreshold;
}

uint32_t ThermodynamicEngine::getWindowLastDetectionEpoch(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0;
    return _windowBuffers[index].detectionEpoch;
}

uint32_t ThermodynamicEngine::getWindowLastDetectionDurationSec(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return 0;
    return _windowBuffers[index].lastDetectionDurationSec;
}

const WindowCircularBuffer& ThermodynamicEngine::getWindowBuffer(size_t index) const {
    static WindowCircularBuffer dummy;
    if (index >= MAX_THERMAL_ZONES) return dummy;
    return _windowBuffers[index];
}

// ---------------------------------------------------------------------------
// Requirement R1: Passive Night-Time Cooling Calibration (01:00 - 05:00)
// ---------------------------------------------------------------------------

void ThermodynamicEngine::evaluateNightCalibration(size_t index, float currentTemp, float outdoorTemp, bool outdoorValid,
                                                  bool zoneIdle, int hour, int minute, uint32_t now, time_t nowEpoch,
                                                  bool windowOpen) {
    if (index >= MAX_THERMAL_ZONES) return;
    PassiveNightCalibState& state = _nightCalib[index];
    bool windowActive = windowOpen || _windowBuffers[index].windowOpen;

    bool inNightWindow = (hour >= 1 && hour < 5);

    if (inNightWindow && zoneIdle) {
        if (!state.active) {
            // Start passive night tracking session only if outdoor weather is valid and gradient > 3°C and no window open
            if (!windowActive && outdoorValid && (currentTemp > outdoorTemp + 3.0f)) {
                state.active = true;
                state.startMs = now;
                state.startEpoch = nowEpoch;
                state.startTemp = currentTemp;
                state.extTempSum = outdoorTemp;
                state.extTempCount = 1;
                state.lastSampleMs = now;
                state.lastValidOutdoorMs = now;
                state.aerationOccurred = false;
            }
        } else {
            // Session already active
            if (windowActive) {
                state.aerationOccurred = true;
            }
            if (outdoorValid) {
                state.lastValidOutdoorMs = now;
                // Sample outdoor temp every 15 minutes
                if (now - state.lastSampleMs >= 900000UL) {
                    state.extTempSum += outdoorTemp;
                    state.extTempCount++;
                    state.lastSampleMs = now;
                }
            } else {
                // Outdoor weather API dropout: allow 30-minute grace period before aborting
                if (now - state.lastValidOutdoorMs >= 1800000UL) {
                    state.active = false; // Weather API lost for > 30 minutes, abort session
                }
            }
        }
    } else {
        // Outside window or zone fired heat: finalize or abort
        if (state.active) {
            if (hour == 5 && state.extTempCount >= 4 && !state.aerationOccurred && !windowActive) {
                // Completed full night window -> compute k_loss
                float elapsedHours = (float)(nowEpoch - state.startEpoch) / 3600.0f;
                if (elapsedHours >= 2.0f) {
                    float drop = state.startTemp - currentTemp;
                    float avgExt = state.extTempSum / (float)state.extTempCount;
                    float avgRoom = (state.startTemp + currentTemp) * 0.5f;
                    float grad = avgRoom - avgExt;
                    if (grad > 3.0f && drop > 0.05f) {
                        float kMeasured = drop / (elapsedHours * grad);
                        // Sanity check: reasonable residential building range [0.005, 0.080]
                        if (kMeasured >= 0.005f && kMeasured <= 0.080f) {
                            // Recursive EMA filter update (80% prior, 20% measurement)
                            _metrics[index].naturalLossRate = 0.8f * _metrics[index].naturalLossRate + 0.2f * kMeasured;
                        }
                    }
                }
            }
            state.active = false;
            state.aerationOccurred = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Requirement R1: Step Response Test during Away State
// ---------------------------------------------------------------------------

bool ThermodynamicEngine::triggerStepTest(size_t index, float currentTemp, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return false;
    StepResponseTest& t = _stepTests[index];
    if (t.state != STEP_IDLE && t.state != STEP_COMPLETE && t.state != STEP_ABORTED) {
        return false; // Test already running
    }
    t.state = STEP_PULSE_HEATING;
    t.startMs = now;
    t.startTemp = currentTemp;
    t.detectedTdeadMin = 0.0f;
    t.pulseEndTemp = 0.0f;
    t.pulseEndMs = 0;
    t.peakCoastTemp = currentTemp;
    t.peakCoastMs = now;
    t.activeRelayDemand = true;
    return true;
}

void ThermodynamicEngine::abortStepTest(size_t index) {
    if (index >= MAX_THERMAL_ZONES) return;
    _stepTests[index].state = STEP_ABORTED;
    _stepTests[index].activeRelayDemand = false;
}

bool ThermodynamicEngine::isStepTestDemandingHeat(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return false;
    return _stepTests[index].activeRelayDemand;
}

StepTestState ThermodynamicEngine::getStepTestState(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return STEP_IDLE;
    return _stepTests[index].state;
}

void ThermodynamicEngine::updateAwayStepTest(size_t index, float currentTemp, bool isAwayActive, uint32_t now) {
    if (index >= MAX_THERMAL_ZONES) return;
    StepResponseTest& t = _stepTests[index];

    // Safety: If Away state was terminated or room overheats, immediately abort
    if (t.state != STEP_IDLE && (!isAwayActive || currentTemp >= 23.5f || _windowBuffers[index].windowOpen)) {
        abortStepTest(index);
        return;
    }

    switch (t.state) {
        case STEP_IDLE:
            if (isAwayActive && currentTemp >= 16.0f && currentTemp <= 21.5f && !_windowBuffers[index].windowOpen) {
                triggerStepTest(index, currentTemp, now);
            }
            break;

        case STEP_PULSE_HEATING: {
            // Detect dead time inflection: room temp rises by >= 0.08°C
            if (t.detectedTdeadMin <= 0.0f && (currentTemp >= t.startTemp + 0.08f)) {
                t.detectedTdeadMin = (now - t.startMs) / 60000.0f;
                if (t.detectedTdeadMin < 10.0f) t.detectedTdeadMin = 10.0f;
                if (t.detectedTdeadMin > 120.0f) t.detectedTdeadMin = 120.0f;
            }

            // 60-minute heating pulse duration
            if (now - t.startMs >= 3600000UL) {
                t.pulseEndTemp = currentTemp;
                t.pulseEndMs = now;
                t.activeRelayDemand = false; // Turn off valve/burner
                t.peakCoastTemp = currentTemp;
                t.peakCoastMs = now;

                // Calibrate heating slope
                float heatMinutes = 60.0f - (t.detectedTdeadMin > 0.0f ? t.detectedTdeadMin : 30.0f);
                if (heatMinutes > 10.0f) {
                    float rise = t.pulseEndTemp - (t.startTemp + 0.08f);
                    if (rise > 0.05f) {
                        float slopeMeas = (rise / heatMinutes) * 60.0f; // °C/h
                        if (slopeMeas >= 0.20f && slopeMeas <= 3.0f) {
                            _metrics[index].heatingSlope = 0.7f * _metrics[index].heatingSlope + 0.3f * slopeMeas;
                        }
                    }
                }
                if (t.detectedTdeadMin > 0.0f) {
                    _metrics[index].tDeadMin = 0.7f * _metrics[index].tDeadMin + 0.3f * t.detectedTdeadMin;
                }

                t.state = STEP_COASTING;
            }
            break;
        }

        case STEP_COASTING: {
            if (currentTemp > t.peakCoastTemp) {
                t.peakCoastTemp = currentTemp;
                t.peakCoastMs = now;
            }

            // Coasting completes when temp peaks and starts descending by 0.05°C, or 90 min elapsed
            bool peaked = (currentTemp <= t.peakCoastTemp - 0.05f);
            bool coastTimeout = (now - t.pulseEndMs >= 5400000UL); // 90 min

            if (peaked || coastTimeout) {
                float overshootMeas = t.peakCoastTemp - t.pulseEndTemp;
                if (overshootMeas < 0.05f) overshootMeas = 0.05f;
                if (overshootMeas > 1.20f) overshootMeas = 1.20f;
                _metrics[index].overshootDelta = 0.7f * _metrics[index].overshootDelta + 0.3f * overshootMeas;

                t.state = STEP_COMPLETE;
            }
            break;
        }

        case STEP_COMPLETE:
        case STEP_ABORTED:
            t.activeRelayDemand = false;
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Dynamic Predictive Pre-Heat Timing Engine
// ---------------------------------------------------------------------------

uint32_t ThermodynamicEngine::calculatePreheatLeadMin(size_t index, float currentTemp, float targetTemp,
                                                     float outdoorTemp, bool outdoorValid) {
    if (index >= MAX_THERMAL_ZONES) return 45;
    if (targetTemp <= currentTemp + 0.1f) return 0;

    const CalibratedMetrics& m = _metrics[index];

    float deltaTemp = targetTemp - currentTemp;

    // Scale effective overshoot proportionally so active heating is never suppressed
    // Physical thermal inertia: coasting provides at most ~45% of total temperature rise
    // when starting from cold/idle steady-state.
    float effectiveOvershoot = m.overshootDelta;
    if (effectiveOvershoot > deltaTemp * 0.45f) {
        effectiveOvershoot = deltaTemp * 0.45f;
    }

    float deltaNeeded = deltaTemp - effectiveOvershoot;
    if (deltaNeeded < 0.05f) deltaNeeded = 0.05f;

    // Net heating rate = dynamic heating slope minus outdoor heat loss rate
    float deltaExt = outdoorValid ? (currentTemp - outdoorTemp) : 12.0f;
    if (deltaExt < 0.0f) deltaExt = 0.0f;
    float lossRate = m.naturalLossRate * deltaExt; // °C/h
    float netRate = m.heatingSlope - lossRate;
    if (netRate < 0.20f) netRate = 0.20f; // Minimum net rate clamp

    float heatHours = deltaNeeded / netRate;
    float totalMinutes = m.tDeadMin + (heatHours * 60.0f);

    if (totalMinutes > 240.0f) totalMinutes = 240.0f; // Clamp to 4 hours max
    if (totalMinutes < 0.0f) totalMinutes = 0.0f;

    _metrics[index].preheatLeadMin = (uint32_t)(totalMinutes + 0.5f);
    return _metrics[index].preheatLeadMin;
}

// ---------------------------------------------------------------------------
// NVS Persistence with Flash Wear Protection (> +-2% delta)
// ---------------------------------------------------------------------------

bool ThermodynamicEngine::shouldPersistDelta(float oldVal, float newVal) {
    if (fabsf(oldVal) < 0.0001f && fabsf(newVal) < 0.0001f) return false;
    float denom = (fabsf(oldVal) > 0.001f) ? fabsf(oldVal) : 1.0f;
    float deltaPct = fabsf(newVal - oldVal) / denom;
    return deltaPct > 0.02f; // strictly > 2%
}

void ThermodynamicEngine::loadFromPreferences(Preferences& prefs) {
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String p = "z" + String(i) + "_";
        uint8_t ft = prefs.getUChar((p + "fl_type").c_str(), (uint8_t)_roomConfigs[i].floorType);
        _roomConfigs[i].floorType = (ft == 1) ? FLOOR_PARQUET : FLOOR_TILES;
        _roomConfigs[i].screedThicknessCm = prefs.getFloat((p + "screed").c_str(), _roomConfigs[i].screedThicknessCm);
        uint8_t ins = prefs.getUChar((p + "insul").c_str(), (uint8_t)_roomConfigs[i].insulation);
        _roomConfigs[i].insulation = (ins <= 3) ? (InsulationLevel)ins : INSULATION_MEDIUM;

        // Load calibrated metrics if present
        _metrics[i].tDeadMin = prefs.getFloat((p + "tdead").c_str(), _metrics[i].tDeadMin);
        _metrics[i].heatingSlope = prefs.getFloat((p + "hslope").c_str(), _metrics[i].heatingSlope);
        _metrics[i].naturalLossRate = prefs.getFloat((p + "kloss").c_str(), _metrics[i].naturalLossRate);
        _metrics[i].overshootDelta = prefs.getFloat((p + "ovshot").c_str(), _metrics[i].overshootDelta);
        _metrics[i].preheatLeadMin = prefs.getUInt((p + "lead").c_str(), _metrics[i].preheatLeadMin);

        _persistedMetrics[i] = _metrics[i];
    }
}

void ThermodynamicEngine::saveToPreferences(Preferences& prefs) {
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String p = "z" + String(i) + "_";
        prefs.putUChar((p + "fl_type").c_str(), (uint8_t)_roomConfigs[i].floorType);
        prefs.putFloat((p + "screed").c_str(), _roomConfigs[i].screedThicknessCm);
        prefs.putUChar((p + "insul").c_str(), (uint8_t)_roomConfigs[i].insulation);

        // Flash wear protection: only save metrics if delta > +-2%
        if (shouldPersistDelta(_persistedMetrics[i].tDeadMin, _metrics[i].tDeadMin)) {
            prefs.putFloat((p + "tdead").c_str(), _metrics[i].tDeadMin);
            _persistedMetrics[i].tDeadMin = _metrics[i].tDeadMin;
        }
        if (shouldPersistDelta(_persistedMetrics[i].heatingSlope, _metrics[i].heatingSlope)) {
            prefs.putFloat((p + "hslope").c_str(), _metrics[i].heatingSlope);
            _persistedMetrics[i].heatingSlope = _metrics[i].heatingSlope;
        }
        if (shouldPersistDelta(_persistedMetrics[i].naturalLossRate, _metrics[i].naturalLossRate)) {
            prefs.putFloat((p + "kloss").c_str(), _metrics[i].naturalLossRate);
            _persistedMetrics[i].naturalLossRate = _metrics[i].naturalLossRate;
        }
        if (shouldPersistDelta(_persistedMetrics[i].overshootDelta, _metrics[i].overshootDelta)) {
            prefs.putFloat((p + "ovshot").c_str(), _metrics[i].overshootDelta);
            _persistedMetrics[i].overshootDelta = _metrics[i].overshootDelta;
        }
        if (_persistedMetrics[i].preheatLeadMin != _metrics[i].preheatLeadMin) {
            if (_persistedMetrics[i].preheatLeadMin != _metrics[i].preheatLeadMin) {
            prefs.putUInt((p + "lead").c_str(), _metrics[i].preheatLeadMin);
            _persistedMetrics[i].preheatLeadMin = _metrics[i].preheatLeadMin;
        }
            _persistedMetrics[i].preheatLeadMin = _metrics[i].preheatLeadMin;
        }
    }
}

// ---------------------------------------------------------------------------
// String and Enum Converters
// ---------------------------------------------------------------------------

const char* ThermodynamicEngine::floorTypeToStr(FloorType t) {
    switch (t) {
        case FLOOR_TILES:   return "tiles";
        case FLOOR_PARQUET: return "parquet";
        default:            return "tiles";
    }
}

FloorType ThermodynamicEngine::parseFloorType(const char* str) {
    if (!str) return FLOOR_TILES;
    if (strcasecmp(str, "parquet") == 0 || strcasecmp(str, "wood") == 0 || strcasecmp(str, "parchet") == 0) {
        return FLOOR_PARQUET;
    }
    return FLOOR_TILES;
}

const char* ThermodynamicEngine::insulationLevelToStr(InsulationLevel l) {
    switch (l) {
        case INSULATION_POOR:    return "poor";
        case INSULATION_MEDIUM:  return "medium";
        case INSULATION_GOOD:    return "good";
        case INSULATION_PASSIVE: return "passive";
        default:                 return "medium";
    }
}

InsulationLevel ThermodynamicEngine::parseInsulationLevel(const char* str) {
    if (!str) return INSULATION_MEDIUM;
    if (strcasecmp(str, "poor") == 0 || strcasecmp(str, "slaba") == 0 || strcasecmp(str, "slabă") == 0) {
        return INSULATION_POOR;
    }
    if (strcasecmp(str, "good") == 0 || strcasecmp(str, "buna") == 0 || strcasecmp(str, "bună") == 0) {
        return INSULATION_GOOD;
    }
    if (strcasecmp(str, "passive") == 0 || strcasecmp(str, "pasiva") == 0 || strcasecmp(str, "excelenta") == 0) {
        return INSULATION_PASSIVE;
    }
    return INSULATION_MEDIUM;
}
