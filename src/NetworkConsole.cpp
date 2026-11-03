#include "NetworkConsole.h"
#include <time.h>
#include <mbedtls/base64.h>
#include "ParamStore.h"

NetworkConsole netConsole;

NetworkConsole::NetworkConsole()
    : _telnetServer(TELNET_PORT),
      _webServer(nullptr),
      _commandHandler(nullptr),
      _telnetAuth(false),
      _telnetConnMs(0),
      _head(0),
      _count(0),
      _totalBytes(0),
      _atLineStart(true) {
    memset(_logRing, 0, sizeof(_logRing));
}

void NetworkConsole::begin(WebServer* server) {
    _webServer = server;
    _telnetServer.begin();
    _telnetServer.setNoDelay(true);

    if (_webServer) {
        setupWebRoutes();
    }
}

void NetworkConsole::setCommandHandler(CommandCallback cb) {
    _commandHandler = cb;
}

void NetworkConsole::appendToRing(char c) {
    appendToRing((const uint8_t*)&c, 1);
}

void NetworkConsole::appendToRing(const uint8_t *data, size_t len) {
    if (!data || len == 0) return;
    _totalBytes += len;
    if (len >= LOG_BUFFER_SIZE) {
        data += (len - LOG_BUFFER_SIZE);
        len = LOG_BUFFER_SIZE;
        memcpy(_logRing, data, LOG_BUFFER_SIZE);
        _head = 0;
        _count = LOG_BUFFER_SIZE;
        return;
    }
    size_t firstPart = (len < (LOG_BUFFER_SIZE - _head)) ? len : (LOG_BUFFER_SIZE - _head);
    memcpy(&_logRing[_head], data, firstPart);
    if (len > firstPart) {
        memcpy(&_logRing[0], data + firstPart, len - firstPart);
    }
    _head = (_head + len) % LOG_BUFFER_SIZE;
    if (_count < LOG_BUFFER_SIZE) {
        _count = (_count + len < LOG_BUFFER_SIZE) ? (_count + len) : LOG_BUFFER_SIZE;
    }
}

void NetworkConsole::printTimestamp() {
    time_t now = time(nullptr);
    static time_t lastSec = 0;
    static char cachedBuf[16] = "";
    if (now != lastSec || cachedBuf[0] == '\0') {
        lastSec = now;
        if (now > 1700000000) {
            struct tm ti;
            localtime_r(&now, &ti);
            snprintf(cachedBuf, sizeof(cachedBuf), "[%02d:%02d:%02d] ", ti.tm_hour, ti.tm_min, ti.tm_sec);
        } else {
            uint32_t s = millis() / 1000;
            snprintf(cachedBuf, sizeof(cachedBuf), "[+%02u:%02u:%02u] ", (s / 3600) % 24, (s / 60) % 60, s % 60);
        }
    }
    rawWrite((const uint8_t*)cachedBuf, strlen(cachedBuf));
}

void NetworkConsole::rawWrite(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) return;
    int avail = Serial.availableForWrite();
    if (avail > 0) {
        size_t toWrite = ((size_t)avail < size) ? (size_t)avail : size;
        Serial.write(buffer, toWrite);
    }
    appendToRing(buffer, size);
    if (_telnetClient && _telnetClient.connected()) {
        _telnetClient.write(buffer, size);
    }
}

size_t NetworkConsole::write(uint8_t c) {
    return write(&c, 1);
}

size_t NetworkConsole::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    size_t start = 0;
    while (start < size) {
        if (_atLineStart && buffer[start] != '\r' && buffer[start] != '\n') {
            _atLineStart = false;
            printTimestamp();
        }

        size_t end = start;
        while (end < size && buffer[end] != '\n') {
            end++;
        }

        if (end < size && buffer[end] == '\n') {
            end++;
            _atLineStart = true;
        }

        size_t chunkSize = end - start;
        if (chunkSize > 0) {
            rawWrite(buffer + start, chunkSize);
        }
        start = end;
    }
    return size;
}

