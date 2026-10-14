#ifndef PUMP_CONTROLLER_H
#define PUMP_CONTROLLER_H
#include <Arduino.h>

void initPumpController();
void updatePumpController();
bool triggerPump(uint16_t durationSeconds, const String& actionType);
bool isPumpActive();
uint32_t getPumpRemainingTime();
void stopPump();

// Direct pump control — bypasses algorithm, system disable and daily limit
bool directPumpOn(uint16_t durationSeconds);
void directPumpOff();
bool isDirectPumpMode();

// Returns the volume of the last direct pump session and resets the counter (0 = no new session)
uint16_t getLastDirectPumpVolumeMl();

// Exact run time (ms) of the last finished direct pump session (not reset on read)
uint32_t getLastDirectPumpRunMs();

#endif