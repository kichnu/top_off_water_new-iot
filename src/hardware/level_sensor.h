#ifndef LEVEL_SENSOR_H
#define LEVEL_SENSOR_H

#include <Arduino.h>

// VL53L4CD ToF sensor above the source-water reservoir (shared I2C bus, addr 0x29).
// Measures distance to the water surface in non-blocking series of N samples
// (one sample per loop() iteration).

#define LEVEL_TIMING_BUDGET_MS        50     // per-sample ranging budget (continuous mode)
#define LEVEL_SAMPLES_PER_SERIES      10     // N valid samples per series
#define LEVEL_SERIES_INTERVAL_MS   10000UL   // bring-up value; target ~60 s
#define LEVEL_SERIES_TIMEOUT_MS     3000UL   // series closed with what it has after this
#define LEVEL_INIT_RETRY_MS        30000UL   // retry init when the sensor is not detected
#define LEVEL_FAULT_SERIES             3     // consecutive bad series → FAULT
#define LEVEL_PUMP_QUIET_MS       120000UL   // no scheduled series this long after a pump stop
                                             // (partitioned tank equalizes through a small hole)

// Reservoir geometry — TEST: hardcoded until configurable (vertical walls assumed)
#define LEVEL_TANK_LENGTH_MM         282.0f
#define LEVEL_TANK_WIDTH_MM          202.0f
#define LEVEL_SENSOR_TO_BOTTOM_MM    470.0f  // sensor face → reservoir bottom
#define LEVEL_TANK_MAX_LEVEL_MM      380.0f  // max water level above the bottom
#define LEVEL_ML_PER_MM  (LEVEL_TANK_LENGTH_MM * LEVEL_TANK_WIDTH_MM / 1000.0f)

enum LevelSensorState : uint8_t {
    LEVEL_SENSOR_ABSENT = 0,   // no ACK / init failed
    LEVEL_SENSOR_OK     = 1,
    LEVEL_SENSOR_FAULT  = 2,   // detected, but LEVEL_FAULT_SERIES bad series in a row
};

struct LevelSeries {
    bool     valid;          // at least N/2 valid samples
    float    distance_mm;    // mean of valid samples
    float    stddev_mm;      // sample standard deviation
    float    sem_mm;         // standard error of the mean (stddev / sqrt(n))
    uint8_t  samples;        // valid samples (range_status == 0)
    uint8_t  rejected;       // samples with range_status != 0
    uint16_t signal_kcps;    // mean signal rate of valid samples
    uint16_t ambient_kcps;   // mean ambient rate of valid samples
    uint32_t finished_ms;    // millis() when the series closed
};

// Call after Wire.begin() and the I2C bus reset in setup()
void initLevelSensor();

// Call every loop() iteration
void updateLevelSensor();

LevelSensorState   getLevelSensorState();
const char*        getLevelSensorStateString();
const LevelSeries& getLastLevelSeries();   // last completed series (valid or not)

// Start a series now (restarts one in progress, allowed in the post-pump quiet time);
// completion bumps the series counter
void     requestLevelSeries();
uint32_t getLevelSeriesCount();            // number of completed series since boot

#endif // LEVEL_SENSOR_H
