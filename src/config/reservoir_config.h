#ifndef RESERVOIR_CONFIG_H
#define RESERVOIR_CONFIG_H

#include <Arduino.h>

// Reservoir geometry and level-sensor settings, persisted in FRAM.
// Rectangular cross-section with vertical walls; the marker on the tank is the
// max water level (full reservoir).

#define RESERVOIR_CONFIG_MAGIC        0xB3
#define RESERVOIR_FLAG_USE_SENSOR     0x01   // level sensor verifies refills and doses; off = portion
                                             // counting only (sensor still measures, no sensor alarms)

// Fixed reserve thresholds, both rounded up to whole portions (see ReservoirLevels).
// HIDDEN — % of the full capacity kept below "empty" for pump safety (intake
//          height); never shown to the user, the usable range ends above it.
// LOW    — % of the usable capacity shown as the permanent yellow reserve range.
// Both modes (portion counter): low = information only, 0 portions = alarm + pump stop.
#define RESERVE_LOW_PCT_FIXED          30
#define RESERVE_HIDDEN_PCT_FIXED        5

// Validation ranges
#define RESERVOIR_DIM_MIN_MM           50
#define RESERVOIR_DIM_MAX_MM         2000
#define RESERVOIR_MARKER_MIN_MM        20
#define RESERVOIR_TOL_MIN_MM            1
#define RESERVOIR_TOL_MAX_MM           30
#define PORTION_PAUSE_MIN_S            15
#define PORTION_PAUSE_MAX_S           120

// Pump calibration in the working point: portion time for dose_ml (median of
// CAL_RUNS runs stopped by the user at the vessel mark). Saved only by the
// calibration procedure, which is also the only way to change Single Dose ml.
#define CAL_SENSOR_NONE   INT16_MIN

struct PumpCalibration {
    uint32_t portion_ms;      // 0 = not calibrated
    uint32_t ts;              // unix time of the calibration
    uint16_t dose_ml;         // portion the time belongs to
    uint16_t spread_ms;       // max − min of the runs
    int16_t  sensor_ml;       // level-sensor drop over all runs, CAL_SENSOR_NONE = no data
    uint8_t  runs;
    uint8_t  _pad;
};
static_assert(sizeof(PumpCalibration) == 16, "PumpCalibration must be exactly 16 bytes");

struct ReservoirConfig {
    uint16_t length_mm;
    uint16_t width_mm;
    uint16_t sensor_to_bottom_mm;   // sensor face → reservoir bottom
    uint16_t marker_level_mm;       // marker (max water level) above the bottom
    uint8_t  marker_tol_mm;         // marker check tolerance (±)
    uint8_t  portion_pause_s;       // pause between portions (calibration, manual top-off)
    uint8_t  flags;                 // RESERVOIR_FLAG_*
    uint8_t  magic;                 // RESERVOIR_CONFIG_MAGIC = valid block
    PumpCalibration cal;            // not editable via setReservoirConfig
    uint8_t  _reserved[4];
};
static_assert(sizeof(ReservoirConfig) == 32, "ReservoirConfig must be exactly 32 bytes");

// Call after initNVS() (FRAM ready) — loads the config or falls back to defaults
void initReservoirConfig();

const ReservoirConfig& getReservoirConfig();
bool  isReservoirConfigSaved();      // false = defaults, never saved by the user

// Validates and saves. Returns nullptr on success, otherwise an error message.
const char* setReservoirConfig(const ReservoirConfig& cfg);

// Saves the pump calibration (keeps the rest of the config)
bool setPumpCalibration(const PumpCalibration& cal);

// Non-blocking warning for a valid config (nullptr = none)
const char* reservoirConfigWarning(const ReservoirConfig& cfg);

// Derived values
float    reservoirMlPerMm();             // ml per 1 mm of level
uint32_t reservoirCapacityMl();          // up to the marker (geometry, not rounded)

// Capacity in whole portions of doseMl (all values are multiples of doseMl)
struct ReservoirLevels {
    uint32_t fullMl;     // capacity up to the marker, rounded down to whole portions
    uint32_t hiddenMl;   // RESERVE_HIDDEN_PCT_FIXED of fullMl, rounded up — safety bottom
    uint32_t usableMl;   // fullMl − hiddenMl — what the user sees as 100%
    uint32_t lowMl;      // RESERVE_LOW_PCT_FIXED of usableMl, rounded up — yellow reserve
};
ReservoirLevels reservoirLevels(uint16_t doseMl);
bool     useLevelSensor();

#endif // RESERVOIR_CONFIG_H
