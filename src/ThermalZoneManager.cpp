#include "ThermalZoneManager.h"
#include "NetworkConsole.h"

ThermalZoneManager thermalZones;

ThermalZoneManager::ThermalZoneManager()
    : _autoMode(true),
      _boilerDemand(false),
      _interlockDelayMs(DEFAULT_ACTUATOR_DELAY_MS),
      _pumpOverrunMs(DEFAULT_PUMP_OVERRUN_MS),
      _minBoilerOffMs(DEFAULT_MIN_BOILER_OFF_MS),
      _lastBoilerOffTime(0),
      _lastBoilerOnTime(0),
      _lastEvaluationMs(0) {

    // Configurare Zona 1: Living + Bucătărie
    _configs[0].id = 1;
    strncpy(_configs[0].name, "Living & Bucatarie", sizeof(_configs[0].name) - 1);
    strncpy(_configs[0].sensorName, "Living Room", sizeof(_configs[0].sensorName) - 1);
    _configs[0].relayPin = PIN_RELAY_ZONE_1;
    _configs[0].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[0].hysteresis = DEFAULT_HYSTERESIS;
    _configs[0].enabled = true;

    // Configurare Zona 2: Camera Copilului
    _configs[1].id = 2;
    strncpy(_configs[1].name, "Camera Copilului", sizeof(_configs[1].name) - 1);
    strncpy(_configs[1].sensorName, "Kids Room", sizeof(_configs[1].sensorName) - 1);
    _configs[1].relayPin = PIN_RELAY_ZONE_2;
    _configs[1].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[1].hysteresis = DEFAULT_HYSTERESIS;
    _configs[1].enabled = true;

    // Configurare Zona 3: Dormitor Matrimonial
    _configs[2].id = 3;
    strncpy(_configs[2].name, "Dormitor Matrimonial", sizeof(_configs[2].name) - 1);
    strncpy(_configs[2].sensorName, "Matrimonial", sizeof(_configs[2].sensorName) - 1);
    _configs[2].relayPin = PIN_RELAY_ZONE_3;
    _configs[2].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[2].hysteresis = DEFAULT_HYSTERESIS;
    _configs[2].enabled = true;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        _statuses[i].state = ZONE_IDLE;
        _statuses[i].currentTemp = 0.0f;
        _statuses[i].sensorValid = false;
        _statuses[i].demand = false;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = 0;
    }
}

void ThermalZoneManager::applyRelay(uint8_t pin, bool active) {
    digitalWrite(pin, active ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
}

void ThermalZoneManager::begin() {
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        pinMode(_configs[i].relayPin, OUTPUT);
        applyRelay(_configs[i].relayPin, false);
        _statuses[i].state = _configs[i].enabled ? ZONE_IDLE : ZONE_DISABLED;
        _statuses[i].relayEngaged = false;
        _statuses[i].stateChangedMs = millis();
    }

    pinMode(PIN_RELAY_SPARE, OUTPUT);
    applyRelay(PIN_RELAY_SPARE, false);

    netConsole.println(F("[Thermal] ThermalZoneManager initialized: 3 zones active, all relays OFF."));
}

const ThermalZoneConfig& ThermalZoneManager::getConfig(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return _configs[0];
    return _configs[index];
}

const ThermalZoneStatus& ThermalZoneManager::getStatus(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return _statuses[0];
    return _statuses[index];
}

void ThermalZoneManager::setTargetTemperature(size_t index, float target) {
    if (index < MAX_THERMAL_ZONES) {
        if (target < 15.0f) target = 15.0f;
        if (target > 30.0f) target = 30.0f;
        _configs[index].targetTemp = target;
        netConsole.printf("[Thermal] Zone %u (%s) new target temp: %.1f C\n",
            _configs[index].id, _configs[index].name, target);
    }
}

void ThermalZoneManager::setHysteresis(size_t index, float hyst) {
    if (index < MAX_THERMAL_ZONES) {
        if (hyst < 0.1f) hyst = 0.1f;
        if (hyst > 2.0f) hyst = 2.0f;
        _configs[index].hysteresis = hyst;
    }
}

