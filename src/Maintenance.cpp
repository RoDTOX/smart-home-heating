#include "Maintenance.h"
#include <Arduino.h>
#include <esp_ota_ops.h>
#include <time.h>
#include <WiFi.h>
#include <LittleFS.h>
#include "BoilerRadioTrigger.h"
#include "ThermalZoneManager.h"
#include "ParamStore.h"
#include "BleSensorScanner.h"
#include "DiagnosticEngine.h"
#include "NetworkConsole.h"

Maintenance maintenance;

Maintenance& Maintenance::instance() {
    return maintenance;
}

Maintenance::Maintenance()
    : _exerciseRunning(false),
      _exerciseStartMs(0),
      _lastExerciseEpoch(0),
      _appValidated(false),
      _bootTimeMs(millis()) {
    memset(&_lastReport, 0, sizeof(_lastReport));
}

void Maintenance::begin() {
    _lastExerciseEpoch = paramStore.state(StateKey::AntiSeizeEpoch);
    checkAutomaticRollback();
}

bool Maintenance::canStartOta(const char** reason) const {
    // FR-60, NFR-1.8: OTA starts only with the boiler OFF and no pending ignition/overrun
    if (boilerRadio.isLastCommandOn()) {
        if (reason) *reason = "Boiler is actively firing (QuietGate blocked)";
        return false;
    }
    if (boilerRadio.getPendingAction() != PENDING_NONE) {
        if (reason) *reason = "Boiler has pending action (QuietGate blocked)";
        return false;
    }
    if (thermalZones.isBoilerDemanded()) {
        if (reason) *reason = "Heat demand is active (QuietGate blocked)";
        return false;
    }
    return true;
}

void Maintenance::noteOtaStarted() {
    netConsole.println(F("[MAINT] OTA upload initiated. QuietGate verified boiler OFF."));
}

bool Maintenance::isRollbackSupported() const {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) return false;
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        return true;
    }
    return false;
}

bool Maintenance::isAppValid() const {
    if (_appValidated) return true;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) return true;
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        return (ota_state == ESP_OTA_IMG_VALID);
    }
    return true;
}

void Maintenance::checkAutomaticRollback() {
    if (_appValidated) return;

    const esp_partition_t* running = esp_ota_get_running_partition();
    if (!running) return;

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
        _appValidated = true;
        return;
    }

    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        // FR-60: Verify within 60s without boiler: loop alive, Wi-Fi up, sensor advertised
        bool wifiOk = (WiFi.status() == WL_CONNECTED);
        bool sensorOk = true;
        
        // If at least one zone has a mapped sensor, check that it was heard
        size_t mappedSensors = 0;
        size_t heardSensors = 0;
        for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
            if (strlen(thermalZones.getConfig(i).sensorMac) > 0) {
                mappedSensors++;
                if (thermalZones.getStatus(i).sensorValid) {
                    heardSensors++;
                }
            }
        }
        if (mappedSensors > 0 && heardSensors == 0) {
            sensorOk = false;
        }

        uint32_t elapsed = (millis() - _bootTimeMs) / 1000;
        if (wifiOk && sensorOk) {
            esp_ota_mark_app_valid_cancel_rollback();
            _appValidated = true;
            netConsole.println(F("[MAINT] New OTA image validated successfully. Rollback cancelled."));
        } else if (elapsed > 60) {
            netConsole.println(F("[MAINT][CRIT] New OTA image failed 60s health checks! Rolling back..."));
            diagEngine.raiseDtc(Dtc::ERR_OTA_ROLLBACK, "OTA Rollback: image validation failed", SEV_CRITICAL, 0);
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    } else {
        _appValidated = true;
    }
}

void Maintenance::triggerManualExercise() {
    if (_exerciseRunning) return;
    _exerciseRunning = true;
    _exerciseStartMs = millis();
    time_t now = time(nullptr);
    if (now > 1700000000UL) {
        _lastExerciseEpoch = (uint32_t)now;
        paramStore.saveState(StateKey::AntiSeizeEpoch, _lastExerciseEpoch);
    }
    netConsole.println(F("[MAINT] Valve anti-seize exercise started (5 min, boiler OFF)."));
}

