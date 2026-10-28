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
    c.portion_pause_s     = 30;
    c.magic               = RESERVOIR_CONFIG_MAGIC;
}

static const char* validateGeometry(const ReservoirConfig& c) {
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
    if (c.portion_pause_s < PORTION_PAUSE_MIN_S || c.portion_pause_s > PORTION_PAUSE_MAX_S) {
        return "pause between portions: 15–120 s";
    }
    return nullptr;
}

static const char* validate(const ReservoirConfig& c, uint16_t doseMl) {
    const char* err = validateGeometry(c);
    if (err) return err;
    uint8_t a = alarmPct(c), r = reservePct(c);
    if (a < ALARM_PCT_MIN || a > ALARM_PCT_MAX) return "alarm level: 5–50 %";
    if (r <= a || r > RESERVE_PCT_MAX)          return "reserve level: above the alarm level, max 90 %";
    uint32_t dose = doseMl ? doseMl : 1;
    ReservoirLevels lv = reservoirLevels(c, doseMl);
    if (lv.reserveMl < lv.alarmMl + RESERVE_MIN_GAP_PORTIONS * dose) {
        return "reserve level must be at least 2 portions above the alarm level";
    }
    return nullptr;
}

void initReservoirConfig() {
    // Geometry only: a reserve/alarm pair that no longer fits a changed portion
    // stays as saved (the Settings save checks it)
    if (loadReservoirConfigFromFRAM(g_cfg) && validateGeometry(g_cfg) == nullptr) {
        g_saved = true;
        LOG_INFO("Reservoir config loaded: %ux%u mm, sensor→bottom %u mm, marker %u mm, reserve %u%%, alarm %u%%",
                 g_cfg.length_mm, g_cfg.width_mm, g_cfg.sensor_to_bottom_mm, g_cfg.marker_level_mm,
                 reservePct(g_cfg), alarmPct(g_cfg));
    } else {
        setDefaults(g_cfg);
        g_saved = false;
        LOG_WARNING("Reservoir config not in FRAM — using defaults");
    }
}

const ReservoirConfig& getReservoirConfig() { return g_cfg; }
bool isReservoirConfigSaved()               { return g_saved; }

const char* setReservoirConfig(const ReservoirConfig& cfg, uint16_t doseMl) {
    const char* err = validate(cfg, doseMl);
    if (err) return err;

    ReservoirConfig c = cfg;
    c.magic = RESERVOIR_CONFIG_MAGIC;
    c.cal = g_cfg.cal;                                          // not editable here
    memcpy(c._reserved, g_cfg._reserved, sizeof(c._reserved));   // reserve/alarm % come from cfg
    if (!saveReservoirConfigToFRAM(c)) return "FRAM write failed";

    g_cfg   = c;
    g_saved = true;
    LOG_INFO("Reservoir config saved: %ux%u mm, sensor→bottom %u mm, marker %u mm, reserve %u%%, alarm %u%%",
             c.length_mm, c.width_mm, c.sensor_to_bottom_mm, c.marker_level_mm, reservePct(c), alarmPct(c));
    return nullptr;
}

bool setPumpCalibration(const PumpCalibration& cal) {
    ReservoirConfig c = g_cfg;
    c.cal   = cal;
    c.magic = RESERVOIR_CONFIG_MAGIC;
    if (!saveReservoirConfigToFRAM(c)) return false;
    g_cfg   = c;
    g_saved = true;
    return true;
}

const char* reservoirConfigWarning(const ReservoirConfig& c) {
    // Footprint of the sensor's field of view on the bottom (empty tank) vs the
    // shorter side — wider footprint may pick up the walls at low levels
    float footprint = 2.0f * c.sensor_to_bottom_mm * tanf(LEVEL_FOV_DEG * 0.5f * (float)M_PI / 180.0f);
    uint16_t shorter = min(c.length_mm, c.width_mm);
    if (footprint > shorter) {
        return "sensor field of view at the bottom is wider than the tank — readings near empty may hit the walls";
    }
    // Alarm level vs the sensor's largest jump between series
    if (alarmPct(c) * c.marker_level_mm < RESERVE_SENSOR_JUMP_MM * 100u) {
        return "alarm level is less than 30 mm of water above the bottom — a sensor jump could hide an empty reservoir";
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

uint8_t reservePct(const ReservoirConfig& c) { return c.reserve_pct ? c.reserve_pct : RESERVE_PCT_DEFAULT; }
uint8_t alarmPct(const ReservoirConfig& c)   { return c.alarm_pct   ? c.alarm_pct   : ALARM_PCT_DEFAULT; }

ReservoirLevels reservoirLevels(const ReservoirConfig& c, uint16_t doseMl) {
    uint32_t dose = doseMl ? doseMl : 1;
    uint32_t cap  = (uint32_t)lroundf(c.length_mm * (float)c.width_mm / 1000.0f * c.marker_level_mm);
    ReservoirLevels l;
    l.fullMl    = cap / dose * dose;
    l.reserveMl = pctUpToPortions(l.fullMl, reservePct(c), dose);
    l.alarmMl   = pctUpToPortions(l.fullMl, alarmPct(c), dose);
    return l;
}

ReservoirLevels reservoirLevels(uint16_t doseMl) {
    return reservoirLevels(g_cfg, doseMl);
}