void ThermalZoneManager::setZoneEnabled(size_t index, bool enabled) {
    if (index < MAX_THERMAL_ZONES) {
        _configs[index].enabled = enabled;
        if (!enabled) {
            _statuses[index].demand = false;
            _statuses[index].state = ZONE_DISABLED;
            if (_statuses[index].relayEngaged) {
                applyRelay(_configs[index].relayPin, false);
                _statuses[index].relayEngaged = false;
            }
        }
        netConsole.printf("[Thermal] Zone %u (%s) %s\n",
            _configs[index].id, _configs[index].name, enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setSensorName(size_t index, const char* sensorName) {
    if (index < MAX_THERMAL_ZONES && sensorName) {
        strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        _configs[index].sensorName[sizeof(_configs[index].sensorName) - 1] = '\0';
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorName);
    }
}

void ThermalZoneManager::setAutoMode(bool autoMode) {
    _autoMode = autoMode;
    netConsole.printf("[Thermal] Operation mode switched to: %s\n", _autoMode ? "AUTO" : "MANUAL");
}

void ThermalZoneManager::setRelayManual(size_t index, bool on) {
    if (index < MAX_THERMAL_ZONES && !_autoMode) {
        _statuses[index].relayEngaged = on;
        applyRelay(_configs[index].relayPin, on);
        _statuses[index].state = on ? ZONE_HEATING : ZONE_IDLE;
        netConsole.printf("[Thermal][MANUAL] Relay Zone %u switched to %s\n", _configs[index].id, on ? "ON" : "OFF");
    }
}

void ThermalZoneManager::setBoilerManual(BoilerRadioTrigger& boiler, bool on) {
    if (!_autoMode) {
        if (on) {
            boiler.sendBoilerOn();
            _lastBoilerOnTime = millis();
        } else {
            boiler.sendBoilerOff();
            _lastBoilerOffTime = millis();
        }
    }
}

const char* ThermalZoneManager::getStateStr(ZoneState state) const {
    switch (state) {
        case ZONE_DISABLED:   return "Disabled";
        case ZONE_IDLE:       return "Idle (Temp OK)";
        case ZONE_WARMING_UP: return "Actuator Warming Up";
        case ZONE_HEATING:    return "Heating Active";
        default:              return "Unknown";
    }
}

void ThermalZoneManager::update(MqttTelemetryClient& telemetry, BoilerRadioTrigger& boiler) {
    uint32_t now = millis();
    if (now - _lastEvaluationMs < 1000) {
        return;
    }
    _lastEvaluationMs = now;

    if (!_autoMode) {
        return; // In manual mode, automatic decisions are suspended
    }

    // 1. Evaluate heat demand per zone
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (!_configs[i].enabled) {
            _statuses[i].state = ZONE_DISABLED;
            _statuses[i].demand = false;
            if (_statuses[i].relayEngaged) {
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
            }
            continue;
        }

        const RoomSensor* s = telemetry.findSensorByName(_configs[i].sensorName);
        if (s && s->valid) {
            _statuses[i].sensorValid = true;
            _statuses[i].currentTemp = s->temperature;

            // Hysteresis control loop
            if (!_statuses[i].demand) {
                if (_statuses[i].currentTemp <= (_configs[i].targetTemp - _configs[i].hysteresis)) {
                    _statuses[i].demand = true;
                    netConsole.printf("[Thermal] Zone %u (%s) heat demand: %.1f C <= %.1f C (target: %.1f C)\n",
                        _configs[i].id, _configs[i].name,
                        _statuses[i].currentTemp,
                        _configs[i].targetTemp - _configs[i].hysteresis,
                        _configs[i].targetTemp);
                }
            } else {
                if (_statuses[i].currentTemp >= (_configs[i].targetTemp + _configs[i].hysteresis)) {
                    _statuses[i].demand = false;
                    netConsole.printf("[Thermal] Zone %u (%s) target reached: %.1f C >= %.1f C (target: %.1f C)\n",
                        _configs[i].id, _configs[i].name,
                        _statuses[i].currentTemp,
                        _configs[i].targetTemp + _configs[i].hysteresis,
                        _configs[i].targetTemp);
                }
            }
        } else {
            _statuses[i].sensorValid = false;
            // No valid sensor reading; keep failsafe state
        }
    }

    // 2. Evaluate thermal actuators and relay switching
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (!_configs[i].enabled) continue;

        if (_statuses[i].demand) {
            if (!_statuses[i].relayEngaged) {
                applyRelay(_configs[i].relayPin, true);
                _statuses[i].relayEngaged = true;
                _statuses[i].state = ZONE_WARMING_UP;
                _statuses[i].stateChangedMs = now;
                netConsole.printf("[Thermal] Opening actuator for Zone %u (%s)... Waiting %u ms interlock.\n",
                    _configs[i].id, _configs[i].name, _interlockDelayMs);
            } else {
                if (_statuses[i].state == ZONE_WARMING_UP && (now - _statuses[i].stateChangedMs >= _interlockDelayMs)) {
                    if (boiler.isLastCommandOn()) {
                        _statuses[i].state = ZONE_HEATING;
                    }
                } else if (boiler.isLastCommandOn()) {
                    _statuses[i].state = ZONE_HEATING;
                }
            }
        } else {
            // Demand satisfied for this zone
            if (_statuses[i].relayEngaged) {
                // Verify boiler is off and pump overrun has elapsed
                bool boilerRunning = boiler.isLastCommandOn();
                if (!boilerRunning && (now - _lastBoilerOffTime >= _pumpOverrunMs)) {
                    applyRelay(_configs[i].relayPin, false);
                    _statuses[i].relayEngaged = false;
                    _statuses[i].state = ZONE_IDLE;
                    _statuses[i].stateChangedMs = now;
                    netConsole.printf("[Thermal] Closing actuator for Zone %u (%s) after pump overrun.\n",
                        _configs[i].id, _configs[i].name);
                } else {
                    _statuses[i].state = ZONE_IDLE;
                }
            } else {
                _statuses[i].state = ZONE_IDLE;
            }
        }
    }

    // 3. Evaluate boiler firing command
    bool anyDemand = false;
    bool anyActuatorReady = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled && _statuses[i].demand) {
            anyDemand = true;
            if (_statuses[i].relayEngaged && (now - _statuses[i].stateChangedMs >= _interlockDelayMs)) {
                anyActuatorReady = true;
            }
        }
    }

    _boilerDemand = (anyDemand && anyActuatorReady);

    if (_boilerDemand) {
        if (!boiler.isLastCommandOn()) {
            // Anti-short-cycling guard
            if (now - _lastBoilerOffTime >= _minBoilerOffMs || _lastBoilerOffTime == 0) {
                netConsole.println(F("[Thermal] Firing boiler (actuator interlock complete, circuits open)"));
                boiler.sendBoilerOn();
                _lastBoilerOnTime = now;
                for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                    if (_configs[i].enabled && _statuses[i].demand && _statuses[i].relayEngaged) {
                        _statuses[i].state = ZONE_HEATING;
                    }
                }
            } else {
                netConsole.printf("[Thermal] Anti-short-cycling delay active (%u s remaining)...\n",
                    (_minBoilerOffMs - (now - _lastBoilerOffTime)) / 1000);
            }
        }
    } else if (!anyDemand) {
        if (boiler.isLastCommandOn()) {
            netConsole.println(F("[Thermal] Stopping boiler (no thermal demand remaining)"));
            boiler.sendBoilerOff();
            _lastBoilerOffTime = now;
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                if (_statuses[i].state == ZONE_HEATING) {
                    _statuses[i].state = ZONE_IDLE;
                }
            }
        }
    }
}
