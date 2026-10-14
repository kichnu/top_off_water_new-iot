#ifndef LEVEL_TEST_H
#define LEVEL_TEST_H

#include <Arduino.h>
#include "level_sensor.h"

// TEST: VL53L4CD consistency check against the manual (direct) pump.
// Each direct pump run is compared: pumped ml (run time × volume_per_second)
// vs ml derived from the reservoir level drop. Result: one "LVLTEST,..." log line.

#define LEVEL_TEST_SETTLE_MS  LEVEL_PUMP_QUIET_MS  // wait after pump stop before measuring
#define LEVEL_TEST_AFTER_SERIES           3     // series averaged for the "after" level
#define LEVEL_TEST_BEFORE_SERIES          5     // max latest between-runs series averaged for "before"
#define LEVEL_TEST_BEFORE_MAX_AGE_MS  30000UL   // newest "before" series older → run marked stale

// Call every loop() iteration (after updateLevelSensor())
void updateLevelTest();

#endif // LEVEL_TEST_H
