#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

// Source-water reserve as measured by the VL53L4CD level sensor — information
// only (GUI). Volume = reservoir floor area × water height; the user sees the
// usable part above the hidden safety bottom, in ml and estimated portions.
// The sensor error (~±2 cm) is fine for the yellow reserve range, but not for
// stopping the pump — isReserveEmpty() is the reservoir float switch
// (RESERVOIR_FLOAT_PIN, LOW = no water, latched until a reset with water back).

// TEMPORARY: float switch not installed yet — set to 1 once it is wired
#define RESERVE_FLOAT_ENABLED        0
#define RESERVE_FLOAT_DEBOUNCE_MS 3000UL

enum ReserveZone : uint8_t {
    RESERVE_ZONE_OK      = 0,
    RESERVE_ZONE_LOW     = 1,   // in the yellow reserve range — information only
    RESERVE_ZONE_EMPTY   = 2,   // nothing usable above the hidden bottom — information only
    RESERVE_ZONE_UNKNOWN = 3,   // reservoir not configured or no valid sensor reading
};

// Reserve as shown to the user (GUI, display)
struct ReserveView {
    bool        reading;           // valid sensor reading behind the values below
    uint32_t    ml;                // usable ml (above the hidden bottom)
    uint32_t    capacityMl;        // usable capacity up to the marker
    uint32_t    lowMl;             // reserve range: zone LOW at or below it
    uint16_t    portions;          // estimated whole portions in ml
    uint16_t    capacityPortions;  // whole portions in capacityMl
    float       pct;               // ml / capacity × 100 (may exceed 100 above the marker)
    float       waterMm;           // water height above the bottom
    ReserveZone zone;
};

// Init — configures the float switch input
void initReserveController();

// Call every loop() iteration — float switch debounce
void updateReserveController();

// doseMl — portion size for the portion estimate and the rounded thresholds
ReserveView getReserveView(uint16_t doseMl);

bool isReserveLow();     // sensor: in the reserve range (not empty)
bool isReserveEmpty();   // float switch: no water (or latched) → alarm, no new portion

// Reset (GUI / button): clears the latch when the float reports water again
void clearReserveLatch();

const char* getReserveFloatString();   // "ok", "low", "latched", "disabled"

#endif // RESERVE_CONTROLLER_H
