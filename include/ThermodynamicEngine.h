#ifndef THERMODYNAMIC_ENGINE_H
#define THERMODYNAMIC_ENGINE_H

#include <Arduino.h>
#include <Preferences.h>
#include <cmath>
#include "Config.h"

#ifndef MAX_THERMAL_ZONES
#define MAX_THERMAL_ZONES 3
#endif

// Physical Floor Covering Type
enum FloorType : uint8_t {
    FLOOR_TILES = 0,
    FLOOR_PARQUET = 1
};

// Envelope Insulation Level
enum InsulationLevel : uint8_t {
    INSULATION_POOR = 0,
    INSULATION_MEDIUM = 1,
    INSULATION_GOOD = 2,
    INSULATION_PASSIVE = 3
};

// Room physical configuration priors
struct RoomCalibrationConfig {
    FloorType floorType;          // Tiles vs Parquet
    float screedThicknessCm;      // 4.0 - 8.0 cm (default 5.0)
    InsulationLevel insulation;   // Poor, Medium, Good, Passive
};

// Dynamic 2R2C calibrated thermodynamic metrics [SWE-REQ-014]
struct CalibratedMetrics {
    float tDeadMin;               // Dynamic dead time (minutes) [SWE-REQ-016]
    float heatingSlope;           // Rising rate (°C/h) [SWE-REQ-016]
    float naturalLossRate;        // Natural cooling rate k_loss (°C/(h*K)) [SWE-REQ-015]
    float overshootDelta;         // Expected overshoot (°C) [SWE-REQ-016]
    uint32_t preheatLeadMin;      // Dynamic calculated preheat lead time (minutes) [SWE-REQ-017]
};

// Rolling 12-sample circular buffer for derivative window detection [SWE-REQ-018]
#define WINDOW_BUFFER_SIZE 12
#define WINDOW_SAMPLE_INTERVAL_MS 300000UL // 5 minutes

struct WindowCircularBuffer {
    float samples[WINDOW_BUFFER_SIZE];
    uint32_t sampleTimestamps[WINDOW_BUFFER_SIZE];
    uint8_t head;
    uint8_t count;
    float firstDerivative;        // dT/dt (°C/min) [SWE-REQ-019]
    float secondDerivative;       // d²T/dt² (°C/min²) [SWE-REQ-019]
    bool windowOpen;              // Rapid derivative open-window state [SWE-REQ-020]
    uint32_t detectionEpoch;
    uint32_t pauseStartMs;
    float pauseMinTemp;
    uint8_t recoveryCount;        // Consecutive samples with dT/dt >= 0 [SWE-REQ-021]
    uint32_t lastDetectionDurationSec;
    float dropThreshold;          // Backward compatible threshold (default 1.2°C)
    uint32_t lastSampleMs;
    float lastSampleTemp;
};

// Step response test states during Away mode
enum StepTestState : uint8_t {
    STEP_IDLE = 0,
    STEP_STABILIZING,
    STEP_PULSE_HEATING,
    STEP_COASTING,
    STEP_COMPLETE,
    STEP_ABORTED
};

struct StepResponseTest {
    StepTestState state;
    uint32_t startMs;
    float startTemp;
    float detectedTdeadMin;
    float pulseEndTemp;
    uint32_t pulseEndMs;
    float peakCoastTemp;
    uint32_t peakCoastMs;
    bool activeRelayDemand;
};

// Quiescent night-time cooling calibration (01:00 - 05:00)
struct PassiveNightCalibState {
    bool active;
    uint32_t startMs;
    time_t startEpoch;
    float startTemp;
    float extTempSum;
    uint16_t extTempCount;
    uint32_t lastSampleMs;
    uint32_t lastValidOutdoorMs;
    bool aerationOccurred;
};

class ThermodynamicEngine {
public:
    ThermodynamicEngine();

    void begin();

    // Configuration
    void setRoomConfig(size_t index, const RoomCalibrationConfig& config);
    const RoomCalibrationConfig& getRoomConfig(size_t index) const;
    bool setRoomConfig(size_t index, const char* floorStr, float screedCm, const char* insulationStr);

    // Calibrated Metrics
    const CalibratedMetrics& getMetrics(size_t index) const;
    void setMetrics(size_t index, const CalibratedMetrics& metrics);

    // Rapid Derivative Window Detection (Requirement R2) [SWE-REQ-018, SWE-REQ-019, SWE-REQ-020]
    void evaluateWindow(size_t index, float currentTemp, float outdoorTemp, bool outdoorValid, uint32_t now);
    bool isWindowOpen(size_t index) const;
    float getWindowFirstDerivative(size_t index) const;
    float getWindowSecondDerivative(size_t index) const;
    uint32_t getWindowPauseRemainingSec(size_t index, uint32_t now) const;
    bool clearWindowPause(size_t index, uint32_t now); // [SWE-REQ-021]
    void setWindowDropThreshold(size_t index, float threshold);
    float getWindowDropThreshold(size_t index) const;
    uint32_t getWindowLastDetectionEpoch(size_t index) const;
    uint32_t getWindowLastDetectionDurationSec(size_t index) const;
    const WindowCircularBuffer& getWindowBuffer(size_t index) const;

    // Passive Night Cooling Calibration (Requirement R1, 01:00 - 05:00) [SWE-REQ-015]
    void evaluateNightCalibration(size_t index, float currentTemp, float outdoorTemp, bool outdoorValid,
                                  bool zoneIdle, int hour, int minute, uint32_t now, time_t nowEpoch,
                                  bool windowOpen = false);

    // Step Response Test during Away State (Requirement R1) [SWE-REQ-016]
    void updateAwayStepTest(size_t index, float currentTemp, bool isAwayActive, uint32_t now);
    bool isStepTestDemandingHeat(size_t index) const;
    StepTestState getStepTestState(size_t index) const;
    void abortStepTest(size_t index);
    bool triggerStepTest(size_t index, float currentTemp, uint32_t now);

    // Dynamic Predictive Pre-Heat Timing Engine (Requirement R1) [SWE-REQ-017]
    uint32_t calculatePreheatLeadMin(size_t index, float currentTemp, float targetTemp,
                                     float outdoorTemp, bool outdoorValid);

    // Physics Priors Engine [SWE-REQ-014]
    static CalibratedMetrics calculateDefaultPriors(const RoomCalibrationConfig& config);

    // String / Enum conversions
    static const char* floorTypeToStr(FloorType t);
    static FloorType parseFloorType(const char* str);
    static const char* insulationLevelToStr(InsulationLevel l);
    static InsulationLevel parseInsulationLevel(const char* str);

    // NVS Persistence with Flash Wear Protection (> +-2% delta)
    void loadFromPreferences(Preferences& prefs);
    void saveToPreferences(Preferences& prefs);

    // Reset zone calibration to defaults
    void resetZoneToPriors(size_t index);

private:
    RoomCalibrationConfig _roomConfigs[MAX_THERMAL_ZONES];
    CalibratedMetrics _metrics[MAX_THERMAL_ZONES];
    CalibratedMetrics _persistedMetrics[MAX_THERMAL_ZONES];
    WindowCircularBuffer _windowBuffers[MAX_THERMAL_ZONES];
    StepResponseTest _stepTests[MAX_THERMAL_ZONES];
    PassiveNightCalibState _nightCalib[MAX_THERMAL_ZONES];

    static bool shouldPersistDelta(float oldVal, float newVal);
};

#endif // THERMODYNAMIC_ENGINE_H
