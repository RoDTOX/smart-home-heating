#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declarations
class BoilerRadioTrigger;
class ThermalZoneManager;

// Shared between main.cpp, WebApi.cpp and ConsoleCommands.cpp
extern WebServer server;

void blinkLed(uint8_t count = 2, uint16_t delayMs = 60);
const char* getResetReasonStr(esp_reset_reason_t reason);

void registerWebRoutes();             // WebApi.cpp
void handleSerialCommand(String cmd); // ConsoleCommands.cpp

// Module 1: App (AR-21 ControlTask, FR-23 timing metrics, NFR-1.5 task watchdog)
class App {
public:
    static void init();
    static void startControlTask(BoilerRadioTrigger* boiler, ThermalZoneManager* zones);

    static void recordLoopPass(uint32_t durationMs);
    static uint32_t getLongestTickDelayMs();
    static uint32_t getLongestLoopPassMs();

    static uint32_t getControlStackFree();
    static uint32_t getLoopStackFree();

    static void feedLoopWatchdog();
    static TaskHandle_t getControlTaskHandle();

private:
    static void controlTaskLoop(void* param);

    static TaskHandle_t _controlTaskHandle;
    static TaskHandle_t _loopTaskHandle;
    static volatile uint32_t _longestTickDelayMs;
    static volatile uint32_t _longestLoopPassMs;
    static BoilerRadioTrigger* _boiler;
    static ThermalZoneManager* _zones;
};

#endif // APP_H