void NetworkConsole::broadcast(const char* str) {
    if (str) {
        write((const uint8_t*)str, strlen(str));
    }
}

bool NetworkConsole::hasTelnetClient() {
    return (bool)_telnetClient && (_telnetClient.connected() != 0);
}

static const char CMD_SEP[] = "--------------------------------------------------";

static bool isOpenCommand(const String& upper) {
    return upper == "HELP" || upper == "?" || upper == "STATUS";
}

// PASSWD, then a space or a tab: never reaches the log whatever the separator (FR-76)
static bool isPasswd(const String& upper) {
    return upper == "PASSWD" || (upper.startsWith("PASSWD") && isspace((unsigned char)upper[6]));
}

// A command that only reads or writes the log lines is not a security event (FR-77)
static bool isQuietCommand(const String& upper) {
    return isOpenCommand(upper) || upper.startsWith("LOG ") || upper.startsWith("MSG ") || upper.startsWith("ECHO ");
}

// FR-75.2: PASSWD <new> from Serial sets the first password; PASSWD <old> <new> changes it. The arguments never
// reach the log (FR-76): the answer says only what happened.
bool NetworkConsole::handlePasswd(const String& cmd, SecSource src, const uint8_t* ip) {
    String rest = cmd.substring(6);
    rest.trim();
    int sp = -1;
    for (unsigned i = 0; i < rest.length(); i++) {
        if (isspace((unsigned char)rest[i])) { sp = i; break; }
    }
    uint8_t none[4] = {0, 0, 0, 0};
    const uint8_t* addr = ip ? ip : none;
    if (rest.length() == 0) {
        this->println(F("[PASSWD] usage: PASSWD <new> (Serial, first password) or PASSWD <old> <new>"));
        return true;
    }
    if (sp < 0) {
#ifdef SH_DEV_OPEN
        const bool firstFromHere = true;  // FR-75.6: the first password may be set over the network, no forced change
#else
        const bool firstFromHere = (src == SEC_SERIAL);
#endif
        if (!firstFromHere) {
            this->println(F("[PASSWD] refused: give the old and the new password"));
        } else if (consoleAuth.passwordSet()) {
            this->println(F("[PASSWD] a password is set: use PASSWD <old> <new>"));
        } else if (consoleAuth.setInitial(rest.c_str(), src == SEC_SERIAL ? true : false)) {
            this->println(src == SEC_SERIAL ? F("[PASSWD] first password set; the first network login must change it with PASSWD <old> <new>") : F("[PASSWD] first password set"));
        } else {
            this->println(F("[PASSWD] refused: 8 to 63 characters"));
        }
        return true;
    }
    String oldPw = rest.substring(0, sp);
    String newPw = rest.substring(sp + 1);
    newPw.trim();
    AuthResult r = consoleAuth.change(oldPw.c_str(), newPw.c_str(), addr, src);
    if (r == AUTH_OK) this->println(F("[PASSWD] password changed"));
    else if (r == AUTH_LOCKED) this->println(F("[PASSWD] locked out, try later"));
    else if (r == AUTH_NOT_SET) this->println(F("[PASSWD] no password is set: set the first one from Serial"));
    else this->println(F("[PASSWD] refused: wrong old password, or the new one is equal to the old one or not 8 to 63 characters"));
    return true;
}

