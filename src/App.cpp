#include "App.h"
#include "BoilerRadioTrigger.h"
#include "ThermalZoneManager.h"
#include <esp_task_wdt.h>

TaskHandle_t App::_controlTaskHandle = nullptr;
TaskHandle_t App::_loopTaskHandle = nullptr;
volatile uint32_t App::_longestTickDelayMs = 0;
volatile uint32_t App::_longestLoopPassMs = 0;
BoilerRadioTrigger* App::_boiler = nullptr;
ThermalZoneManager* App::_zones = nullptr;

void App::init() {
    _loopTaskHandle = xTaskGetCurrentTaskHandle();
    _longestTickDelayMs = 0;
    _longestLoopPassMs = 0;
}

void App::startControlTask(BoilerRadioTrigger* boiler, ThermalZoneManager* zones) {
    _boiler = boiler;
    _zones = zones;

    // AR-21: Control task runs on Core 1, priority 3 (above loop() at priority 1)
    // 4096 B stack size, pinned to Core 1
    xTaskCreatePinnedToCore(
        controlTaskLoop,
        "ctl",
        4096,
        nullptr,
        3,
        &_controlTaskHandle,
        1
    );
}

void App::controlTaskLoop(void* param) {
    // Register control task with 90s TWDT (NFR-1.5, AR-21)
    esp_task_wdt_add(NULL);
    Serial.println(F("[App] Control task (AR-21) initialized on Core 1 (priority 3, stack 4 KB)."));

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000);
    uint32_t expectedWakeMs = millis();

    while (true) {
        expectedWakeMs += 1000;
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        uint32_t nowMs = millis();
        uint32_t delayMs = (nowMs > expectedWakeMs) ? (nowMs - expectedWakeMs) : 0;
        if (delayMs > _longestTickDelayMs) {
            _longestTickDelayMs = delayMs;
        }
        if (delayMs > 500) {
            Serial.printf("[App][WARN] Control tick delay %lu ms exceeded 500 ms bound!\n", (unsigned long)delayMs);
        }
        // Protect against cascading drift if delayed by > 1 period
        if (nowMs > expectedWakeMs + 1000) {
            expectedWakeMs = nowMs;
        }

        // Feed TWDT every tick (1s << 90s timeout)
        esp_task_wdt_reset();

        // Safety & hydronic control logic
        if (_boiler) {
            _boiler->update();
        }
        if (_zones && _boiler) {
            _zones->update(*_boiler);
        }
    }
}

void App::recordLoopPass(uint32_t durationMs) {
    if (durationMs > _longestLoopPassMs) {
        _longestLoopPassMs = durationMs;
    }
}

uint32_t App::getLongestTickDelayMs() {
    return _longestTickDelayMs;
}

uint32_t App::getLongestLoopPassMs() {
    return _longestLoopPassMs;
}

uint32_t App::getControlStackFree() {
    if (_controlTaskHandle) {
        return (uint32_t)uxTaskGetStackHighWaterMark(_controlTaskHandle);
    }
    TaskHandle_t h = xTaskGetHandle("ctl");
    if (h) {
        return (uint32_t)uxTaskGetStackHighWaterMark(h);
    }
    return 0;
}

uint32_t App::getLoopStackFree() {
    if (_loopTaskHandle) {
        return (uint32_t)uxTaskGetStackHighWaterMark(_loopTaskHandle);
    }
    TaskHandle_t h = xTaskGetHandle("loopTask");
    if (h) {
        return (uint32_t)uxTaskGetStackHighWaterMark(h);
    }
    return (uint32_t)uxTaskGetStackHighWaterMark(nullptr);
}

void App::feedLoopWatchdog() {
    esp_task_wdt_reset();
}

TaskHandle_t App::getControlTaskHandle() {
    return _controlTaskHandle;
}
