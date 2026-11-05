#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include "ParamStore.h"

// docs/architecture.md §4.3 & §18 Step 9: Commands (one command path, rule P8, AR-16).
// Every user action passes through Commands, so limits, authentication, and guards are identical
// for WebApi, Console, and tests.

enum class CmdCode : uint8_t {
    OK = 0,
    REFUSED,
    RATE_LIMITED,
    UNAUTHORIZED,
    BAD_PARAM,
    NOT_FOUND,
    ERROR
};

struct CmdResult {
    CmdCode code;
    uint16_t waitSec;
    const char* message;

    bool ok() const { return code == CmdCode::OK; }
};

class Commands {
public:
    void begin();

    // Comfort actions (FR-58: no authentication needed)
    CmdResult setSetpoint(uint8_t zoneIndex, float temp);
    CmdResult clearWindowPause(uint8_t zoneIndex); // 0xFF = all zones
    CmdResult setAway(bool away);
    CmdResult clearHold(uint8_t zoneIndex);

    // Mode actions (FR-58: MANUAL mode requires authentication)
    CmdResult setSeasonMode(uint8_t mode); // 0 = AUTO, 1 = ON, 2 = OFF
    CmdResult setManualMode(bool manual, bool authenticated);

    // Manual outputs (FR-43, FR-58: requires authentication)
    CmdResult boilerOn(bool authenticated);
    CmdResult boilerOff(bool authenticated);
    CmdResult boilerCancel(bool authenticated);
    CmdResult relayTest(uint8_t pin, bool state, bool authenticated);

    // Diagnostics & Maintenance (FR-58: requires authentication)
    CmdResult clearDtcs(bool authenticated);
    CmdResult resetRuntime(bool authenticated);

    // Configuration / Safety parameters (FR-58: safety params require authentication)
    CmdResult setParameter(Param p, int32_t val, bool authenticated);

    // Credentials (FR-59)
    bool isDefaultCredentials() const;
    bool setCredentials(const char* user, const char* pass, bool authenticated);
    void resetToDefault();
    bool checkCredentials(const char* user, const char* pass) const;
    const char* getUser() const;
    const char* getPass() const;

private:
    char _user[32];
    char _pass[64];
    bool _customCreds;

    void loadCredentials();
};

extern Commands commands;

#endif // COMMANDS_H
