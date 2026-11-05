#include "Commands.h"
#include "Config.h"
#include "ThermalZoneManager.h"
#include "BoilerRadioTrigger.h"
#include "DiagnosticEngine.h"
#include "OutputBank.h"

Commands commands;

#pragma pack(push, 1)
struct ApiCredRecord {
    uint8_t magic; // 0xA5
    char user[32];
    char pass[64];
};
#pragma pack(pop)

void Commands::begin() {
    loadCredentials();
}

void Commands::loadCredentials() {
    ApiCredRecord rec;
    _customCreds = false;
    if (paramStore.loadBlob("apicred", &rec, sizeof(rec)) && rec.magic == 0xA5) {
        strlcpy(_user, rec.user, sizeof(_user));
        strlcpy(_pass, rec.pass, sizeof(_pass));
        _customCreds = true;
    } else {
        strlcpy(_user, OTA_USER, sizeof(_user));
        strlcpy(_pass, OTA_PASS, sizeof(_pass));
    }
}

bool Commands::isDefaultCredentials() const {
    // FR-59: reports DTC-403 while any default is active
    if (!_customCreds) {
        return true;
    }
    if (strcmp(_pass, "admin123") == 0 || strcmp(_pass, DEFAULT_ADMIN_PASS) == 0 || strcmp(_user, "admin") == 0) {
        return true;
    }
    return false;
}

void Commands::resetToDefault() {
    paramStore.eraseBlob("apicred");
    strlcpy(_user, OTA_USER, sizeof(_user));
    strlcpy(_pass, DEFAULT_ADMIN_PASS, sizeof(_pass));
    _customCreds = false;
}

bool Commands::checkCredentials(const char* user, const char* pass) const {
    if (!user || !pass) return false;
    return (strcmp(user, _user) == 0 && strcmp(pass, _pass) == 0);
}

const char* Commands::getUser() const {
    return _user;
}

const char* Commands::getPass() const {
    return _pass;
}

bool Commands::setCredentials(const char* user, const char* pass, bool authenticated) {
    if (!authenticated || !user || !pass) return false;
    size_t ulen = strlen(user);
    size_t plen = strlen(pass);
    if (ulen == 0 || ulen >= sizeof(_user) || plen < 8 || plen >= sizeof(_pass)) return false;

    ApiCredRecord rec;
    rec.magic = 0xA5;
    strlcpy(rec.user, user, sizeof(rec.user));
    strlcpy(rec.pass, pass, sizeof(rec.pass));
    paramStore.saveBlob("apicred", &rec, sizeof(rec));

    strlcpy(_user, user, sizeof(_user));
    strlcpy(_pass, pass, sizeof(_pass));
    _customCreds = true;
    return true;
}

CmdResult Commands::setSetpoint(uint8_t zoneIndex, float temp) {
    if (zoneIndex >= thermalZones.getZoneCount()) {
        return {CmdCode::NOT_FOUND, 0, "Zone not found"};
    }
    if (temp < 10.0f || temp > 30.0f) {
        return {CmdCode::BAD_PARAM, 0, "Target out of bounds [10, 30]"};
    }
    if (!thermalZones.setTargetTemperatureChecked(zoneIndex, temp)) {
        return {CmdCode::BAD_PARAM, 0, "Invalid boost step"};
    }
    return {CmdCode::OK, 0, "Target updated"};
}

CmdResult Commands::clearWindowPause(uint8_t zoneIndex) {
    if (zoneIndex == 0xFF) {
        for (size_t i = 0; i < thermalZones.getZoneCount(); i++) {
            thermalZones.clearWindowPause(i);
        }
        return {CmdCode::OK, 0, "Cleared all window pauses"};
    }
    if (zoneIndex >= thermalZones.getZoneCount()) {
        return {CmdCode::NOT_FOUND, 0, "Zone not found"};
    }
    bool cleared = thermalZones.clearWindowPause(zoneIndex);
    return {CmdCode::OK, 0, cleared ? "Window pause cleared" : "Window pause was not active"};
}

CmdResult Commands::clearHold(uint8_t zoneIndex) {
    if (zoneIndex >= thermalZones.getZoneCount()) {
        return {CmdCode::NOT_FOUND, 0, "Zone not found"};
    }
    thermalZones.clearManualHold(zoneIndex);
    return {CmdCode::OK, 0, "Manual hold cleared"};
}

CmdResult Commands::setAway(bool away) {
    thermalZones.setAway(away);
    return {CmdCode::OK, 0, away ? "Away mode ON" : "Away mode OFF"};
}

CmdResult Commands::setSeasonMode(uint8_t mode) {
    if (mode > 2) return {CmdCode::BAD_PARAM, 0, "Invalid season mode"};
    thermalZones.setSeasonMode(mode);
    return {CmdCode::OK, 0, "Season mode updated"};
}

CmdResult Commands::setManualMode(bool manual, bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required for MANUAL mode"};
    }
    thermalZones.setAutoMode(!manual);
    return {CmdCode::OK, 0, manual ? "Manual mode ON" : "Manual mode OFF"};
}

