#ifndef PUMP_CALIBRATION_H
#define PUMP_CALIBRATION_H

#include <Arduino.h>

// Pump calibration in the working point (portion = Single Dose ml).
// The user sets the portion as the first step, then runs the pump CAL_RUNS times
// and stops each run when the vessel reaches the portion mark; the firmware
// measures the run time in ms. With the level sensor on, the level is measured
// before and after (data for later analysis). Confirm saves the portion time
// (median), the dose and the derived ml/s. Changing Single Dose ml is only
// possible through this procedure.

#define CAL_RUNS               10
#define CAL_RUN_MAX_MS     300000UL   // a run left running this long aborts the calibration
#define CAL_DOSE_MIN_ML         1
#define CAL_DOSE_MAX_ML      2000

enum CalPhase : uint8_t {
    CAL_IDLE    = 0,
    CAL_BEFORE  = 1,   // stable level measurement before the runs
    CAL_READY   = 2,   // waiting for "Start run" (pause between runs enforced)
    CAL_RUNNING = 3,   // pump on, waiting for "Stop"
    CAL_AFTER   = 4,   // quiet time + stable level measurement after the runs
    CAL_RESULT  = 5,   // result shown, waiting for confirm / cancel
};

struct CalStatus {
    CalPhase    phase;
    uint16_t    doseMl;              // portion being calibrated
    uint8_t     runs;                // completed runs
    uint32_t    runMs[CAL_RUNS];
    uint32_t    pauseLeftS;          // READY: seconds until the next run may start
    uint32_t    runElapsedMs;        // RUNNING
    bool        sensor;              // level data collected
    float       beforeMm;            // sensor → water before (NAN = none)
    float       afterMm;             // after (NAN = none)
    // Result
    uint32_t    medianMs, minMs, maxMs;
    float       sdMs;
    int32_t     sensorMl;            // level drop × ml/mm (valid when hasSensorMl)
    bool        hasSensorMl;
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

// Portion time for the current Single Dose ml — calibrated, or ml/s fallback
uint32_t getPortionMs();
bool     isPumpCalibrated();   // calibration exists for the current dose

#endif // PUMP_CALIBRATION_H
