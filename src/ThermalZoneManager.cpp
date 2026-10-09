#include "ThermalZoneManager.h"
#include "NetworkConsole.h"
#include "BleSensorScanner.h"
#include "DiagnosticEngine.h"

ThermalZoneManager thermalZones;

ThermalZoneManager::ThermalZoneManager()
    : _autoMode(true),
      _boilerDemand(false),
      _pumpControlEnabled(true),
      _pumpActive(false),
      _pumpStopScheduledMs(0),
      _overrunKeeperZone(-1),
      _actuatorNormallyClosed(true),
      _freezeProtectionTemp(12.0f),
      _frostProtectionActive(false),
      _antiSeizeRunning(false),
      _antiSeizeStartMs(0),
      _lastAntiSeizeRunMs(0),
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
        _statuses[i].sensorFault = false;
        _statuses[i].frostActive = false;
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
    _pumpControlEnabled = prefs.getBool("pump_en", true);
    _actuatorNormallyClosed = prefs.getBool("act_nc", true);
    _freezeProtectionTemp = prefs.getFloat("frz_temp", 12.0f);
    _lastAntiSeizeRunMs = prefs.getUInt("as_last", 0);

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
    netConsole.println(F("[Thermal] Zone configs, pump settings, and actuator type loaded from NVS."));
}

void ThermalZoneManager::saveToPreferences() {
    Preferences prefs;
    if (!prefs.begin("zones", false)) {
        netConsole.println(F("[Thermal] ERROR: Failed to open NVS namespace 'zones' for writing!"));
        return;
    }

    prefs.putBool("z_init", true);
    prefs.putBool("autoMode", _autoMode);
    prefs.putBool("pump_en", _pumpControlEnabled);
    prefs.putBool("act_nc", _actuatorNormallyClosed);
    prefs.putFloat("frz_temp", _freezeProtectionTemp);
    prefs.putUInt("as_last", _lastAntiSeizeRunMs);

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
    netConsole.println(F("[Thermal] Zone configs successfully persisted to NVS."));
}

void ThermalZoneManager::applyRelay(uint8_t pin, bool active) {
    if (pin == PIN_RELAY_SPARE) {
        // Circulation pump is directly active HIGH (runs when energized)
        digitalWrite(pin, active ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    } else {
        // Thermal zone actuator:
        // NC (Normally Closed): active (open valve) -> energize relay (HIGH)
        // NO (Normally Open):   active (open valve) -> de-energize relay (LOW)
        bool level = _actuatorNormallyClosed ? active : !active;
        digitalWrite(pin, level ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL);
    }
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
    _pumpActive = false;

    netConsole.printf("[Thermal] Initialized: 3 zones, Actuators: %s, Pump Relay 4: %s\n",
        _actuatorNormallyClosed ? "NC (Normally Closed)" : "NO (Normally Open)",
        _pumpControlEnabled ? "ENABLED" : "DISABLED");
}

const ThermalZoneConfig& ThermalZoneManager::getConfig(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return _configs[0];
    return _configs[index];
}

const ThermalZoneStatus& ThermalZoneManager::getStatus(size_t index) const {
    if (index >= MAX_THERMAL_ZONES) return _statuses[0];
    return _statuses[index];
}

void ThermalZoneManager::setPumpControlEnabled(bool enabled) {
    _pumpControlEnabled = enabled;
    if (!enabled && _pumpActive) {
        applyRelay(PIN_RELAY_SPARE, false);
        _pumpActive = false;
    }
    saveToPreferences();
    netConsole.printf("[Thermal] Circulation pump control via Relay 4 %s\n", enabled ? "ENABLED" : "DISABLED");
}

void ThermalZoneManager::setActuatorNormallyClosed(bool nc) {
    _actuatorNormallyClosed = nc;
    saveToPreferences();
    // Refresh all zone relays with new logic polarity
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled) {
            applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged);
        }
    }
    netConsole.printf("[Thermal] Actuator type set to: %s\n", nc ? "NC (Normally Closed)" : "NO (Normally Open)");
}

void ThermalZoneManager::setFreezeProtectionTemp(float temp) {
    if (temp >= 5.0f && temp <= 16.0f) {
        _freezeProtectionTemp = temp;
        saveToPreferences();
        netConsole.printf("[Thermal] Frost protection threshold set to: %.1f C\n", temp);
    }
}

