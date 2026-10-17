#include "reserve_controller.h"
#include "hardware_pins.h"
#include "level_sensor.h"
#include "../core/logging.h"
#include "../config/reservoir_config.h"
#include "../algorithm/water_algorithm.h"

static bool     g_floatLow     = false;   // debounced: no water
static bool     g_floatLatched = false;   // was low — cleared by a reset with water back
#if RESERVE_FLOAT_ENABLED
static uint32_t g_floatLowSince = 0;      // millis() of the first raw LOW, 0 = HIGH
#endif

static uint16_t currentDose() {
    uint16_t d = waterAlgorithm.getConfig().dose_ml;
    return d ? d : 1;
}

ReserveView getReserveView(uint16_t doseMl) {
    ReserveView v = {};
    uint32_t dose = doseMl ? doseMl : 1;
    const ReservoirLevels lv = reservoirLevels(dose);

    v.capacityMl       = lv.usableMl;
    v.lowMl            = lv.lowMl;
    v.capacityPortions = lv.usableMl / dose;

    const LevelSeries& ls = getLastLevelSeries();
    v.reading = isReservoirConfigSaved() && getLevelSensorState() == LEVEL_SENSOR_OK &&
                ls.valid && ls.finished_ms != 0;
    if (!v.reading) {
        v.zone = RESERVE_ZONE_UNKNOWN;
        return v;
    }

    float h = getReservoirConfig().sensor_to_bottom_mm - ls.distance_mm;
    v.waterMm = h > 0 ? h : 0;
    float total = v.waterMm * reservoirMlPerMm();
    v.ml       = total > lv.hiddenMl ? (uint32_t)(total - lv.hiddenMl) : 0;
    v.portions = v.ml / dose;
    v.pct      = v.capacityMl ? v.ml * 100.0f / v.capacityMl : 0.0f;

    if      (v.ml < dose)     v.zone = RESERVE_ZONE_EMPTY;
    else if (v.ml <= v.lowMl) v.zone = RESERVE_ZONE_LOW;
    else                      v.zone = RESERVE_ZONE_OK;
    return v;
}

bool isReserveLow() {
    ReserveZone z = getReserveView(currentDose()).zone;
    return z == RESERVE_ZONE_LOW || z == RESERVE_ZONE_EMPTY;
}

void initReserveController() {
#if RESERVE_FLOAT_ENABLED
    pinMode(RESERVOIR_FLOAT_PIN, INPUT_PULLUP);
    LOG_INFO("Reserve float switch on GPIO %d: %s", RESERVOIR_FLOAT_PIN,
             digitalRead(RESERVOIR_FLOAT_PIN) == LOW ? "LOW (no water)" : "HIGH");
#else
    LOG_WARNING("Reserve float switch DISABLED (RESERVE_FLOAT_ENABLED = 0) — no pump stop on an empty reservoir");
#endif
}

void updateReserveController() {
#if RESERVE_FLOAT_ENABLED
    if (digitalRead(RESERVOIR_FLOAT_PIN) == LOW) {
        if (g_floatLowSince == 0) g_floatLowSince = millis() | 1u;   // |1 — 0 means HIGH
        if (!g_floatLow && millis() - g_floatLowSince >= RESERVE_FLOAT_DEBOUNCE_MS) {
            g_floatLow     = true;
            g_floatLatched = true;
            LOG_WARNING("Reserve float switch: no water — new portions blocked");
        }
    } else {
        if (g_floatLow) LOG_INFO("Reserve float switch: water back — reset to clear the alarm");
        g_floatLowSince = 0;
        g_floatLow      = false;
    }
#endif
}

bool isReserveEmpty() {
    return g_floatLow || g_floatLatched;
}

void clearReserveLatch() {
    if (g_floatLatched && !g_floatLow) {
        g_floatLatched = false;
        LOG_INFO("Reserve float alarm cleared");
    }
}

const char* getReserveFloatString() {
#if RESERVE_FLOAT_ENABLED
    return g_floatLow ? "low" : g_floatLatched ? "latched" : "ok";
#else
    return "disabled";
#endif
}
