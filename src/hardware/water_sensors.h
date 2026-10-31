#ifndef WATER_SENSORS_H
#define WATER_SENSORS_H

#include <Arduino.h>

// ============================================================
// ATO float sensor (WATER_SENSOR_PIN).
// Hardware: two floats in parallel on one GPIO — hardware redundancy.
// Logic: LOW = water below the level.
//
// Debouncing: a sample every DEBOUNCE_INTERVAL_MS,
//             DEBOUNCE_COUNTER consecutive LOW samples required.
// ============================================================

enum SensorPhase {
    PHASE_IDLE = 0,
    PHASE_DEBOUNCING
};

// ============== INIT AND UPDATE ==============
void initWaterSensor();
void updateWaterSensor();      // Call from loop()

// ============== READ ==============
bool readWaterSensor();        // Raw GPIO read (true = LOW = water below the level)
void resetSensorProcess();     // Back to PHASE_IDLE

// ============== STATE GETTERS (UI / diagnostics) ==============
SensorPhase getSensorPhase();
const char* getSensorPhaseString();
uint8_t     getDebounceCounter();    // Current number of confirmed LOW samples
uint32_t    getPhaseElapsedMs();     // ms since the current phase started

#endif // WATER_SENSORS_H