void NetworkConsole::executeCommand(const String& cmd, SecSource src, const uint8_t* ip) {
    String cleanCmd = cmd;
    cleanCmd.trim();
    if (cleanCmd.length() == 0) return;

    String upper = cleanCmd;
    upper.toUpperCase();

    // Passwords never reach the log: handled before the command is echoed
    if (isPasswd(upper)) {
        handlePasswd(cleanCmd, src, ip);
        return;
    }

    // 1. Instant buffer wipe without re-polluting with banner headers
    if (upper == "CLEAR" || upper == "CLS") {
        if (src != SEC_SERIAL) consoleAuth.addEvent(SEC_COMMAND, src, ip ? ip : (const uint8_t*)"\0\0\0\0");
        clear();
        return;
    }

    // 2. Direct log injection for test suites and system telemetry (no command framing)
    if (upper.startsWith("LOG ") || upper.startsWith("MSG ") || upper.startsWith("ECHO ")) {
        this->println(cleanCmd.substring(cleanCmd.indexOf(' ') + 1));
        return;
    }

    if (!isQuietCommand(upper)) {
        static const uint8_t none[4] = {0, 0, 0, 0};
        consoleAuth.addEvent(SEC_COMMAND, src, ip ? ip : none);
    }

    // The coordinates of the house are not echoed into the log (FR-76)
    String shown = cleanCmd;
    if (upper.startsWith("LOCATION ")) shown = "LOCATION <hidden>";

    this->println();
    this->println(CMD_SEP);
    this->printf("[CMD] > %s\n", shown.c_str());
    this->println(CMD_SEP);

    if (_commandHandler) {
        _commandHandler(cleanCmd);
    }

    this->println(CMD_SEP);
    this->printf("[CMD] < END: %s\n", shown.c_str());
    this->println(CMD_SEP);
    this->println();
}

void NetworkConsole::clear() {
    _head = 0;
    _count = 0;
    _totalBytes = 0;
    _atLineStart = true;
    memset(_logRing, 0, sizeof(_logRing));
    Serial.print("\033[2J\033[H");
    if (_telnetClient && _telnetClient.connected()) {
        _telnetClient.print("\033[2J\033[H");
    }
}

// One line typed on Telnet (FR-75.1): before the login only HELP and STATUS run; any other line is the password.
void NetworkConsole::telnetLine(const String& line) {
    String upper = line;
    upper.trim();
    upper.toUpperCase();
    uint8_t ip[4] = {0, 0, 0, 0};
    IPAddress rip = _telnetClient.remoteIP();
    for (int i = 0; i < 4; i++) ip[i] = rip[i];
    if (!_telnetAuth) {
        if (isOpenCommand(upper)) {
            executeCommand(line, SEC_TELNET, ip);
        } else if (!consoleAuth.passwordSet()) {
            _telnetClient.print("No console password is set: set the first one from Serial with PASSWD <new>.\r\n");
        } else {
            AuthResult r = consoleAuth.check(line.c_str(), ip, SEC_TELNET);
            if (r == AUTH_OK) {
                _telnetAuth = true;
                _telnetClient.print("Login ok.\r\n");
                if (consoleAuth.mustChange()) _telnetClient.print("Password change required: PASSWD <old> <new>\r\n");
            } else if (r == AUTH_LOCKED) {
                _telnetClient.print("Locked out. Connection closed.\r\n");
                _telnetClient.stop();
                return;
            } else {
                _telnetClient.print("Login failed.\r\n");
            }
        }
        return;
    }
    if (consoleAuth.mustChange() && !isPasswd(upper) && !isOpenCommand(upper)) {
        _telnetClient.print("Password change required: PASSWD <old> <new>\r\n");
        return;
    }
    executeCommand(line, SEC_TELNET, ip);
}

