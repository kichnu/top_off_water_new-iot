#ifndef MIXING_PUMP_H
#define MIXING_PUMP_H

#include <Arduino.h>

// Mixing pump — simple relay on MIXING_PUMP_RELAY_PIN (HIGH = ON)

void initMixingPump();
void startMixingPump();
void stopMixingPump();
bool isMixingPumpActive();

#endif // MIXING_PUMP_H
