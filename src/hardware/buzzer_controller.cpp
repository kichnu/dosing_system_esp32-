#include "buzzer_controller.h"
#include "../config/config.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Parametry taska
static constexpr uint32_t TICK_MS     = 50;
static constexpr uint32_t CYCLE_TICKS = 12000 / TICK_MS;  // 240 ticków = 12s cykl
static constexpr uint32_t ALARM_TICK2 = 3;                 // drugi pik w ALARM: tick 3 = 150ms

volatile BuzzerMode g_mode = BuzzerMode::OFF;
static TaskHandle_t s_taskHandle = nullptr;

static void buzzerTask(void*) {
    TickType_t lastWake = xTaskGetTickCount();
    uint32_t tick = 0;
    for (;;) {
        bool on = false;
        switch (g_mode) {
            case BuzzerMode::WARNING: on = (tick == 0); break;
            case BuzzerMode::ALARM:   on = (tick == 0) || (tick == ALARM_TICK2); break;
            default: break;
        }
        digitalWrite(BUZZER_PIN, on ? BUZZER_ACTIVE : BUZZER_INACTIVE);
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(TICK_MS));
        tick = (tick + 1) % CYCLE_TICKS;
    }
}

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, BUZZER_INACTIVE);
    xTaskCreate(buzzerTask, "buzzer", 1024, nullptr, 1, &s_taskHandle);
}

void setBuzzerMode(BuzzerMode mode) {
    g_mode = mode;
}

// Wspólna prymitywna funkcja beep — wstrzymuje task na czas odtwarzania
static void _beep(uint32_t onMs, uint32_t offMs, int count) {
    if (s_taskHandle) vTaskSuspend(s_taskHandle);
    digitalWrite(BUZZER_PIN, BUZZER_INACTIVE);
    for (int i = 0; i < count; i++) {
        digitalWrite(BUZZER_PIN, BUZZER_ACTIVE);
        delay(onMs);
        digitalWrite(BUZZER_PIN, BUZZER_INACTIVE);
        if (i < count - 1) delay(offMs);
    }
    if (s_taskHandle) vTaskResume(s_taskHandle);
}

void buzzerPowerOn() {
    _beep(300, 0, 1);
}

void buzzerOK() {
    _beep(100, 100, 2);
}

void buzzerCritical() {
    _beep(50, 50, 3);
}