void NetworkConsole::update() {
    // 1. Manage Telnet incoming connections
    if (_telnetServer.hasClient()) {
        if (!_telnetClient || !_telnetClient.connected()) {
            if (_telnetClient) _telnetClient.stop();
            _telnetClient = _telnetServer.available();
            _telnetClient.print("\r\n==================================================\r\n");
            _telnetClient.print("  Smart Heating ESP32 - Virtual Serial Console\r\n");
            _telnetClient.print("==================================================\r\n");
#ifdef SH_DEV_OPEN
            _telnetClient.print("DEV - consola deschisa (FR-75.6): no login.\r\n> ");
            _telnetRxBuffer = "";
            _telnetAuth = true;
#else
            _telnetClient.print("HELP and STATUS need no login; any other command needs the console password.\r\n");
            _telnetClient.print("Password: ");
            _telnetRxBuffer = "";
            _telnetAuth = false;
#endif
            _telnetConnMs = millis();
        } else {
            WiFiClient rejected = _telnetServer.available();
            rejected.println("Console busy. Connection closed.");
            rejected.stop();
        }
    }

    // A login that is not finished in time closes the connection (FR-75.4)
    if (_telnetClient && _telnetClient.connected() && !_telnetAuth &&
        millis() - _telnetConnMs > (uint32_t)paramStore.get(Param::TelnetLoginTimeout) * 1000UL) {
        _telnetClient.print("\r\nLogin timeout. Connection closed.\r\n");
        _telnetClient.stop();
        return;
    }

    // 2. Read incoming characters from Telnet client
    if (_telnetClient && _telnetClient.connected()) {
        while (_telnetClient.available()) {
            char c = (char)_telnetClient.read();
            if (c == '\r' || c == '\n') {
                if (_telnetRxBuffer.length() > 0) {
                    _telnetClient.print("\r\n");
                    String cmd = _telnetRxBuffer;
                    _telnetRxBuffer = "";
                    telnetLine(cmd);
                    if (!(_telnetClient && _telnetClient.connected())) return;
                    _telnetClient.print(_telnetAuth ? "\r\n> " : "\r\nPassword: ");
                }
            } else if (c == 0x08 || c == 0x7F) { // Backspace
                if (_telnetRxBuffer.length() > 0) {
                    _telnetRxBuffer.remove(_telnetRxBuffer.length() - 1);
                    if (_telnetAuth) _telnetClient.print("\b \b");
                }
            } else if (c >= 32 && c <= 126) {
                if (_telnetRxBuffer.length() < 160) _telnetRxBuffer += c;
                if (_telnetAuth) _telnetClient.print(c); // Local echo only after the login: a password is never echoed
            }
        }
    }
}

void NetworkConsole::setupWebRoutes() {
    _webServer->on("/api/console/logs", HTTP_GET, std::bind(&NetworkConsole::handleApiConsoleLogs, this));
    _webServer->on("/api/console/send", HTTP_POST, std::bind(&NetworkConsole::handleApiConsoleSend, this));
    _webServer->on("/api/security/events", HTTP_GET, std::bind(&NetworkConsole::handleApiSecurityEvents, this));
}

String NetworkConsole::getLogsSince(uint32_t since) {
    size_t delta = 0;
    if (since == 0 || since > _totalBytes || (_totalBytes - since) >= _count) {
        delta = _count;
    } else if (since < _totalBytes) {
        delta = _totalBytes - since;
    }
    if (delta == 0) return "";

    String output;
    output.reserve(delta + 1);
    size_t startPos = (_head + LOG_BUFFER_SIZE - delta) % LOG_BUFFER_SIZE;
    if (startPos + delta <= LOG_BUFFER_SIZE) {
        output.concat((const char*)&_logRing[startPos], (unsigned int)delta);
    } else {
        size_t part1 = LOG_BUFFER_SIZE - startPos;
        size_t part2 = delta - part1;
        output.concat((const char*)&_logRing[startPos], (unsigned int)part1);
        output.concat((const char*)&_logRing[0], (unsigned int)part2);
    }
    return output;
}

static void clientIp(WebServer* ws, uint8_t out[4]) {
    IPAddress ip = ws->client().remoteIP();
    for (int i = 0; i < 4; i++) out[i] = ip[i];
}

static bool onSetupAp(WebServer* ws) {
    IPAddress local = ws->client().localIP();
    return WiFi.getMode() != WIFI_STA && local == WiFi.softAPIP();
}

static void sendUnauthorized(WebServer* ws, const char* msg) {
    ws->send(401, "application/json", String("{\"error\":\"") + msg + "\"}");
}

