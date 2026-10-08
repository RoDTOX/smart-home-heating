#include "ThermalZoneManager.h"
#include "NetworkConsole.h"
#include "BleSensorScanner.h"

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

    // Default Zone 1
    _configs[0].id = 1;
    strncpy(_configs[0].name, "Living & Bucatarie", sizeof(_configs[0].name) - 1);
    strncpy(_configs[0].sensorMac, "A4:C1:38:A4:C1:DD", sizeof(_configs[0].sensorMac) - 1);
    strncpy(_configs[0].sensorName, "Living Room", sizeof(_configs[0].sensorName) - 1);
    _configs[0].relayPin = PIN_RELAY_ZONE_1;
    _configs[0].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[0].hysteresis = DEFAULT_HYSTERESIS;
    _configs[0].enabled = true;

    // Default Zone 2
    _configs[1].id = 2;
    strncpy(_configs[1].name, "Camera Copilului", sizeof(_configs[1].name) - 1);
    strncpy(_configs[1].sensorMac, "A4:C1:38:84:A1:E8", sizeof(_configs[1].sensorMac) - 1);
    strncpy(_configs[1].sensorName, "Kids Room", sizeof(_configs[1].sensorName) - 1);
    _configs[1].relayPin = PIN_RELAY_ZONE_2;
    _configs[1].targetTemp = DEFAULT_SETPOINT_TEMP;
    _configs[1].hysteresis = DEFAULT_HYSTERESIS;
    _configs[1].enabled = true;

    // Default Zone 3
    _configs[2].id = 3;
    strncpy(_configs[2].name, "Dormitor Matrimonial", sizeof(_configs[2].name) - 1);
    strncpy(_configs[2].sensorMac, "", sizeof(_configs[2].sensorMac) - 1);
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

void ThermalZoneManager::loadFromPreferences() {
    Preferences prefs;
    if (!prefs.begin("zones", true)) {
        return; // Use constructor defaults
    }

    if (!prefs.isKey("z_init")) {
        prefs.end();
        saveToPreferences(); // Save defaults
        return;
    }

    _autoMode = prefs.getBool("autoMode", true);

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String p = "z" + String(i) + "_";
        prefs.getString((p + "name").c_str(), _configs[i].name, sizeof(_configs[i].name));
        prefs.getString((p + "mac").c_str(), _configs[i].sensorMac, sizeof(_configs[i].sensorMac));
        prefs.getString((p + "sname").c_str(), _configs[i].sensorName, sizeof(_configs[i].sensorName));
        _configs[i].relayPin = prefs.getUChar((p + "pin").c_str(), _configs[i].relayPin);
        _configs[i].targetTemp = prefs.getFloat((p + "target").c_str(), _configs[i].targetTemp);
        _configs[i].hysteresis = prefs.getFloat((p + "hyst").c_str(), _configs[i].hysteresis);
        _configs[i].enabled = prefs.getBool((p + "en").c_str(), _configs[i].enabled);
    }
    prefs.end();
    netConsole.println(F("[Thermal] Zone configs and sensor mappings loaded from NVS."));
}

void ThermalZoneManager::saveToPreferences() {
    Preferences prefs;
    if (!prefs.begin("zones", false)) {
        netConsole.println(F("[Thermal] ERROR: Failed to open NVS namespace 'zones' for writing!"));
        return;
    }

    prefs.putBool("z_init", true);
    prefs.putBool("autoMode", _autoMode);

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        String p = "z" + String(i) + "_";
        prefs.putString((p + "name").c_str(), _configs[i].name);
        prefs.putString((p + "mac").c_str(), _configs[i].sensorMac);
        prefs.putString((p + "sname").c_str(), _configs[i].sensorName);
        prefs.putUChar((p + "pin").c_str(), _configs[i].relayPin);
        prefs.putFloat((p + "target").c_str(), _configs[i].targetTemp);
        prefs.putFloat((p + "hyst").c_str(), _configs[i].hysteresis);
        prefs.putBool((p + "en").c_str(), _configs[i].enabled);
    }
    prefs.end();
    netConsole.println(F("[Thermal] Zone configs and sensor mappings successfully persisted to NVS."));
}

