#include "buzzer_controller.h"
#include "hardware_pins.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "driver/gpio.h"

static volatile BuzzerMode g_mode = BUZZER_OFF;
static TaskHandle_t g_taskHandle = nullptr;

static void buzzerTask(void*) {
    TickType_t xLastWake = xTaskGetTickCount();
    uint16_t   tick      = 0;
    BuzzerMode lastMode  = BUZZER_OFF;

    while (true) {
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(BUZZER_TICK_MS));

        // Mode change restarts the cycle — a new alarm sounds immediately
        BuzzerMode mode = g_mode;
        if (mode != lastMode) {
            tick     = 0;
            lastMode = mode;
        }
        uint16_t cycleTicks = (mode == BUZZER_ALARM ? BUZZER_ALARM_CYCLE_MS : BUZZER_CYCLE_MS)
                              / BUZZER_TICK_MS;

        bool on = false;
        switch (mode) {
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

        digitalWrite(BUZZER_PIN, on ? BUZZER_LEVEL_ON : BUZZER_LEVEL_OFF);

        if (++tick >= cycleTicks) tick = 0;
    }
}

void initBuzzer() {
    // GPIO 7 = JTAG MTDO on ESP32-C3: gpio_reset_pin() disconnects IO matrix before pinMode
    gpio_reset_pin((gpio_num_t)BUZZER_PIN);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, BUZZER_LEVEL_OFF);
    xTaskCreate(buzzerTask, "buzzer", 1024, nullptr, 1, &g_taskHandle);
}

void setBuzzerMode(BuzzerMode mode) {
    g_mode = mode;
}

// ---- Blocking boot signals ----

static void beep(uint32_t ms) {
    if (g_taskHandle) vTaskSuspend(g_taskHandle);
    digitalWrite(BUZZER_PIN, BUZZER_LEVEL_ON);
    delay(ms);
    digitalWrite(BUZZER_PIN, BUZZER_LEVEL_OFF);
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

void buzzerConfirm() {
    beep(80);
    delay(80);
    beep(80);
}

void buzzerCritical() {
    for (int i = 0; i < 4; i++) {
        beep(250);
        if (i < 3) delay(100);
    }
}