CmdResult Commands::boilerOn(bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required for boiler manual control"};
    }
    if (boilerRadio.getPendingAction() == PENDING_OFF) {
        boilerRadio.cancelPendingAction();
        return {CmdCode::OK, 0, "Pending OFF cancelled"};
    }
    if (boilerRadio.hasTransmittedOnce() && boilerRadio.isLastCommandOn()) {
        return {CmdCode::OK, 0, "Boiler already ON"};
    }
    if (!thermalZones.boilerOnPermitted()) {
        boilerRadio.noteRefusal(RS_INTERLOCK, 0, true);
        boilerRadio.recordSpamAttempt();
        return {CmdCode::REFUSED, 0, "No zone valve open for interlockDelay"};
    }
    uint32_t waitSec = 0;
    bool isIdempotent = false;
    uint8_t guardWhy = RS_NONE;
    if (!boilerRadio.canTransmit(true, &waitSec, &isIdempotent, &guardWhy)) {
        boilerRadio.noteRefusal(guardWhy, waitSec, true);
        boilerRadio.recordSpamAttempt();
        if (!boilerRadio.isLastCommandOn()) {
            boilerRadio.setPendingAction(PENDING_ON);
            return {CmdCode::RATE_LIMITED, (uint16_t)waitSec, "Anti-short-cycling guard active (pending ON)"};
        }
        return {CmdCode::RATE_LIMITED, (uint16_t)waitSec, "Anti-short-cycling guard active"};
    }
    bool success = boilerRadio.sendBoilerOn();
    return success ? CmdResult{CmdCode::OK, 0, "Boiler commanded ON"}
                   : CmdResult{CmdCode::RATE_LIMITED, (uint16_t)waitSec, "Transmission throttled"};
}

CmdResult Commands::boilerOff(bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required for boiler manual control"};
    }
    if (boilerRadio.getPendingAction() == PENDING_ON) {
        boilerRadio.cancelPendingAction();
        return {CmdCode::OK, 0, "Pending ON cancelled"};
    }
    if (boilerRadio.hasTransmittedOnce() && !boilerRadio.isLastCommandOn()) {
        return {CmdCode::OK, 0, "Boiler already OFF"};
    }
    uint32_t waitSec = 0;
    bool isIdempotent = false;
    uint8_t guardWhy = RS_NONE;
    if (!boilerRadio.canTransmit(false, &waitSec, &isIdempotent, &guardWhy)) {
        boilerRadio.noteRefusal(guardWhy, waitSec, true);
        boilerRadio.recordSpamAttempt();
        if (boilerRadio.isLastCommandOn()) {
            boilerRadio.setPendingAction(PENDING_OFF);
            return {CmdCode::RATE_LIMITED, (uint16_t)waitSec, "Burner minimum run active (pending OFF)"};
        }
        return {CmdCode::RATE_LIMITED, (uint16_t)waitSec, "Burner minimum run active"};
    }
    bool success = boilerRadio.sendBoilerOff();
    return success ? CmdResult{CmdCode::OK, 0, "Boiler commanded OFF"}
                   : CmdResult{CmdCode::RATE_LIMITED, (uint16_t)waitSec, "Transmission throttled"};
}

CmdResult Commands::boilerCancel(bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required"};
    }
    boilerRadio.cancelPendingAction();
    return {CmdCode::OK, 0, "Pending action cancelled"};
}

CmdResult Commands::relayTest(uint8_t pin, bool state, bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required for relay test"};
    }
    if (!ParamStore::validRelayPin(pin)) {
        return {CmdCode::BAD_PARAM, 0, "Invalid relay pin"};
    }
    int zone = thermalZones.zoneUsingRelay(pin, SIZE_MAX);
    bool isPump = zone < 0 && thermalZones.isPumpControlEnabled() && pin == thermalZones.getPumpRelayPin();
    if (thermalZones.isAutoMode() || (zone < 0 && !isPump)) {
        return {CmdCode::REFUSED, 0, "Relay test needs MANUAL mode and an output of a zone or the pump"};
    }
    if (zone >= 0) thermalZones.setRelayManual((size_t)zone, state);
    else thermalZones.setPumpManual(state);
    return {CmdCode::OK, 0, "Relay updated"};
}

CmdResult Commands::clearDtcs(bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required to clear DTCs"};
    }
    diagEngine.clearAllDtcs();
    return {CmdCode::OK, 0, "DTCs cleared"};
}

CmdResult Commands::resetRuntime(bool authenticated) {
    if (!authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required to reset runtime"};
    }
    boilerRadio.resetRuntime();
    return {CmdCode::OK, 0, "Runtime counters reset"};
}

CmdResult Commands::setParameter(Param p, int32_t val, bool authenticated) {
    // Safety parameters in FR-58 require authentication
    bool isSafetyParam = (p == Param::InterlockDelay || p == Param::Overrun ||
                          p == Param::BurnerMinRun || p == Param::BurnerMinPause ||
                          p == Param::SensorTimeout || p == Param::FrostTemp ||
                          p == Param::HbInterval || p == Param::BoilerLinkHeartbeat ||
                          p == Param::OpenLoopBypass || p == Param::ActuatorType ||
                          p == Param::PumpOutput);
    if (isSafetyParam && !authenticated) {
        return {CmdCode::UNAUTHORIZED, 0, "Authentication required for safety parameters"};
    }
    bool ok = paramStore.set(p, val);
    return ok ? CmdResult{CmdCode::OK, 0, "Parameter saved"}
              : CmdResult{CmdCode::BAD_PARAM, 0, "Parameter value out of bounds"};
}