void ThermalZoneManager::applyRelay(uint8_t pin, bool active) {
    digitalWrite(pin, active ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
}

void ThermalZoneManager::begin() {
    loadFromPreferences();

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
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) new target temp: %.1f C\n",
            _configs[index].id, _configs[index].name, target);
    }
}

void ThermalZoneManager::setHysteresis(size_t index, float hyst) {
    if (index < MAX_THERMAL_ZONES) {
        if (hyst < 0.1f) hyst = 0.1f;
        if (hyst > 2.0f) hyst = 2.0f;
        _configs[index].hysteresis = hyst;
        saveToPreferences();
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
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) %s\n",
            _configs[index].id, _configs[index].name, enabled ? "ENABLED" : "DISABLED");
    }
}

void ThermalZoneManager::setZoneName(size_t index, const char* name) {
    if (index < MAX_THERMAL_ZONES && name) {
        strncpy(_configs[index].name, name, sizeof(_configs[index].name) - 1);
        _configs[index].name[sizeof(_configs[index].name) - 1] = '\0';
        saveToPreferences();
    }
}

void ThermalZoneManager::setSensorName(size_t index, const char* sensorName) {
    if (index < MAX_THERMAL_ZONES && sensorName) {
        strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        _configs[index].sensorName[sizeof(_configs[index].sensorName) - 1] = '\0';
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor name '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorName);
    }
}

void ThermalZoneManager::setSensorMac(size_t index, const char* sensorMac) {
    if (index < MAX_THERMAL_ZONES && sensorMac) {
        strncpy(_configs[index].sensorMac, sensorMac, sizeof(_configs[index].sensorMac) - 1);
        _configs[index].sensorMac[sizeof(_configs[index].sensorMac) - 1] = '\0';
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u (%s) assigned to sensor MAC '%s'\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac);
    }
}

void ThermalZoneManager::setRelayPin(size_t index, uint8_t pin) {
    if (index < MAX_THERMAL_ZONES) {
        _configs[index].relayPin = pin;
        pinMode(pin, OUTPUT);
        applyRelay(pin, false);
        saveToPreferences();
    }
}

void ThermalZoneManager::mapZone(size_t index, const char* name, const char* sensorMac, const char* sensorName, uint8_t relayPin, float targetTemp, float hysteresis, bool enabled) {
    if (index < MAX_THERMAL_ZONES) {
        if (name && strlen(name) > 0) strncpy(_configs[index].name, name, sizeof(_configs[index].name) - 1);
        if (sensorMac) strncpy(_configs[index].sensorMac, sensorMac, sizeof(_configs[index].sensorMac) - 1);
        if (sensorName) strncpy(_configs[index].sensorName, sensorName, sizeof(_configs[index].sensorName) - 1);
        if (relayPin > 0) {
            _configs[index].relayPin = relayPin;
            pinMode(relayPin, OUTPUT);
        }
        if (targetTemp >= 15.0f && targetTemp <= 30.0f) _configs[index].targetTemp = targetTemp;
        if (hysteresis >= 0.1f && hysteresis <= 2.0f) _configs[index].hysteresis = hysteresis;
        _configs[index].enabled = enabled;
        saveToPreferences();
        netConsole.printf("[Thermal] Zone %u fully mapped: '%s' | MAC: %s | Pin: %u | Target: %.1f C\n",
            _configs[index].id, _configs[index].name, _configs[index].sensorMac, _configs[index].relayPin, _configs[index].targetTemp);
    }
}