void ThermalZoneManager::triggerAntiSeizeExercise() {
    if (_antiSeizeRunning) return;
    _antiSeizeRunning = true;
    _antiSeizeStartMs = millis();
    netConsole.println(F("[Thermal] >>> ANTI-SEIZE EXERCISE STARTED (Pumps & Valves) <<<"));

    // Open all enabled actuators
    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (_configs[i].enabled) {
            applyRelay(_configs[i].relayPin, true);
        }
    }
    // Start circulation pump if enabled
    if (_pumpControlEnabled) {
        applyRelay(PIN_RELAY_SPARE, true);
        _pumpActive = true;
    }
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
        netConsole.printf("[Thermal] Zone %u mapped: '%s' | MAC: %s | Pin: %u | Target: %.1f C\n",
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

void ThermalZoneManager::setPumpManual(bool on) {
    if (!_autoMode) {
        _pumpActive = on;
        applyRelay(PIN_RELAY_SPARE, on);
        netConsole.printf("[Thermal][MANUAL] Circulation Pump Relay 4 switched to %s\n", on ? "ON" : "OFF");
    }
}

void ThermalZoneManager::setBoilerManual(BoilerRadioTrigger& boiler, bool on) {
    if (!_autoMode) {
        if (on) {
            if (boiler.sendBoilerOn()) {
                _lastBoilerOnTime = millis();
            }
        } else {
            if (boiler.sendBoilerOff()) {
                _lastBoilerOffTime = millis();
            }
        }
    }
}

const char* ThermalZoneManager::getStateStr(ZoneState state) const {
    switch (state) {
        case ZONE_DISABLED:      return "Disabled";
        case ZONE_IDLE:          return "Idle (Temp OK)";
        case ZONE_WARMING_UP:    return "Actuator Warming Up";
        case ZONE_HEATING:       return "Heating Active";
        case ZONE_FAULT_TIMEOUT: return "Sensor Timeout (Failsafe OFF)";
        case ZONE_FROST_GUARD:   return "Frost Protection Active (<12C)";
        default:                 return "Unknown";
    }
}

void ThermalZoneManager::checkAntiSeizeSchedule(uint32_t now) {
    // Run anti-seize exercise every 7 days (604800000 ms) if heating hasn't been active
    if (_lastBoilerOnTime == 0 || (now - _lastBoilerOnTime > 604800000UL)) {
        if (_lastAntiSeizeRunMs == 0 || (now - _lastAntiSeizeRunMs > 604800000UL)) {
            triggerAntiSeizeExercise();
        }
    }
}

void ThermalZoneManager::update(BoilerRadioTrigger& boiler) {
    uint32_t now = millis();
    if (now - _lastEvaluationMs < 1000) {
        return;
    }
    _lastEvaluationMs = now;

    // Handle ongoing Anti-Seize Exercise (20s duration)
    if (_antiSeizeRunning) {
        if (now - _antiSeizeStartMs >= 20000UL) {
            _antiSeizeRunning = false;
            _lastAntiSeizeRunMs = now;
            saveToPreferences();

            // Turn off pump
            if (_pumpControlEnabled) {
                applyRelay(PIN_RELAY_SPARE, false);
                _pumpActive = false;
            }
            // Restore relays based on demand
            for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
                if (_configs[i].enabled) {
                    applyRelay(_configs[i].relayPin, _statuses[i].relayEngaged);
                }
            }
            netConsole.println(F("[Thermal] Anti-seize exercise completed successfully."));
        }
        return; // Don't run standard logic during exercise
    }

    // Periodic check for anti-seize
    checkAntiSeizeSchedule(now);

    // 1. Evaluate heat demand per zone
    _frostProtectionActive = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (!_configs[i].enabled) {
            _statuses[i].state = ZONE_DISABLED;
            _statuses[i].demand = false;
            _statuses[i].sensorFault = false;
            _statuses[i].frostActive = false;
            if (_statuses[i].relayEngaged) {
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
            }
            continue;
        }

        float currentTemp = -999.0f;
        bool found = false;

        // Direct Native BLE Sensor Resolution
        if (strlen(_configs[i].sensorMac) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByMac(_configs[i].sensorMac);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < 1800000UL)) {
                currentTemp = ble->temperature;
                found = true;
            }
        }

        if (!found && strlen(_configs[i].sensorName) > 0) {
            const BleSensorData* ble = bleScanner.findSensorByName(_configs[i].sensorName);
            if (ble && ble->valid && (millis() - ble->lastSeenMs < 1800000UL)) {
                currentTemp = ble->temperature;
                found = true;
            }
        }

        if (found) {
            _statuses[i].sensorValid = true;
            _statuses[i].sensorFault = false;
            _statuses[i].currentTemp = currentTemp;

            // Recover from sensor timeout fault as soon as valid reading is received
            if (_statuses[i].state == ZONE_FAULT_TIMEOUT) {
                _statuses[i].state = _statuses[i].demand ? ZONE_HEATING : ZONE_IDLE;
            }

            // Failsafe: Emergency Frost Protection Guard (< 12C)
            if (currentTemp < _freezeProtectionTemp) {
                _statuses[i].frostActive = true;
                _frostProtectionActive = true;
                _statuses[i].demand = true;
                _statuses[i].state = ZONE_FROST_GUARD;
                netConsole.printf("[Thermal][FROST GUARD] Emergency heating triggered on Zone %u (%s): %.1f C < %.1f C!\n",
                    _configs[i].id, _configs[i].name, currentTemp, _freezeProtectionTemp);
            } else if (_statuses[i].frostActive && currentTemp >= (_freezeProtectionTemp + 1.5f)) {
                _statuses[i].frostActive = false;
            }

            // Normal Hysteresis loop (only if autoMode and not forced by frost guard)
            if (_autoMode && !_statuses[i].frostActive) {
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
                        _statuses[i].state = ZONE_IDLE;
                        netConsole.printf("[Thermal] Zone %u (%s) target reached: %.1f C >= %.1f C (target: %.1f C)\n",
                            _configs[i].id, _configs[i].name,
                            _statuses[i].currentTemp,
                            _configs[i].targetTemp + _configs[i].hysteresis,
                            _configs[i].targetTemp);
                    }
                }
            }
        } else {
            // Sensor missing > 30 minutes: CRITICAL FAILSAFE
            _statuses[i].sensorValid = false;
            _statuses[i].sensorFault = true;
            _statuses[i].state = ZONE_FAULT_TIMEOUT;

            if (_statuses[i].demand) {
                netConsole.printf("[Thermal] CRITICAL FAILSAFE: Zone %u (%s) sensor timeout >30 min! Shutting down actuator.\n",
                    _configs[i].id, _configs[i].name);
                _statuses[i].demand = false;
                applyRelay(_configs[i].relayPin, false);
                _statuses[i].relayEngaged = false;
                diagEngine.raiseDtc("DTC-101", "Sensor timeout >30 min - zone shutdown", SEV_ERROR);
            }
        }
    }

    if (!_autoMode && !_frostProtectionActive) {
        return; // In manual mode without frost emergency, skip automatic actuator & boiler sequencing
    }

    // 2. Actuator Relay State Machine with Warm-up Delay
    bool anyZoneDemanding = false;
    bool anyZoneReadyForBoiler = false;

    for (size_t i = 0; i < MAX_THERMAL_ZONES; i++) {
        if (!_configs[i].enabled || _statuses[i].sensorFault) continue;

        if (_statuses[i].demand) {
            anyZoneDemanding = true;
            if (_statuses[i].state == ZONE_IDLE || _statuses[i].state == ZONE_FAULT_TIMEOUT) {
                // Begin actuator warm-up
                _statuses[i].state = ZONE_WARMING_UP;
                _statuses[i].stateChangedMs = now;
                applyRelay(_configs[i].relayPin, true);
                _statuses[i].relayEngaged = true;
                netConsole.printf("[Thermal] Actuator Zone %u (%s) opening on pin %u. Waiting %u s...\n",
                    _configs[i].id, _configs[i].name, _configs[i].relayPin, _interlockDelayMs / 1000);
            } else if (_statuses[i].state == ZONE_WARMING_UP) {
                if (now - _statuses[i].stateChangedMs >= _interlockDelayMs) {
                    _statuses[i].state = ZONE_HEATING;
                    _statuses[i].stateChangedMs = now;
                    netConsole.printf("[Thermal] Actuator Zone %u (%s) fully OPEN. Circuit ready!\n",
                        _configs[i].id, _configs[i].name);
                }
            }

            if (_statuses[i].state == ZONE_HEATING || _statuses[i].state == ZONE_FROST_GUARD) {
                anyZoneReadyForBoiler = true;
                _overrunKeeperZone = (int8_t)i; // Track last active zone for hydraulic overrun heat dissipation
            }
        } else {
            // Demand is false
            if (_statuses[i].state == ZONE_HEATING || _statuses[i].state == ZONE_WARMING_UP) {
                // If pump overrun is in progress and this is the keeper zone, keep it open until pump stops
                if (_pumpActive && (int8_t)i == _overrunKeeperZone) {
                    // Keeper zone stays engaged for residual heat dissipation
                } else {
                    _statuses[i].state = ZONE_IDLE;
                    _statuses[i].stateChangedMs = now;
                    applyRelay(_configs[i].relayPin, false);
                    _statuses[i].relayEngaged = false;
                    netConsole.printf("[Thermal] Zone %u (%s) closed relay on pin %u.\n",
                        _configs[i].id, _configs[i].name, _configs[i].relayPin);
                }
            }
        }
    }

    // 3. Boiler Flame & Secondary Circulation Pump Control
    if (anyZoneReadyForBoiler) {
        if (!_boilerDemand) {
            // Anti-short-cycling check
            if (now - _lastBoilerOffTime >= _minBoilerOffMs || _lastBoilerOffTime == 0) {
                _boilerDemand = true;
                _lastBoilerOnTime = now;
                boiler.sendBoilerOn();
                netConsole.println(F("[Thermal] BOILER START commanded via RF 433MHz!"));

                // Start circulation pump if enabled (Interlock satisfied: at least 1 actuator is open!)
                if (_pumpControlEnabled && !_pumpActive) {
                    applyRelay(PIN_RELAY_SPARE, true);
                    _pumpActive = true;
                    netConsole.println(F("[Thermal] Circulation Pump (Relay 4) STARTED."));
                }
            } else {
                uint32_t waitSec = (_minBoilerOffMs - (now - _lastBoilerOffTime)) / 1000;
                netConsole.printf("[Thermal] Anti-short-cycling active. Waiting %u s before ignition...\n", waitSec);
            }
        } else {
            // Boiler is running; ensure pump is running as well
            if (_pumpControlEnabled && !_pumpActive) {
                applyRelay(PIN_RELAY_SPARE, true);
                _pumpActive = true;
            }
        }
    } else {
        // No zones ready for boiler
        if (_boilerDemand) {
            _boilerDemand = false;
            _lastBoilerOffTime = now;
            boiler.sendBoilerOff();
            netConsole.println(F("[Thermal] BOILER STOP commanded via RF 433MHz!"));

            // Schedule Pump Overrun (Post-circulation)
            if (_pumpControlEnabled && _pumpActive) {
                _pumpStopScheduledMs = now + _pumpOverrunMs;
                netConsole.printf("[Thermal] Pump Overrun active: pump will run for %u s to dissipate heat.\n", _pumpOverrunMs / 1000);
            }
        }

        // Handle pump overrun termination
        if (_pumpActive && _pumpControlEnabled) {
            if (_pumpStopScheduledMs > 0 && now >= _pumpStopScheduledMs) {
                applyRelay(PIN_RELAY_SPARE, false);
                _pumpActive = false;
                _pumpStopScheduledMs = 0;
                netConsole.println(F("[Thermal] Pump Overrun completed. Circulation pump stopped."));

                // Now close the keeper zone that was kept open for dissipation
                if (_overrunKeeperZone >= 0 && _overrunKeeperZone < MAX_THERMAL_ZONES) {
                    if (!_statuses[_overrunKeeperZone].demand) {
                        applyRelay(_configs[_overrunKeeperZone].relayPin, false);
                        _statuses[_overrunKeeperZone].relayEngaged = false;
                        _statuses[_overrunKeeperZone].state = ZONE_IDLE;
                        netConsole.printf("[Thermal] Overrun keeper Zone %u closed.\n", _configs[_overrunKeeperZone].id);
                    }
                    _overrunKeeperZone = -1;
                }
            }
        }
    }
}
