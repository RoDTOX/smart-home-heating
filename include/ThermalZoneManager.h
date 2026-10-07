#ifndef THERMAL_ZONE_MANAGER_H
#define THERMAL_ZONE_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "MqttTelemetryClient.h"
#include "BoilerRadioTrigger.h"

#define MAX_THERMAL_ZONES 3

enum ZoneState {
    ZONE_DISABLED = 0,
    ZONE_IDLE,          // Temperatura atinsă, robinet oprit
    ZONE_WARMING_UP,    // Deschidere robinet NC (așteptare interlock actuator)
    ZONE_HEATING        // Robinet deschis & flacăra centralei activă
};

struct ThermalZoneConfig {
    uint8_t id;
    char name[32];
    char sensorName[32];
    uint8_t relayPin;
    float targetTemp;
    float hysteresis;
    bool enabled;
};

struct ThermalZoneStatus {
    ZoneState state;
    float currentTemp;
    bool sensorValid;
    bool demand;
    bool relayEngaged;
    uint32_t stateChangedMs;
};

class ThermalZoneManager {
public:
    ThermalZoneManager();

    void begin();
    void update(MqttTelemetryClient& telemetry, BoilerRadioTrigger& boiler);

    // Getters
    size_t getZoneCount() const { return MAX_THERMAL_ZONES; }
    const ThermalZoneConfig& getConfig(size_t index) const;
    const ThermalZoneStatus& getStatus(size_t index) const;
    bool isBoilerDemanded() const { return _boilerDemand; }
    bool isAutoMode() const { return _autoMode; }
    uint32_t getInterlockDelayMs() const { return _interlockDelayMs; }
    uint32_t getPumpOverrunMs() const { return _pumpOverrunMs; }
    uint32_t getMinBoilerOffMs() const { return _minBoilerOffMs; }

    // Setters
    void setTargetTemperature(size_t index, float target);
    void setHysteresis(size_t index, float hyst);
    void setZoneEnabled(size_t index, bool enabled);
    void setSensorName(size_t index, const char* sensorName);
    void setAutoMode(bool autoMode);
    void setInterlockDelayMs(uint32_t ms) { _interlockDelayMs = ms; }
    void setPumpOverrunMs(uint32_t ms) { _pumpOverrunMs = ms; }
    void setMinBoilerOffMs(uint32_t ms) { _minBoilerOffMs = ms; }

    // Control Manual (când autoMode == false)
    void setRelayManual(size_t index, bool on);
    void setBoilerManual(BoilerRadioTrigger& boiler, bool on);

    // Helpers
    const char* getStateStr(ZoneState state) const;

private:
    ThermalZoneConfig _configs[MAX_THERMAL_ZONES];
    ThermalZoneStatus _statuses[MAX_THERMAL_ZONES];

    bool _autoMode;
    bool _boilerDemand;
    uint32_t _interlockDelayMs;
    uint32_t _pumpOverrunMs;
    uint32_t _minBoilerOffMs;
    uint32_t _lastBoilerOffTime;
    uint32_t _lastBoilerOnTime;
    uint32_t _lastEvaluationMs;

    void applyRelay(uint8_t pin, bool active);
};

extern ThermalZoneManager thermalZones;

#endif // THERMAL_ZONE_MANAGER_H