// FR-75.1: HTTP Basic, user `console`. A request without a password counts as no attempt (no event, no lockout).
bool NetworkConsole::httpAuthorized(bool mustChangeBlocks, const String& cmdUpper, bool recordOk) {
#ifdef SH_DEV_OPEN
    return true;  // FR-75.6: development profile, no login
#endif
    if (!consoleAuth.passwordSet()) {
        _webServer->send(401, "application/json", F("{\"error\":\"no console password set: set the first one from Serial with PASSWD <new>\"}"));
        return false;
    }
    String h = _webServer->header("Authorization");
    String user, pass;
    if (h.startsWith("Basic ")) {
        String b64 = h.substring(6);
        b64.trim();
        unsigned char out[96];
        size_t olen = 0;
        if (b64.length() < 120 &&
            mbedtls_base64_decode(out, sizeof(out) - 1, &olen, (const unsigned char*)b64.c_str(), b64.length()) == 0) {
            out[olen] = 0;
            String dec = String((const char*)out);
            int c = dec.indexOf(':');
            if (c >= 0) {
                user = dec.substring(0, c);
                pass = dec.substring(c + 1);
            }
        }
    }
    if (user.length() == 0 || pass.length() == 0) {
        sendUnauthorized(_webServer, "login required");
        return false;
    }
    uint8_t ip[4];
    clientIp(_webServer, ip);
    AuthResult r = AUTH_BAD;
    if (user == "console") r = consoleAuth.check(pass.c_str(), ip, SEC_HTTP, recordOk);
    else if (consoleAuth.locked(ip)) r = AUTH_LOCKED;
    else consoleAuth.registerFail(ip, SEC_HTTP);
    if (r == AUTH_LOCKED) {
        _webServer->send(429, "application/json", F("{\"error\":\"locked out, try later\"}"));
        return false;
    }
    if (r != AUTH_OK) {
        sendUnauthorized(_webServer, "wrong user or password");
        return false;
    }
    if (mustChangeBlocks && consoleAuth.mustChange() && !isPasswd(cmdUpper)) {
        _webServer->send(403, "application/json", F("{\"error\":\"password change required\"}"));
        return false;
    }
    return true;
}

void NetworkConsole::handleApiConsoleLogs() {
    if (onSetupAp(_webServer) && !httpAuthorized(false, "", false)) return;  // public only on the home network (FR-75.1)
    uint32_t since = _webServer->hasArg("since") ? (uint32_t)_webServer->arg("since").toInt() : 0;
    String output = getLogsSince(since);

    _webServer->sendHeader("X-Log-Seq", String(_totalBytes));
    _webServer->sendHeader("Access-Control-Expose-Headers", "X-Log-Seq");
    _webServer->send(200, "text/plain; charset=utf-8", output);
}

void NetworkConsole::handleApiSecurityEvents() {
    if (onSetupAp(_webServer) && !httpAuthorized(false, "", false)) return;
    _webServer->send(200, "application/json", consoleAuth.eventsJson());
}

void NetworkConsole::handleApiConsoleSend() {
    String cmd = "";
    if (_webServer->hasArg("cmd")) {
        cmd = _webServer->arg("cmd");
    } else if (_webServer->hasArg("c")) {
        cmd = _webServer->arg("c");
    } else if (_webServer->hasArg("plain")) {
        cmd = _webServer->arg("plain");
    }
    String upper = cmd;
    upper.trim();
    upper.toUpperCase();

    // FR-75.1: HELP and STATUS need no login, every other command does
    if (!isOpenCommand(upper) && !httpAuthorized(true, upper)) return;

    uint8_t ip[4];
    clientIp(_webServer, ip);
    uint32_t since = _webServer->hasArg("since") ? (uint32_t)_webServer->arg("since").toInt() : _totalBytes;

    executeCommand(cmd, SEC_HTTP, ip);

    String output = getLogsSince(since);

    _webServer->sendHeader("X-Log-Seq", String(_totalBytes));
    _webServer->sendHeader("Access-Control-Expose-Headers", "X-Log-Seq");
    _webServer->send(200, "text/plain; charset=utf-8", output);
}
