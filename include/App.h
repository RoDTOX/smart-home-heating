#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>

// Shared between main.cpp, WebApi.cpp and ConsoleCommands.cpp
extern WebServer server;

void blinkLed(uint8_t count = 2, uint16_t delayMs = 60);
const char* getResetReasonStr(esp_reset_reason_t reason);

void registerWebRoutes();             // WebApi.cpp
void handleSerialCommand(String cmd); // ConsoleCommands.cpp

#endif // APP_H
