#ifndef NETWORK_CONSOLE_H
#define NETWORK_CONSOLE_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <functional>

#define TELNET_PORT 23
#define LOG_BUFFER_SIZE 8192

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
    void handleWebConsolePage();
    void handleApiConsoleLogs();
    void handleApiConsoleSend();

    bool hasTelnetClient();
    void broadcast(const char* str);
    void executeCommand(const String& cmd);
    void clear();

private:
    WiFiServer _telnetServer;
    WiFiClient _telnetClient;
    WebServer* _webServer;
    CommandCallback _commandHandler;

    String _telnetRxBuffer;
    char _logRing[LOG_BUFFER_SIZE];
    size_t _head;
    size_t _count;
    uint32_t _totalBytes;
    bool _atLineStart;

    void appendToRing(char c);
    void printTimestamp();
    void rawWrite(const uint8_t *buffer, size_t size);
    void setupWebRoutes();
};

extern NetworkConsole netConsole;

#endif // NETWORK_CONSOLE_H
