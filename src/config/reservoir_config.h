#ifndef RESERVOIR_CONFIG_H
#define RESERVOIR_CONFIG_H

#include <Arduino.h>

// Reservoir geometry and level-sensor settings, persisted in FRAM.
// Rectangular cross-section with vertical walls; the marker on the tank is the
// max water level (full reservoir).

#define RESERVOIR_CONFIG_MAGIC        0xB3
#define RESERVOIR_FLAG_USE_SENSOR     0x01   // reserve from the level sensor; off = portion counting
                                             // (sensor still measures, no sensor alarms, pump stops at low)

// Fixed reserve thresholds — % of the capacity up to the marker.
// With the sensor: low = warning, empty = alarm + pump stop.
// Without the sensor (portion counting): pump stop + alarm already at low.
#define RESERVE_LOW_PCT_FIXED          30
#define RESERVE_EMPTY_PCT_FIXED         5

// Validation ranges
#define RESERVOIR_DIM_MIN_MM           50
#define RESERVOIR_DIM_MAX_MM         2000
#define RESERVOIR_MARKER_MIN_MM        20
#define RESERVOIR_TOL_MIN_MM            1
#define RESERVOIR_TOL_MAX_MM           30
#define PORTION_PAUSE_MIN_S            15
#define PORTION_PAUSE_MAX_S           120

struct ReservoirConfig {
    uint16_t length_mm;
    uint16_t width_mm;
    uint16_t sensor_to_bottom_mm;   // sensor face → reservoir bottom
    uint16_t marker_level_mm;       // marker (max water level) above the bottom
    uint8_t  marker_tol_mm;         // marker check tolerance (±)
    uint8_t  portion_pause_s;       // pause between portions (calibration, manual top-off)
    uint8_t  flags;                 // RESERVOIR_FLAG_*
    uint8_t  magic;                 // RESERVOIR_CONFIG_MAGIC = valid block
    uint8_t  _reserved[20];         // pump calibration / marker check (later steps)
};
static_assert(sizeof(ReservoirConfig) == 32, "ReservoirConfig must be exactly 32 bytes");

// Call after initNVS() (FRAM ready) — loads the config or falls back to defaults
void initReservoirConfig();

const ReservoirConfig& getReservoirConfig();
bool  isReservoirConfigSaved();      // false = defaults, never saved by the user

// Validates and saves. Returns nullptr on success, otherwise an error message.
const char* setReservoirConfig(const ReservoirConfig& cfg);

// Non-blocking warning for a valid config (nullptr = none)
const char* reservoirConfigWarning(const ReservoirConfig& cfg);

// Derived values
float    reservoirMlPerMm();             // ml per 1 mm of level
uint32_t reservoirCapacityMl();          // up to the marker
bool     useLevelSensor();

#endif // RESERVOIR_CONFIG_H
