#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

// Source-water reserve from the VL53L4CD level sensor, with the pump stop on
// the alarm level (ReservoirConfig alarm_pct). Volume = reservoir floor area ×
// water height, 100% = the marker.
//
// Measured mode: level = min of the last RESERVE_MIN_SERIES valid series minus
// the water pumped since. Estimated mode (no valid reading for RESERVE_STALE_MS
// outside the post-pump quiet time, and after a boot): the half of what was
// above the alarm level at the last reading, minus everything pumped since —
// i.e. every portion counts double. Back to measured after RESERVE_MIN_SERIES
// valid series in a row. The last reading and the water pumped since are kept
// in FRAM, so a restart continues from them.
//
// The alarm (critical, buzzer) latches when the level / estimate reaches the
// alarm level, when a dose would go below it, or without any reading at all;
// it clears only on a reset with a measured level ≥ alarm + 1 portion, or with
// "reservoir refilled" (estimated mode only — sensor not reading).

#define RESERVE_MIN_SERIES                3      // series for the level min / back to measured
#define RESERVE_STALE_MS             300000UL    // no valid reading this long → estimated mode
#define RESERVE_NO_READING_ALERT_MS 1800000UL    // no valid reading this long → warning (no buzzer)
#define RESERVE_NO_DATA_GRACE_MS      60000UL    // after boot, no reading ever → alarm after this
#define RESERVE_TRACK_MAGIC            0xC5

enum ReserveZone : uint8_t {
    RESERVE_ZONE_OK      = 0,
    RESERVE_ZONE_RESERVE = 1,   // at or below the reserve level — warning, no buzzer
    RESERVE_ZONE_ALARM   = 2,   // at or below the alarm level
    RESERVE_ZONE_UNKNOWN = 3,   // reservoir not configured or no reading ever
};

enum ReserveAlarmCause : uint8_t {
    RESERVE_ALARM_NONE     = 0,
    RESERVE_ALARM_LEVEL    = 1,   // measured level at the alarm level
    RESERVE_ALARM_ESTIMATE = 2,   // estimate (no reading) down to the alarm level
    RESERVE_ALARM_NO_DATA  = 3,   // no reading ever / reservoir not configured
    RESERVE_ALARM_DOSE     = 4,   // the next portion would go below the alarm level
};

enum ReserveSource : uint8_t {
    RESERVE_SRC_NONE   = 0,
    RESERVE_SRC_SENSOR = 1,
    RESERVE_SRC_REFILL = 2,       // "reservoir refilled" — assumed full to the marker
};

// Persisted in FRAM (FRAM_ADDR_RESERVE_TRACK), 16 bytes + checksum
struct ReserveTrack {
    uint32_t base_ml;      // water at the last reading (or the refill)
    uint32_t base_ts;      // unix time of base_ml
    uint32_t drawn_ml;     // pumped since base_ml
    uint8_t  source;       // ReserveSource of base_ml
    uint8_t  alarm;        // ReserveAlarmCause, latched
    uint8_t  magic;        // RESERVE_TRACK_MAGIC
    uint8_t  _pad;
};
static_assert(sizeof(ReserveTrack) == 16, "ReserveTrack must be exactly 16 bytes");

// Reserve as shown to the user (GUI, display)
struct ReserveView {
    bool        reading;           // measured mode (else estimated / unknown)
    bool        estimated;         // estimated mode with a base to estimate from
    uint32_t    ml;                // water in the reservoir (estimated: the minimum)
    uint32_t    capacityMl;        // capacity up to the marker
    uint32_t    reserveMl;         // reserve level: zone RESERVE at or below it
    uint32_t    alarmMl;           // alarm level: zone ALARM at or below it
    int32_t     availMl;           // may still be pumped above the alarm level (≤ 0 → alarm)
    uint16_t    portions;          // whole portions in availMl
    uint16_t    capacityPortions;  // whole portions between the alarm level and the marker
    float       pct;               // ml / capacity × 100 (may exceed 100 above the marker)
    float       waterMm;           // water height above the bottom
    uint32_t    noReadingS;        // since the last valid reading (or boot)
    ReserveZone zone;
};

// Init after initReservoirConfig() — loads the FRAM state
void initReserveController();

// Call every loop() iteration — readings, mode, alarm latch, FRAM save
void updateReserveController();

// doseMl — portion size for the portion counts and the rounded thresholds
ReserveView getReserveView(uint16_t doseMl);

bool isReserveLow();             // zone RESERVE or ALARM
bool isReserveEmpty();           // alarm latched → buzzer, no new portion
ReserveAlarmCause getReserveAlarmCause();
const char* getReserveAlarmString();
bool isReserveNoReadingAlert();  // no valid reading for RESERVE_NO_READING_ALERT_MS

// One more portion fits above the alarm level (no side effects — any context)
bool reserveCanDose();
// The same before an auto dose (loop() context): latches the alarm when it does not fit
bool reserveCheckDose();

// Pump run finished (any context) — water taken from the reservoir
void reserveOnPumpRun(uint32_t runMs, bool calibration);

// Reset (GUI / button): clears the alarm with a measured level ≥ alarm + 1 portion
void clearReserveLatch();

// "Reservoir refilled to the marker" — only while the sensor is not reading.
// Returns nullptr on success, otherwise an error message.
const char* reserveRefilled();

#endif // RESERVE_CONTROLLER_H
