#ifndef PUMP_CALIBRATION_H
#define PUMP_CALIBRATION_H

#include <Arduino.h>

// Pump calibration in the working point (portion = Single Dose ml).
// The user sets the portion as the first step; the first run starts by itself
// CAL_START_DELAY_MS later, runs 2..CAL_RUNS are started by the user after a
// fixed pause (water in the hose drains back). The user stops each run when
// the vessel reaches the portion mark; the firmware measures the run time in ms.
// Right after the last run the result (mean time) can be confirmed or rejected.
// No level measurement during the calibration. Changing Single Dose ml is only
// possible through this procedure.

#define CAL_RUNS                4
#define CAL_START_DELAY_MS   5000UL   // Start calibration → run 1 starts by itself
#define CAL_PAUSE_MS        20000UL   // between runs: hose drains back, vessel emptied
#define CAL_RUN_MAX_MS     300000UL   // a run left running this long aborts the calibration
#define CAL_SPREAD_WARN_PCT    20     // max − min above this % of the mean → hint to repeat
#define CAL_DOSE_MIN_ML         1
#define CAL_DOSE_MAX_ML      2000
#define DEFAULT_PORTION_MS   5000UL   // uncalibrated pump — short and safe until calibrated

enum CalPhase : uint8_t {
    CAL_IDLE      = 0,
    CAL_COUNTDOWN = 1,   // run 1 starts by itself when the countdown ends
    CAL_READY     = 2,   // waiting for "Start run" (pause between runs enforced)
    CAL_RUNNING   = 3,   // pump on, waiting for "Stop"
    CAL_RESULT    = 4,   // result shown, waiting for confirm / cancel
};

struct CalStatus {
    CalPhase    phase;
    uint16_t    doseMl;              // portion being calibrated
    uint8_t     runs;                // completed runs
    uint32_t    runMs[CAL_RUNS];
    uint32_t    pauseLeftS;          // COUNTDOWN / READY: seconds until the next run
    uint32_t    runElapsedMs;        // RUNNING
    // Result
    uint32_t    meanMs, minMs, maxMs;
    float       sdMs;
    const char* note;                // last message (also after the procedure ended)
};

// Starts the procedure with the new portion; puts the system into Service Mode.
// nullptr = OK, else an error message.
const char* startPumpCalibration(uint16_t doseMl);

// "run", "stop", "confirm", "cancel". nullptr = OK, else an error message.
const char* pumpCalibrationAction(const String& action);

// Call every loop() iteration
void updatePumpCalibration();

const CalStatus& getCalStatus();
bool isPumpCalibrationActive();

// Portion time for the current Single Dose ml — calibrated, or DEFAULT_PORTION_MS
uint32_t getPortionMs();
bool     isPumpCalibrated();   // calibration exists for the current dose

#endif // PUMP_CALIBRATION_H
