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

// Manual dosing in portions — a deliberate on-site service action: Service Mode
// only (the switch to Auto stops it), otherwise never blocked (algorithm state,
// daily limit, reservoir level) and not counted anywhere (24 h volume, history,
// EMA); only the reserve tracking subtracts the water. A command adds n portions
// (getPortionMs() each) to the queue; portions are separated by a short pause.
// OFF finishes the running portion and clears the queue. Preconditions (Service
// Mode, calibration, reservoir) are checked by node_commands.
#define MANUAL_PORTION_PAUSE_MS  5000
#define MANUAL_QUEUE_MAX         20

// Returns the number of portions queued (0..n): 0 when the algorithm/calibration
// pump runs or outside Service Mode, fewer than n when the queue is full
uint8_t manualPortionsAdd(uint8_t n);
void manualPortionsStop();
bool isDirectPumpMode();          // true for the whole manual session (portions + pauses)
bool isManualPause();
uint8_t getManualPortionsLeft();  // including the running one
uint8_t getManualPortionsDone();  // finished in the current session

// Total run time (ms) of the last finished manual session (not reset on read)
uint32_t getLastDirectPumpRunMs();

#endif
