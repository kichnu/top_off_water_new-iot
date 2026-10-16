#ifndef RESERVE_CONTROLLER_H
#define RESERVE_CONTROLLER_H

#include <Arduino.h>

// Source-water reserve = portion counter, in both modes (level sensor on/off).
// Counts ml remaining in the reservoir from the geometry capacity (whole portions,
// ReservoirLevels::fullMl, hidden safety bottom included); every portion — ATO,
// manual, ml from a direct pump run — is subtracted. The user sees whole portions
// above the hidden bottom; 0 portions = no water → alarm, no new portion starts.
// The reservoir is always refilled up to the marker ("Reserve refilled" button).
// With the level sensor on, the refill is verified by a measurement that also
// becomes the reference for the dose check (step 3b).

#define RESERVE_STATE_MAGIC        0xC5
#define RESERVE_FLAG_REF_VALID     0x01   // ref_dist_x10 / ref_ts hold a sensor reference

// Persisted in FRAM (FRAM_ADDR_RESERVE_STATE)
struct ReserveState {
    uint32_t remaining_ml;    // from fullMl down; hidden bottom included
    uint32_t ref_ts;          // unix time of the sensor reference
    uint16_t ref_dist_x10;    // sensor → water at the reference [0.1 mm]
    uint8_t  flags;           // RESERVE_FLAG_*
    uint8_t  magic;           // RESERVE_STATE_MAGIC = valid block
    uint8_t  _reserved[4];    // dose-check block (step 3b)
};
static_assert(sizeof(ReserveState) == 16, "ReserveState must be exactly 16 bytes");

enum ReserveZone : uint8_t {
    RESERVE_ZONE_OK      = 0,
    RESERVE_ZONE_LOW     = 1,   // in the yellow reserve range — information only
    RESERVE_ZONE_EMPTY   = 2,   // 0 portions → alarm, pump blocked
    RESERVE_ZONE_UNKNOWN = 3,   // reservoir not configured
};

// Reserve as shown to the user (GUI, display)
struct ReserveView {
    bool        sensorCheck;    // level sensor on (dose check), else counting only
    uint32_t    ml;             // usable ml (above the hidden bottom)
    uint32_t    capacityMl;     // usable capacity (whole portions)
    uint32_t    lowMl;          // reserve range (whole portions): zone LOW at or below it
    uint16_t    portions;       // whole portions left
    float       pct;            // ml / capacity × 100
    float       waterMm;        // last valid sensor reading as water height (0 = none)
    ReserveZone zone;
};

// doseMl — portion size used to round the capacity and thresholds
ReserveView getReserveView(uint16_t doseMl);

// Init — loads the state from FRAM (call after initReservoirConfig)
void initReserveController();

// Call every loop() iteration — drives the refill measurement
void updateReserveController();

bool isReserveLow();     // in the reserve range, not empty
bool isReserveEmpty();   // 0 whole portions → alarm, no new portion

// Subtracts ml after a pump run — clamps to 0, saves to FRAM
void subtractReserveMl(uint16_t ml);

uint32_t getReserveUsableMl();               // ml above the hidden bottom (current dose)
void     setReserveReference(float distanceMm);   // new sensor reference (e.g. after calibration)

// ---- Refill ("Reserve refilled") ----

enum RefillPhase : uint8_t {
    REFILL_IDLE      = 0,
    REFILL_MEASURING = 1,   // waiting for a stable level
    REFILL_RESULT    = 2,   // result shown, waiting for the user's decision
};

enum RefillResult : uint8_t {
    REFILL_RES_NONE       = 0,
    REFILL_RES_OK         = 1,   // A — within the marker tolerance
    REFILL_RES_MISMATCH   = 2,   // B — valid reading outside the tolerance
    REFILL_RES_NO_READING = 3,   // C — no valid / stable reading
};

struct RefillStatus {
    RefillPhase  phase;
    RefillResult result;
    uint8_t      series;         // valid series in the window
    uint32_t     elapsedS;       // since the measurement started
    float        distanceMm;     // mean sensor → water
    float        expectedMm;     // sensor → water at the marker
    float        deviationMm;    // water level − marker (+ above the marker)
    int32_t      deviationMl;
    float        spreadMm;       // max − min in the window
    bool         sensorSuspect;  // hint: the reading itself looks wrong
    const char*  note;           // reason / hint (nullptr = none)
};

// Starts the refill. Sensor off → counter full at once (returns false).
// Sensor on → measurement started (returns true). Pump running → error.
bool startReserveRefill(const char** err);

// Decision on the result: "confirm" (A/B), "no_sensor" (any result: counter full,
// level sensor switched off), "remeasure", "cancel". nullptr = OK, else error.
const char* reserveRefillAction(const String& action);

const RefillStatus& getRefillStatus();
bool isReserveRefillActive();   // measuring or result pending — ATO pump blocked

// Counter full after a refill — true once, for clearing the reserve-empty error
bool takeReserveRefilled();

#endif // RESERVE_CONTROLLER_H
