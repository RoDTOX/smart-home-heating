#ifndef THERMAL_ZONE_MANAGER_H
#define THERMAL_ZONE_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"
#include "MqttTelemetryClient.h"
#include "BoilerRadioTrigger.h"

#define MAX_THERMAL_ZONES 3

enum ZoneState {
    ZONE_DISABLED = 0,
    ZONE_IDLE,          // Temperatura atinsa, robinet oprit
    ZONE_WARMING_UP,    // Deschidere robinet NC (asteptare interlock actuator)
    ZONE_HEATING        // Robinet deschis & flacara centralei activa
};

struct ThermalZoneConfig {
    uint8_t id;
    char name[32];
    char sensorMac[18];   // Direct MAC mapping (e.g. "A4:C1:38:A4:C1:DD")
    char sensorName[32];  // Friendly name or alias
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

    // Persistence
    void loadFromPreferences();
    void saveToPreferences();

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
    void setZoneName(size_t index, const char* name);
    void setSensorName(size_t index, const char* sensorName);
    void setSensorMac(size_t index, const char* sensorMac);
    void setRelayPin(size_t index, uint8_t pin);
    void mapZone(size_t index, const char* name, const char* sensorMac, const char* sensorName, uint8_t relayPin, float targetTemp, float hysteresis, bool enabled);
    void setAutoMode(bool autoMode);
    void setInterlockDelayMs(uint32_t ms) { _interlockDelayMs = ms; }
    void setPumpOverrunMs(uint32_t ms) { _pumpOverrunMs = ms; }
    void setMinBoilerOffMs(uint32_t ms) { _minBoilerOffMs = ms; }

    // Control Manual (cand autoMode == false)
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
