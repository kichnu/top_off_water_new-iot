#include "reserve_controller.h"
#include "hardware_pins.h"
#include "fram_controller.h"
#include "../core/logging.h"
#include "driver/gpio.h"

static uint32_t g_reserveConfigMl  = RESERVE_ML_DEFAULT;
static uint32_t g_reserveMl        = RESERVE_ML_DEFAULT;
static bool     g_sensorLow        = false;
static bool     g_sensorWasLow     = false;  // latch: sensor był LOW; kasowany przez clearReserveAlarm()
static uint32_t g_sensorLowSince   = 0;      // millis() pierwszego raw-LOW; 0 = pin jest HIGH

#define RESERVE_DEBOUNCE_MS  3000u

void initReserveController() {
    // GPIO reset przed pinMode — rozwiązuje problem z SDIO_DATA2 na ESP32-C6
    gpio_reset_pin((gpio_num_t)AVAILABLE_WATER_SENSOR_PIN);
    pinMode(AVAILABLE_WATER_SENSOR_PIN, INPUT_PULLUP);

    uint32_t cfg = 0, cur = 0;
    if (loadReserveFromFRAM(cfg, cur)) {
        g_reserveConfigMl = cfg;
        g_reserveMl       = cur;
    } else {
        g_reserveConfigMl = RESERVE_ML_DEFAULT;
        g_reserveMl       = RESERVE_ML_DEFAULT;
        saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    }

    bool rawLow = (digitalRead(AVAILABLE_WATER_SENSOR_PIN) == LOW);
    g_sensorLowSince = rawLow ? (millis() | 1u) : 0u;
    g_sensorLow      = false;  // debounce nie potwierdzone przy init

    LOG_INFO("Reserve controller init: config=%lu ml, current=%lu ml, raw_sensor=%s",
             g_reserveConfigMl, g_reserveMl, rawLow ? "LOW" : "HIGH");
}

void updateReserveSensor() {
    bool rawLow = (digitalRead(AVAILABLE_WATER_SENSOR_PIN) == LOW);

    if (rawLow) {
        if (g_sensorLowSince == 0)
            g_sensorLowSince = millis() | 1u;  // |1 żeby unikać wartości 0
        if (!g_sensorLow && (millis() - g_sensorLowSince >= RESERVE_DEBOUNCE_MS)) {
            g_sensorLow    = true;
            g_sensorWasLow = true;  // latch — zwalniany tylko przez clearReserveAlarm()
            LOG_WARNING("Reserve sensor: debounce confirmed LOW (after %lu ms)", millis() - g_sensorLowSince);
        }
    } else {
        g_sensorLowSince = 0;
        g_sensorLow      = false;
        // g_sensorWasLow NIE kasowany — latch działa tak samo jak dotąd
    }
}

bool isReserveSensorLow()  { return g_sensorLow; }
bool isReserveNeedsReset() { return g_sensorWasLow && !g_sensorLow; }
uint32_t getReserveMl()       { return g_reserveMl; }
uint32_t getReserveConfigMl() { return g_reserveConfigMl; }
bool     isReserveEmpty()     { return g_reserveMl == 0; }

void subtractReserveMl(uint16_t ml) {
    if (ml == 0) return;
    if (ml >= g_reserveMl) {
        g_reserveMl = 0;
    } else {
        g_reserveMl -= ml;
    }
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve: -%d ml → %lu ml remaining", ml, g_reserveMl);
}

void setReserveConfigMl(uint32_t ml) {
    if (ml < RESERVE_ML_MIN) ml = RESERVE_ML_MIN;
    if (ml > RESERVE_ML_MAX) ml = RESERVE_ML_MAX;
    g_reserveConfigMl = ml;
    g_reserveMl       = ml;
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve config set: %lu ml", ml);
}

void clearReserveAlarm() {
    g_reserveMl    = g_reserveConfigMl;
    g_sensorWasLow = false;
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve alarm cleared — reset to %lu ml", g_reserveMl);
}
