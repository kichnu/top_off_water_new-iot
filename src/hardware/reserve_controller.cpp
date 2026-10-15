#include "reserve_controller.h"
#include <math.h>
#include "fram_controller.h"
#include "level_sensor.h"
#include "../config/reservoir_config.h"
#include "../core/logging.h"

static uint32_t g_reserveConfigMl  = RESERVE_ML_DEFAULT;
static uint32_t g_reserveMl        = RESERVE_ML_DEFAULT;

void initReserveController() {
    uint32_t cfg = 0, cur = 0;
    if (loadReserveFromFRAM(cfg, cur)) {
        g_reserveConfigMl = cfg;
        g_reserveMl       = cur;
    } else {
        g_reserveConfigMl = RESERVE_ML_DEFAULT;
        g_reserveMl       = RESERVE_ML_DEFAULT;
        saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    }

    LOG_INFO("Reserve controller init: config=%lu ml, current=%lu ml",
             g_reserveConfigMl, g_reserveMl);
}

uint32_t getReserveMl()       { return g_reserveMl; }
uint32_t getReserveConfigMl() { return g_reserveConfigMl; }
bool     isReserveEmpty()     { return g_reserveMl == 0; }

bool isReserveLow() {
    return g_reserveMl > 0 && g_reserveMl * 100u <= g_reserveConfigMl * RESERVE_WARN_PCT;
}

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
    g_reserveMl = g_reserveConfigMl;
    saveReserveToFRAM(g_reserveConfigMl, g_reserveMl);
    LOG_INFO("Reserve refilled — reset to %lu ml", g_reserveMl);
}

ReserveView getReserveView(uint16_t doseMl) {
    ReserveView v = {};
    v.fromSensor = useLevelSensor();
    uint32_t dose = doseMl ? doseMl : 1;

    if (v.fromSensor) {
        // The hidden safety bottom is cut off: 0 usable ml = empty
        const ReservoirLevels lv = reservoirLevels(dose);
        const LevelSeries& ls = getLastLevelSeries();
        v.capacityMl = lv.usableMl;
        v.lowMl      = lv.lowMl;
        v.known = getLevelSensorState() == LEVEL_SENSOR_OK && ls.valid && ls.finished_ms != 0;
        if (v.known) {
            float h = getReservoirConfig().sensor_to_bottom_mm - ls.distance_mm;
            if (h < 0) h = 0;
            v.waterMm = h;
            uint32_t raw = (uint32_t)lroundf(h * reservoirMlPerMm());
            v.ml = raw > lv.hiddenMl ? raw - lv.hiddenMl : 0;
        }
    } else {
        // Counter fallback — capacity is still the configured ml volume until the
        // counter switches to the geometry capacity (portion counting)
        v.capacityMl = g_reserveConfigMl;
        v.lowMl      = (g_reserveConfigMl * RESERVE_LOW_PCT_FIXED + 100u * dose - 1) / (100u * dose) * dose;
        v.ml         = g_reserveMl;
        v.known      = true;
    }

    if (!v.known) {
        v.zone = RESERVE_ZONE_UNKNOWN;
        return v;
    }
    v.pct = v.capacityMl ? v.ml * 100.0f / v.capacityMl : 0.0f;
    if      (v.ml == 0)       v.zone = RESERVE_ZONE_EMPTY;
    else if (v.ml <= v.lowMl) v.zone = RESERVE_ZONE_LOW;
    else                      v.zone = RESERVE_ZONE_OK;
    return v;
}
