#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

#define RESERVE_ML_DEFAULT   2000u   // ml — used when FRAM holds no valid data
#define RESERVE_ML_MIN          1u
#define RESERVE_ML_MAX      10000u
#define RESERVE_WARN_PCT       20u   // warning below this % of configured volume

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
