#ifndef NETWORK_CONSOLE_H
#define NETWORK_CONSOLE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <functional>
#include "ConsoleAuth.h"

#define TELNET_PORT 23
#define LOG_BUFFER_SIZE 4096  // halved from 8192 to win heap (architecture 14.2, owner 01.11.2026)

typedef std::function<void(String)> CommandCallback;

class NetworkConsole : public Print {
public:
    NetworkConsole();
    void begin(WebServer* server = nullptr);
    void update();
    void setCommandHandler(CommandCallback cb);

    // Print interface implementation
    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    // Web handlers
    void handleApiConsoleLogs();
    void handleApiConsoleSend();
    void handleApiSecurityEvents();

    bool hasTelnetClient();
    void broadcast(const char* str);
    // `src` and `ip` say where the command came from (FR-75, FR-77); Serial needs no login.
    void executeCommand(const String& cmd, SecSource src = SEC_SERIAL, const uint8_t* ip = nullptr);
    void clear();
    String getLogsSince(uint32_t since);

private:
    WiFiServer _telnetServer;
    WiFiClient _telnetClient;
    WebServer* _webServer;
    CommandCallback _commandHandler;

    String _telnetRxBuffer;
    bool _telnetAuth;
    uint32_t _telnetConnMs;
    char _logRing[LOG_BUFFER_SIZE];
    size_t _head;
    size_t _count;
    uint32_t _totalBytes;
    bool _atLineStart;

    void appendToRing(char c);
    void appendToRing(const uint8_t *data, size_t size);
    void printTimestamp();
    void rawWrite(const uint8_t *buffer, size_t size);
    void setupWebRoutes();
    bool handlePasswd(const String& cmd, SecSource src, const uint8_t* ip);
    bool httpAuthorized(bool mustChangeBlocks, const String& cmdUpper, bool recordOk = true);
    void telnetLine(const String& line);
};

extern NetworkConsole netConsole;

#endif // NETWORK_CONSOLE_H
