#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

// Source-water reserve as measured by the VL53L4CD level sensor — information
// only (GUI). Volume = reservoir floor area × water height; the user sees the
// usable part above the hidden safety bottom, in ml and estimated portions.
// No pump stop on an empty reservoir yet: isReserveEmpty() is always false
// until the stop on a ToF alarm level is in place.

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

// Init — logs that there is no empty-reservoir stop
void initReserveController();

// doseMl — portion size for the portion estimate and the rounded thresholds
ReserveView getReserveView(uint16_t doseMl);

bool isReserveLow();     // sensor: in the reserve range (not empty)
bool isReserveEmpty();   // empty → alarm, no new portion (always false for now)

// Reset (GUI / button): clears the empty-reservoir latch (nothing to clear for now)
void clearReserveLatch();

#endif // RESERVE_CONTROLLER_H
