#include "NetworkConsole.h"
#include "console_html_gz.h"
#include <LittleFS.h>
#include <time.h>

NetworkConsole netConsole;

NetworkConsole::NetworkConsole()
    : _telnetServer(TELNET_PORT),
      _webServer(nullptr),
      _commandHandler(nullptr),
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

void NetworkConsole::flushRepeatedCount() {
    if (_repeatCount > 0) {
        if (_count > 0) {
            printTimestamp();
            char repMsg[64];
            snprintf(repMsg, sizeof(repMsg), "[Last message repeated %u times]\r\n", _repeatCount);
            rawWrite((const uint8_t*)repMsg, strlen(repMsg));
        }
        _repeatCount = 0;
        _lastRepeatFlushMs = millis();
    }
}

void NetworkConsole::processCompletedLine() {
    _lineAccum[_lineAccumLen] = '\0';

    if (_lineAccumLen == 0) {
        flushRepeatedCount();
        rawWrite((const uint8_t*)"\r\n", 2);
        _prevLine[0] = '\0';
        return;
    }

    if (_prevLine[0] != '\0' && strcmp(_lineAccum, _prevLine) == 0) {
        _repeatCount++;
        if (_repeatCount >= 1000 || (millis() - _lastRepeatFlushMs >= 1800000UL)) {
            flushRepeatedCount();
        }
    } else {
        flushRepeatedCount();
        strncpy(_prevLine, _lineAccum, sizeof(_prevLine) - 1);
        _prevLine[sizeof(_prevLine) - 1] = '\0';
        _lastRepeatFlushMs = millis();

        printTimestamp();
        rawWrite((const uint8_t*)_lineAccum, _lineAccumLen);
        rawWrite((const uint8_t*)"\r\n", 2);
    }

    _lineAccumLen = 0;
}

size_t NetworkConsole::write(uint8_t c) {
    return write(&c, 1);
}

size_t NetworkConsole::write(const uint8_t *buffer, size_t size) {
    if (!buffer || size == 0) return 0;
    _lastCharMs = millis();
    for (size_t i = 0; i < size; i++) {
        char ch = (char)buffer[i];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            processCompletedLine();
        } else {
            if (_lineAccumLen < sizeof(_lineAccum) - 1) {
                _lineAccum[_lineAccumLen++] = ch;
            } else {
                processCompletedLine();
                _lineAccum[_lineAccumLen++] = ch;
            }
        }
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

void NetworkConsole::executeCommand(const String& cmd) {

    flushRepeatedCount();
    String cleanCmd = cmd;
    cleanCmd.trim();
    if (cleanCmd.length() == 0) return;

    String upper = cleanCmd;
    upper.toUpperCase();

    // 1. Instant buffer wipe without re-polluting with banner headers
    if (upper == "CLEAR" || upper == "CLS") {
        clear();
        return;
    }

    // 2. Direct log injection for test suites and system telemetry (no command framing)
    if (upper.startsWith("LOG ") || upper.startsWith("MSG ") || upper.startsWith("ECHO ")) {
        this->println(cleanCmd.substring(cleanCmd.indexOf(' ') + 1));
        return;
    }

    this->println();
    this->println(CMD_SEP);
    this->printf("[CMD] > %s\n", cleanCmd.c_str());
    this->println(CMD_SEP);

    if (_commandHandler) {
        _commandHandler(cleanCmd);
    }

    this->println(CMD_SEP);
    this->printf("[CMD] < END: %s\n", cleanCmd.c_str());
    this->println(CMD_SEP);
    this->println();
}

void NetworkConsole::clear() {
    _head = 0;
    _count = 0;
    _totalBytes = 0;
    _atLineStart = true;
    _repeatCount = 0;
    _prevLine[0] = '\0';
    memset(_logRing, 0, sizeof(_logRing));
    Serial.print("\033[2J\033[H");
    if (_telnetClient && _telnetClient.connected()) {
        _telnetClient.print("\033[2J\033[H");
    }
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
            _telnetClient.print("Type 'HELP' or '?' for available commands.\r\n\r\n> ");
            _telnetRxBuffer = "";
        } else {
            WiFiClient rejected = _telnetServer.available();
            rejected.println("Console busy. Connection closed.");
            rejected.stop();
        }
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
                    executeCommand(cmd);
                    _telnetClient.print("\r\n> ");
                }
            } else if (c == 0x08 || c == 0x7F) { // Backspace
                if (_telnetRxBuffer.length() > 0) {
                    _telnetRxBuffer.remove(_telnetRxBuffer.length() - 1);
                    _telnetClient.print("\b \b");
                }
            } else if (c >= 32 && c <= 126) {
                _telnetRxBuffer += c;
                _telnetClient.print(c); // Local echo
            }
        }
    }
}

void NetworkConsole::setupWebRoutes() {
    _webServer->on("/console", HTTP_GET, std::bind(&NetworkConsole::handleWebConsolePage, this));
    _webServer->on("/api/console/logs", HTTP_GET, std::bind(&NetworkConsole::handleApiConsoleLogs, this));
    _webServer->on("/api/console/send", HTTP_POST, std::bind(&NetworkConsole::handleApiConsoleSend, this));
    _webServer->on("/api/console/cmd", HTTP_POST, std::bind(&NetworkConsole::handleApiConsoleSend, this));
}

String NetworkConsole::getLogsSince(uint32_t since) {

    flushRepeatedCount();
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

void NetworkConsole::handleApiConsoleLogs() {
    uint32_t since = _webServer->hasArg("since") ? (uint32_t)_webServer->arg("since").toInt() : 0;
    String output = getLogsSince(since);

    _webServer->sendHeader("X-Log-Seq", String(_totalBytes));
    _webServer->sendHeader("Access-Control-Expose-Headers", "X-Log-Seq");
    _webServer->sendHeader("Access-Control-Allow-Origin", "*");
    _webServer->send(200, "text/plain; charset=utf-8", output);
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

    uint32_t since = _webServer->hasArg("since") ? (uint32_t)_webServer->arg("since").toInt() : _totalBytes;

    executeCommand(cmd);

    String output = getLogsSince(since);

    _webServer->sendHeader("X-Log-Seq", String(_totalBytes));
    _webServer->sendHeader("Access-Control-Expose-Headers", "X-Log-Seq");
    _webServer->sendHeader("Access-Control-Allow-Origin", "*");
    _webServer->send(200, "text/plain; charset=utf-8", output);
}

void NetworkConsole::handleWebConsolePage() {
    _webServer->sendHeader("Access-Control-Allow-Origin", "*");
    _webServer->sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    _webServer->sendHeader("Pragma", "no-cache");
    if (LittleFS.exists("/console.html.gz")) {
        File f = LittleFS.open("/console.html.gz", "r");
        if (f) {
            _webServer->streamFile(f, "text/html");
            f.close();
            return;
        }
    }
    _webServer->sendHeader("Content-Encoding", "gzip");
    _webServer->send_P(200, "text/html", (const char*)CONSOLE_HTML_GZ, CONSOLE_HTML_GZ_LEN);
}