void ThermalZoneManager::setAutoMode(bool autoMode) {
    _autoMode = autoMode;
    saveToPreferences();
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

        // Resolution order:
        // A. Match by explicit MAC via BleSensorScanner
        // B. Match by explicit MAC via MqttTelemetryClient
        // C. Match by Sensor Name via BleSensorScanner
        // D. Match by Sensor Name via MqttTelemetryClient
        float currentTemp = -999.0f;
        bool found = false;

        if (strlen(_configs[i].sensorMac) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByMac(_configs[i].sensorMac);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < 1800000UL)) {
                currentTemp = ble->temperature;
                found = true;
            } else {
                const RoomSensor* mqttS = telemetry.findSensorByMac(_configs[i].sensorMac);
                if (mqttS && mqttS->valid && (millis() - mqttS->lastSeenMs < 1800000UL)) {
                    currentTemp = mqttS->temperature;
                    found = true;
                }
            }
        }

        if (!found && strlen(_configs[i].sensorName) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByName(_configs[i].sensorName);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < 1800000UL)) {
                currentTemp = ble->temperature;
                found = true;
            } else {
                const RoomSensor* mqttS = telemetry.findSensorByName(_configs[i].sensorName);
                if (mqttS && mqttS->valid && (millis() - mqttS->lastSeenMs < 1800000UL)) {
                    currentTemp = mqttS->temperature;
                    found = true;
                }
            }
        }

        if (found) {
            _statuses[i].sensorValid = true;
            _statuses[i].currentTemp = currentTemp;

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
            // Failsafe: if sensor lost >30min, turn off demand for safety
            if (_statuses[i].demand) {
                netConsole.printf("[Thermal] WARNING: Zone %u (%s) sensor timeout! Failsafe shutdown.\n",
                    _configs[i].id, _configs[i].name);
                _statuses[i].demand = false;
            }
        }
    }

    // 2. Actuator Relay State Machine with Warm-up Delay
    bool anyZoneDemanding = false;
    bool anyZoneReadyForBoiler = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (!_configs[i].enabled) continue;

        if (_statuses[i].demand) {
            anyZoneDemanding = true;
            if (_statuses[i].state == ZONE_IDLE) {
                // Begin actuator warm-up
                _statuses[i].state = ZONE_WARMING_UP;
                _statuses[i].stateChangedMs = now;
                applyRelay(_configs[i].relayPin, true);
                _statuses[i].relayEngaged = true;
                netConsole.printf("[Thermal] Actuator Zone %u (%s) opening on pin %u. Waiting %u s...\n",
                    _configs[i].id, _configs[i].name, _configs[i].relayPin, _interlockDelayMs / 1000);
            } else if (_statuses[i].state == ZONE_WARMING_UP) {
                // Check if interlock delay expired
                if (now - _statuses[i].stateChangedMs >= _interlockDelayMs) {
                    _statuses[i].state = ZONE_HEATING;
                    _statuses[i].stateChangedMs = now;
                    netConsole.printf("[Thermal] Actuator Zone %u (%s) fully OPEN. Ready for boiler flame!\n",
                        _configs[i].id, _configs[i].name);
                }
            }

            if (_statuses[i].state == ZONE_HEATING) {
                anyZoneReadyForBoiler = true;
            }
        } else {
            // Demand is false
            if (_statuses[i].state == ZONE_HEATING || _statuses[i].state == ZONE_WARMING_UP) {
                // Turn off relay immediately if boiler is not running or pump overrun will handle it
                _statuses[i].state = ZONE_IDLE;
                _statuses[i].stateChangedMs = now;
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
                netConsole.printf("[Thermal] Zone %u (%s) closed relay on pin %u.\n",
                    _configs[i].id, _configs[i].name, _configs[i].relayPin);
            }
        }
    }

    // 3. Boiler Flame Control & Anti-Short-Cycling Protection
    if (anyZoneReadyForBoiler) {
        if (!_boilerDemand) {
            // Check anti-short-cycling delay
            if (now - _lastBoilerOffTime >= _minBoilerOffMs || _lastBoilerOffTime == 0) {
                _boilerDemand = true;
                _lastBoilerOnTime = now;
                boiler.sendBoilerOn();
                netConsole.println(F("[Thermal] BOILER START commanded via RF 433MHz!"));
            } else {
                uint32_t waitSec = (_minBoilerOffMs - (now - _lastBoilerOffTime)) / 1000;
                netConsole.printf("[Thermal] Anti-short-cycling active. Waiting %u s before boiler ignition...\n", waitSec);
            }
        }
    } else {
        if (_boilerDemand) {
            _boilerDemand = false;
            _lastBoilerOffTime = now;
            boiler.sendBoilerOff();
            netConsole.println(F("[Thermal] BOILER STOP commanded via RF 433MHz!"));
        }
    }
}
