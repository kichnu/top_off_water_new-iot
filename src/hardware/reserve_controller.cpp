#include "reserve_controller.h"
#include "level_sensor.h"
#include "../core/logging.h"
#include "../config/reservoir_config.h"
#include "../algorithm/water_algorithm.h"

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
    LOG_WARNING("Reserve: no pump stop on an empty reservoir");
}

bool isReserveEmpty() {
    return false;
}

void clearReserveLatch() {
}
