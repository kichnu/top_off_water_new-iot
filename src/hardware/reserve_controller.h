#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

// Source-water reserve as measured by the VL53L4CD level sensor. Volume =
// reservoir floor area × water height, 100% = the marker. Zones from the
// reserve and alarm levels in ReservoirConfig; portions are counted down to the
// alarm level (what can still be pumped). The sensor error (~±2 cm) is covered by
// the alarm level margin (RESERVE_SENSOR_JUMP_MM).
// No pump stop on an empty reservoir yet: isReserveEmpty() is always false
// until the stop on the alarm level is in place.

enum ReserveZone : uint8_t {
    RESERVE_ZONE_OK      = 0,
    RESERVE_ZONE_RESERVE = 1,   // at or below the reserve level — warning, no buzzer
    RESERVE_ZONE_ALARM   = 2,   // at or below the alarm level
    RESERVE_ZONE_UNKNOWN = 3,   // reservoir not configured or no valid sensor reading
};

// Reserve as shown to the user (GUI, display)
struct ReserveView {
    bool        reading;           // valid sensor reading behind the values below
    uint32_t    ml;                // water in the reservoir (from the bottom)
    uint32_t    capacityMl;        // capacity up to the marker
    uint32_t    reserveMl;         // reserve level: zone RESERVE at or below it
    uint32_t    alarmMl;           // alarm level: zone ALARM at or below it
    uint16_t    portions;          // whole portions above the alarm level
    uint16_t    capacityPortions;  // whole portions between the alarm level and the marker
    float       pct;               // ml / capacity × 100 (may exceed 100 above the marker)
    float       waterMm;           // water height above the bottom
    ReserveZone zone;
};

// Init — logs that there is no empty-reservoir stop
void initReserveController();

// doseMl — portion size for the portion estimate and the rounded thresholds
ReserveView getReserveView(uint16_t doseMl);

bool isReserveLow();     // sensor: zone RESERVE or ALARM
bool isReserveEmpty();   // empty → alarm, no new portion (always false for now)

// Reset (GUI / button): clears the empty-reservoir latch (nothing to clear for now)
void clearReserveLatch();

#endif // RESERVE_CONTROLLER_H
