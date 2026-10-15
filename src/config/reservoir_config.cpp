#include "reservoir_config.h"
#include <math.h>
#include "../hardware/fram_controller.h"
#include "../hardware/level_sensor.h"
#include "../core/logging.h"

static ReservoirConfig g_cfg;
static bool            g_saved = false;

static void setDefaults(ReservoirConfig& c) {
    memset(&c, 0, sizeof(c));
    c.length_mm           = 282;
    c.width_mm            = 202;
    c.sensor_to_bottom_mm = 470;
    c.marker_level_mm     = 380;
    c.marker_tol_mm       = 5;
    c.portion_pause_s     = 30;
    c.flags               = RESERVOIR_FLAG_USE_SENSOR;
    c.magic               = RESERVOIR_CONFIG_MAGIC;
}

static const char* validate(const ReservoirConfig& c) {
    if (c.length_mm < RESERVOIR_DIM_MIN_MM || c.length_mm > RESERVOIR_DIM_MAX_MM ||
        c.width_mm  < RESERVOIR_DIM_MIN_MM || c.width_mm  > RESERVOIR_DIM_MAX_MM) {
        return "length/width: 50–2000 mm";
    }
    if (c.sensor_to_bottom_mm < RESERVOIR_MARKER_MIN_MM + LEVEL_MIN_CLEARANCE_MM ||
        c.sensor_to_bottom_mm > LEVEL_MAX_RANGE_MM) {
        return "sensor to bottom: 70–1200 mm (VL53L4CD range)";
    }
    if (c.marker_level_mm < RESERVOIR_MARKER_MIN_MM ||
        c.marker_level_mm > c.sensor_to_bottom_mm - LEVEL_MIN_CLEARANCE_MM) {
        return "marker level: 20 mm … (sensor to bottom − 50 mm) — sensor needs 50 mm clearance";
    }
    if (c.marker_tol_mm < RESERVOIR_TOL_MIN_MM || c.marker_tol_mm > RESERVOIR_TOL_MAX_MM) {
        return "marker tolerance: 1–30 mm";
    }
    if (c.portion_pause_s < PORTION_PAUSE_MIN_S || c.portion_pause_s > PORTION_PAUSE_MAX_S) {
        return "pause between portions: 15–120 s";
    }
    return nullptr;
}

void initReservoirConfig() {
    if (loadReservoirConfigFromFRAM(g_cfg) && validate(g_cfg) == nullptr) {
        g_saved = true;
        LOG_INFO("Reservoir config loaded: %ux%u mm, sensor→bottom %u mm, marker %u mm",
                 g_cfg.length_mm, g_cfg.width_mm, g_cfg.sensor_to_bottom_mm, g_cfg.marker_level_mm);
    } else {
        setDefaults(g_cfg);
        g_saved = false;
        LOG_WARNING("Reservoir config not in FRAM — using defaults");
    }
}

const ReservoirConfig& getReservoirConfig() { return g_cfg; }
bool isReservoirConfigSaved()               { return g_saved; }

const char* setReservoirConfig(const ReservoirConfig& cfg) {
    const char* err = validate(cfg);
    if (err) return err;

    ReservoirConfig c = cfg;
    c.magic = RESERVOIR_CONFIG_MAGIC;
    memcpy(c._reserved, g_cfg._reserved, sizeof(c._reserved));   // not editable here
    if (!saveReservoirConfigToFRAM(c)) return "FRAM write failed";

    g_cfg   = c;
    g_saved = true;
    LOG_INFO("Reservoir config saved: %ux%u mm, sensor→bottom %u mm, marker %u mm ±%u, pause %u s, level sensor %s",
             c.length_mm, c.width_mm, c.sensor_to_bottom_mm, c.marker_level_mm, c.marker_tol_mm,
             c.portion_pause_s, (c.flags & RESERVOIR_FLAG_USE_SENSOR) ? "on" : "off (portion counting)");
    return nullptr;
}

const char* reservoirConfigWarning(const ReservoirConfig& c) {
    // Footprint of the sensor's field of view on the bottom (empty tank) vs the
    // shorter side — wider footprint may pick up the walls at low levels
    float footprint = 2.0f * c.sensor_to_bottom_mm * tanf(LEVEL_FOV_DEG * 0.5f * (float)M_PI / 180.0f);
    uint16_t shorter = min(c.length_mm, c.width_mm);
    if (footprint > shorter) {
        return "sensor field of view at the bottom is wider than the tank — readings near empty may hit the walls";
    }
    return nullptr;
}

float reservoirMlPerMm() {
    return g_cfg.length_mm * (float)g_cfg.width_mm / 1000.0f;
}

uint32_t reservoirCapacityMl() {
    return (uint32_t)lroundf(reservoirMlPerMm() * g_cfg.marker_level_mm);
}

// ceil(ml × pct / 100) rounded up to a whole number of portions
static uint32_t pctUpToPortions(uint32_t ml, uint32_t pct, uint32_t dose) {
    uint32_t den = 100u * dose;
    return (ml * pct + den - 1) / den * dose;
}

ReservoirLevels reservoirLevels(uint16_t doseMl) {
    uint32_t dose = doseMl ? doseMl : 1;
    ReservoirLevels l;
    l.fullMl   = reservoirCapacityMl() / dose * dose;
    l.hiddenMl = pctUpToPortions(l.fullMl, RESERVE_HIDDEN_PCT_FIXED, dose);
    l.usableMl = l.fullMl - l.hiddenMl;
    l.lowMl    = pctUpToPortions(l.usableMl, RESERVE_LOW_PCT_FIXED, dose);
    return l;
}

bool useLevelSensor() {
    return g_cfg.flags & RESERVOIR_FLAG_USE_SENSOR;
}