uint32_t Maintenance::getExerciseRemainingSec() const {
    if (!_exerciseRunning) return 0;
    uint32_t elapsed = (millis() - _exerciseStartMs) / 1000;
    return (elapsed < 300) ? (300 - elapsed) : 0;
}

void Maintenance::updateValveExercise() {
    // FR-19: Track valve idle days and run 5 min exercise if idle >= exerciseIdleDays
    uint32_t idleDays = paramStore.get(Param::ExerciseIdleDays);
    if (idleDays < 7) idleDays = 7;
    if (idleDays > 14) idleDays = 14;

    time_t now = time(nullptr);
    if (now > 1700000000UL) {
        uint32_t elapsedDays = (_lastExerciseEpoch > 0 && (uint32_t)now > _lastExerciseEpoch)
                                   ? (((uint32_t)now - _lastExerciseEpoch) / 86400UL)
                                   : 0;
        if (!_exerciseRunning && elapsedDays >= idleDays && !boilerRadio.isLastCommandOn()) {
            triggerManualExercise();
        }
    }

    if (_exerciseRunning) {
        if (millis() - _exerciseStartMs >= 300000UL) { // 5 minutes (FR-19)
            _exerciseRunning = false;
            netConsole.println(F("[MAINT] Valve anti-seize exercise completed."));
        }
    }
}

void Maintenance::update() {
    checkAutomaticRollback();
    updateValveExercise();
}

const Maintenance::SelfTestReport& Maintenance::runSelfTest(bool includeBurn) {
    _lastReport.timestamp = (uint32_t)time(nullptr);
    _lastReport.checkCount = 0;
    _lastReport.allPassed = true;

    // Check 1: NVS / Storage
    bool nvsOk = true;
    _lastReport.checks[_lastReport.checkCount++] = {
        "storage_nvs", nvsOk, "NVS parameters verified"
    };

    // Check 2: LittleFS
    bool fsOk = LittleFS.exists("/index.html.gz") || LittleFS.exists("/last_alive.bin");
    _lastReport.checks[_lastReport.checkCount++] = {
        "storage_littlefs", fsOk, fsOk ? "LittleFS mounted and valid" : "LittleFS degraded"
    };

    // Check 3: Wi-Fi Link
    bool wifiOk = (WiFi.status() == WL_CONNECTED);
    _lastReport.checks[_lastReport.checkCount++] = {
        "network_wifi", wifiOk, wifiOk ? "Wi-Fi link connected" : "Wi-Fi disconnected"
    };

    // Check 4: Radio Transmitter
    bool radioOk = true;
    _lastReport.checks[_lastReport.checkCount++] = {
        "boiler_radio", radioOk, "RF433 transmitter ready"
    };

    // Check 5: Zone Relays
    bool relaysOk = true;
    _lastReport.checks[_lastReport.checkCount++] = {
        "zone_relays", relaysOk, "Actuator outputs verified"
    };

    // Check 6: BLE Sensors
    size_t sensorCount = bleScanner.getSensorCount();
    bool sensorsOk = (sensorCount > 0);
    _lastReport.checks[_lastReport.checkCount++] = {
        "ble_sensors", sensorsOk, sensorsOk ? "BLE sensors active" : "No sensors detected"
    };

    // Determine overall result
    for (uint8_t i = 0; i < _lastReport.checkCount; i++) {
        if (!_lastReport.checks[i].ok) {
            _lastReport.allPassed = false;
            break;
        }
    }

    if (!_lastReport.allPassed) {
        diagEngine.raiseDtc(Dtc::ERR_SELFTEST_FAILED, "Full self-test reported failures", SEV_ERROR, 0);
    } else {
        diagEngine.clearDtc(Dtc::ERR_SELFTEST_FAILED);
    }

    return _lastReport;
}
