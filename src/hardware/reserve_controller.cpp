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

    v.capacityMl       = lv.fullMl;
    v.reserveMl        = lv.reserveMl;
    v.alarmMl          = lv.alarmMl;
    v.capacityPortions = (lv.fullMl - lv.alarmMl) / dose;

    const LevelSeries& ls = getLastLevelSeries();
    v.reading = isReservoirConfigSaved() && getLevelSensorState() == LEVEL_SENSOR_OK &&
                ls.valid && ls.finished_ms != 0;
    if (!v.reading) {
        v.zone = RESERVE_ZONE_UNKNOWN;
        return v;
    }

    float h = getReservoirConfig().sensor_to_bottom_mm - ls.distance_mm;
    v.waterMm  = h > 0 ? h : 0;
    v.ml       = (uint32_t)(v.waterMm * reservoirMlPerMm());
    v.portions = v.ml > lv.alarmMl ? (v.ml - lv.alarmMl) / dose : 0;
    v.pct      = v.capacityMl ? v.ml * 100.0f / v.capacityMl : 0.0f;

    if      (v.ml <= lv.alarmMl)   v.zone = RESERVE_ZONE_ALARM;
    else if (v.ml <= lv.reserveMl) v.zone = RESERVE_ZONE_RESERVE;
    else                           v.zone = RESERVE_ZONE_OK;
    return v;
}

bool isReserveLow() {
    ReserveZone z = getReserveView(currentDose()).zone;
    return z == RESERVE_ZONE_RESERVE || z == RESERVE_ZONE_ALARM;
}

void initReserveController() {
    LOG_WARNING("Reserve: no pump stop on an empty reservoir");
}

bool isReserveEmpty() {
    return false;
}

void clearReserveLatch() {
}
