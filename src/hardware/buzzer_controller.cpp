#include "buzzer_controller.h"
#include "hardware_pins.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static volatile BuzzerMode g_mode = BUZZER_OFF;
static TaskHandle_t g_taskHandle = nullptr;

static void buzzerTask(void*) {
    TickType_t xLastWake = xTaskGetTickCount();
    uint8_t tick = 0;

    while (true) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BUZZER_TICK_MS));

        bool on = false;
        switch (g_mode) {
            case BUZZER_PROVISIONING:
            case BUZZER_WARNING:
                on = (tick == 0);
                break;
            case BUZZER_ALARM:
                on = (tick == 0) || (tick == BUZZER_ALARM_TICK2);
                break;
            default:
                break;
        }

        digitalWrite(BUZZER_PIN, on ? LOW : HIGH);

        if (++tick >= BUZZER_CYCLE_TICKS) tick = 0;
    }
}

void initBuzzer() {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);  // HIGH = off (active LOW)
    xTaskCreate(buzzerTask, "buzzer", 1024, nullptr, 1, &g_taskHandle);
}

void setBuzzerMode(BuzzerMode mode) {
    g_mode = mode;
}

// ---- Blokujące sygnały bootowe ----

static void beep(uint32_t ms) {
    if (g_taskHandle) vTaskSuspend(g_taskHandle);
    digitalWrite(BUZZER_PIN, LOW);
    delay(ms);
    digitalWrite(BUZZER_PIN, HIGH);
    if (g_taskHandle) vTaskResume(g_taskHandle);
}

void buzzerPowerOn() {
    beep(1000);
}

void buzzerOK() {
    beep(100);
    delay(150);
    beep(500);
}

void buzzerCritical() {
    for (int i = 0; i < 4; i++) {
        beep(250);
        if (i < 3) delay(100);
    }
}
