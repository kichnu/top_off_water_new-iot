#ifndef PUMP_CONTROLLER_H
#define PUMP_CONTROLLER_H
#include <Arduino.h>

void initPumpController();
void updatePumpController();

// Timed run — the end is cut by a one-shot esp_timer (ms precision, independent
// of the loop() period). Used by the ATO algorithm and the pump calibration.
bool triggerPumpMs(uint32_t durationMs, const String& actionType);
bool isPumpActive();
uint32_t getPumpRemainingTime();   // seconds
void stopPump();

// Exact run time (ms) of the last finished run of any kind (not reset on read)
uint32_t getLastPumpRunMs();

// Manual dosing in portions — bypasses algorithm, system disable and daily limit,
// not logged in the cycle history. Each ON adds one portion (getPortionMs()) to the
// queue; portions are separated by a short pause. OFF finishes the running portion
// and clears the queue. A new portion starts only when it fits above the reservoir
// alarm level (reserveCanDose).
#define MANUAL_PORTION_PAUSE_MS  5000
#define MANUAL_QUEUE_MAX         20

bool manualPortionAdd();          // false: algorithm/calibration pump running or queue full
void manualPortionsStop();
bool isDirectPumpMode();          // true for the whole manual session (portions + pauses)
bool isManualPause();
uint8_t getManualPortionsLeft();  // including the running one
uint8_t getManualPortionsDone();  // finished in the current session

// Total run time (ms) of the last finished manual session (not reset on read)
uint32_t getLastDirectPumpRunMs();

#endif
