#ifndef LEVEL_STABLE_H
#define LEVEL_STABLE_H

#include <Arduino.h>

// Stable level measurement — on-demand series until the level at the sensor is
// stable (partitioned tank equalizes slowly). Shared by the reserve refill and
// the pump calibration (one measurement at a time). A pump start fails it.

#define STABLE_SERIES_GAP_MS        5000UL    // next on-demand series this long after the last one
#define STABLE_WINDOW                  8      // stable window: last N valid series
#define STABLE_SPREAD_MM             3.0f     // max − min within the window
#define STABLE_TREND_MM              1.0f     // |mean of 2nd half − mean of 1st half| (still moving?)
#define STABLE_TIMEOUT_MS         300000UL    // no stable window by then → failed

enum StableState : uint8_t {
    STABLE_IDLE      = 0,
    STABLE_WAIT      = 1,   // waiting for the post-pump quiet time to end
    STABLE_RUNNING   = 2,
    STABLE_DONE      = 3,   // distanceMm valid
    STABLE_FAILED    = 4,   // note says why
};

struct StableLevel {
    StableState state;
    float       distanceMm;   // mean of the window (sensor → water)
    float       spreadMm;     // max − min in the window
    uint8_t     series;       // valid series in the window
    uint32_t    elapsedS;     // waiting: since start; running: since the series started
    const char* note;
};

// waitQuiet — first wait until the post-pump quiet time is over
void stableLevelStart(bool waitQuiet);
void stableLevelCancel();

// Call every loop() iteration
void updateStableLevel();

const StableLevel& getStableLevel();

#endif // LEVEL_STABLE_H
