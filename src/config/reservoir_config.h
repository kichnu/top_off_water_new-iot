#ifndef RESERVOIR_CONFIG_H
#define RESERVOIR_CONFIG_H

#include <Arduino.h>

// Reservoir geometry, persisted in FRAM.
// Rectangular cross-section with vertical walls; the marker on the tank is the
// max water level (full reservoir).

#define RESERVOIR_CONFIG_MAGIC        0xB3

// Reserve thresholds in % of the capacity up to the marker, both rounded up to
// whole portions (see ReservoirLevels). 0 in FRAM = default (blocks saved before
// the fields existed).
// RESERVE — yellow reserve range, warning only (no buzzer).
// ALARM   — alarm level; below it the reservoir counts as empty (no pump stop yet).
#define RESERVE_PCT_DEFAULT            30
#define ALARM_PCT_DEFAULT              15
#define ALARM_PCT_MIN                   5
#define ALARM_PCT_MAX                  50
#define RESERVE_PCT_MAX                90
#define RESERVE_MIN_GAP_PORTIONS        2   // reserve level ≥ alarm level + this many portions
// Largest level jump between sensor series seen in tests (docs/RESERVE_LEVEL_SENSOR_VL53L4CD.md §3)
// — the alarm level should keep at least this much water above the bottom
#define RESERVE_SENSOR_JUMP_MM         30

// Validation ranges
#define RESERVOIR_DIM_MIN_MM           50
#define RESERVOIR_DIM_MAX_MM         2000
#define RESERVOIR_MARKER_MIN_MM        20
#define PORTION_PAUSE_MIN_S            15
#define PORTION_PAUSE_MAX_S           120

// Pump calibration in the working point: portion time for dose_ml (mean of
// CAL_RUNS runs stopped by the user at the vessel mark). Saved only by the
// calibration procedure, which is also the only way to change Single Dose ml.
#define CAL_SENSOR_NONE   INT16_MIN

struct PumpCalibration {
    uint32_t portion_ms;      // 0 = not calibrated
    uint32_t ts;              // unix time of the calibration
    uint16_t dose_ml;         // portion the time belongs to
    uint16_t spread_ms;       // max − min of the runs
    int16_t  sensor_ml;       // no longer measured — always CAL_SENSOR_NONE
    uint8_t  runs;
    uint8_t  _pad;
};
static_assert(sizeof(PumpCalibration) == 16, "PumpCalibration must be exactly 16 bytes");

struct ReservoirConfig {
    uint16_t length_mm;
    uint16_t width_mm;
    uint16_t sensor_to_bottom_mm;   // sensor face → reservoir bottom
    uint16_t marker_level_mm;       // marker (max water level) above the bottom
    uint8_t  marker_tol_mm;         // unused (refill check removed), kept for the FRAM layout
    uint8_t  portion_pause_s;       // unused (manual portions use the fixed MANUAL_PORTION_PAUSE_MS,
                                    // the calibration CAL_PAUSE_MS), kept for the FRAM layout
    uint8_t  flags;                 // unused (was: level sensor on/off), kept for the FRAM layout
    uint8_t  magic;                 // RESERVOIR_CONFIG_MAGIC = valid block
    PumpCalibration cal;            // not editable via setReservoirConfig
    uint8_t  reserve_pct;           // 0 = RESERVE_PCT_DEFAULT
    uint8_t  alarm_pct;             // 0 = ALARM_PCT_DEFAULT
    uint8_t  _reserved[2];
};
static_assert(sizeof(ReservoirConfig) == 32, "ReservoirConfig must be exactly 32 bytes");

// Call after initNVS() (FRAM ready) — loads the config or falls back to defaults
void initReservoirConfig();

const ReservoirConfig& getReservoirConfig();
bool  isReservoirConfigSaved();      // false = defaults, never saved by the user

// Validates and saves. doseMl — portion for the reserve/alarm gap check.
// Returns nullptr on success, otherwise an error message.
const char* setReservoirConfig(const ReservoirConfig& cfg, uint16_t doseMl);

// Saves the pump calibration (keeps the rest of the config)
bool setPumpCalibration(const PumpCalibration& cal);

// Non-blocking warning for a valid config (nullptr = none)
const char* reservoirConfigWarning(const ReservoirConfig& cfg);

// Derived values
float    reservoirMlPerMm();             // ml per 1 mm of level
uint32_t reservoirCapacityMl();          // up to the marker (geometry, not rounded)
uint8_t  reservePct(const ReservoirConfig& c);   // effective (default for 0)
uint8_t  alarmPct(const ReservoirConfig& c);

// Capacity in whole portions of doseMl (all values are multiples of doseMl)
struct ReservoirLevels {
    uint32_t fullMl;      // capacity up to the marker, rounded down to whole portions — 100%
    uint32_t reserveMl;   // reserve_pct of fullMl, rounded up — yellow reserve range
    uint32_t alarmMl;     // alarm_pct of fullMl, rounded up — alarm level
};
ReservoirLevels reservoirLevels(uint16_t doseMl);
ReservoirLevels reservoirLevels(const ReservoirConfig& c, uint16_t doseMl);

#endif // RESERVOIR_CONFIG_H
