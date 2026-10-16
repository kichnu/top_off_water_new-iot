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

// Direct pump control — bypasses algorithm, system disable and daily limit
bool directPumpOn(uint16_t durationSeconds);
void directPumpOff();
bool isDirectPumpMode();

// Returns the volume of the last direct pump session and resets the counter (0 = no new session)
uint16_t getLastDirectPumpVolumeMl();

// Exact run time (ms) of the last finished direct pump session (not reset on read)
uint32_t getLastDirectPumpRunMs();

#endif
