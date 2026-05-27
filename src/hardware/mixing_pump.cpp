#include "mixing_pump.h"
#include "hardware_pins.h"
#include "../core/logging.h"

static bool mixRunning = false;

void initMixingPump() {
    pinMode(MIXING_PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(MIXING_PUMP_RELAY_PIN, HIGH);  // HIGH = OFF (active-LOW relay)
    mixRunning = false;
    LOG_INFO("Mixing pump initialized on GPIO %d", MIXING_PUMP_RELAY_PIN);
}

void startMixingPump() {
    if (mixRunning) return;
    digitalWrite(MIXING_PUMP_RELAY_PIN, LOW);   // LOW = ON (active-LOW relay)
    mixRunning = true;
    LOG_INFO("Mixing pump: START");
}

void stopMixingPump() {
    if (!mixRunning) return;
    digitalWrite(MIXING_PUMP_RELAY_PIN, HIGH);  // HIGH = OFF (active-LOW relay)
    mixRunning = false;
    LOG_INFO("Mixing pump: STOP");
}

bool isMixingPumpActive() {
    return mixRunning;
}
