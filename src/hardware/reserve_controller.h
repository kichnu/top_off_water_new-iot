#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

#define RESERVE_ML_DEFAULT   2000u   // ml — used when FRAM holds no valid data
#define RESERVE_ML_MIN          1u
#define RESERVE_ML_MAX      10000u
#define RESERVE_WARN_PCT       20u   // warning below this % of configured volume

// Reserve zone on the portion-rounded thresholds (ReservoirLevels)
enum ReserveZone : uint8_t {
    RESERVE_ZONE_OK      = 0,
    RESERVE_ZONE_LOW     = 1,
    RESERVE_ZONE_EMPTY   = 2,
    RESERVE_ZONE_UNKNOWN = 3,   // sensor mode without a valid reading
};

// Reserve as shown to the user (GUI, display) — from the level sensor when it is
// enabled in the reservoir settings, otherwise from the software ml counter
struct ReserveView {
    bool        fromSensor;
    bool        known;          // false → ml / pct / zone not meaningful
    uint32_t    ml;             // usable ml — sensor: above the hidden safety bottom
    uint32_t    capacityMl;     // sensor: usable capacity (whole portions); counter: configured volume
    uint32_t    lowMl;          // reserve range (whole portions): zone LOW at or below it
    float       pct;            // ml / capacity × 100 (above 100 when over the marker)
    float       waterMm;        // water height above the bottom (sensor only, else 0)
    ReserveZone zone;
};

// doseMl — portion size used to round the capacity and thresholds
ReserveView getReserveView(uint16_t doseMl);

// Init — loads state from FRAM
void initReserveController();

// Current reserve state (software counter until the VL53L4CD level sensor is integrated)
uint32_t getReserveMl();
uint32_t getReserveConfigMl();
bool     isReserveLow();     // <= RESERVE_WARN_PCT and not empty → yellow warning
bool     isReserveEmpty();   // currentMl == 0 → red alarm, pump blocked

// Subtracts ml after a pump run — clamps to 0, saves to FRAM
void subtractReserveMl(uint16_t ml);

// Sets the configured reserve volume (and resets the current value to it)
void setReserveConfigMl(uint32_t ml);

// Reservoir refilled — resets current to configMl and saves to FRAM.
// Called on "Cycle Reset".
void clearReserveAlarm();

#endif // RESERVE_CONTROLLER_H
