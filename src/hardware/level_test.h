#ifndef LEVEL_TEST_H
#define LEVEL_TEST_H

#include <Arduino.h>

// TEST: VL53L4CD consistency check against the manual (direct) pump.
// Each direct pump run is compared: pumped ml (run time × volume_per_second)
// vs ml derived from the reservoir level drop. Result: one "LVLTEST,..." log line.

#define LEVEL_TEST_SETTLE_MS          30000UL   // wait after pump stop before measuring
#define LEVEL_TEST_AFTER_SERIES           3     // series averaged for the "after" level
#define LEVEL_TEST_BEFORE_MAX_AGE_MS  30000UL   // older "before" reading → run marked stale

// Call every loop() iteration (after updateLevelSensor())
void updateLevelTest();

#endif // LEVEL_TEST_H
