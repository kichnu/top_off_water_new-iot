#ifndef WATER_ALGORITHM_H
#define WATER_ALGORITHM_H

#include "algorithm_config.h"
#include "../hardware/fram_controller.h"

// ============================================================
// WaterAlgorithm — state machine driving the top-off pump.
//
// States: IDLE → DEBOUNCING → PUMPING → LOGGING → IDLE
//                                     ↘ STATE_ERROR (reservoir / daily limit / red alert)
//
// Alarm algorithm (score-based):
//   Every top-off computes the deviation d = (rate - EMA) / EMA.
//   d > 0: alarm_score grows (P1 × log2(1+d) × 100).
//   d ≤ 0: alarm_score decays (multiplier P2).
//   Zones: score < zone_green → green, < zone_yellow → yellow, else → red.
//   Red zone → STATE_ERROR (pump blocked), alarm SOUND.
//   A reset of an error with the score in the red zone disarms the anomaly
//   alarm for ALARM_DISARM_S (24 h) or until the score drops below zone_yellow,
//   whichever comes first. The error and the disarm time are kept in FRAM.
//   One reset for the GUI and the button (held RESET_HOLD_MS).
// ============================================================

class WaterAlgorithm {
private:
    AlgorithmState currentState;

    // ---- Timing ----
    uint32_t stateStartMs;
    uint32_t pumpStartMs;

    // ---- Interval tracking ----
    uint32_t lastTopOffTimestamp;
    uint16_t totalCycleCount;

    // ---- Rolling 24h (cache) ----
    uint16_t rolling24hVolumeMl;
    uint32_t manualResetTs;
    uint32_t manualResetMs;   // millis() of the manual reset, 0 = none since boot

    // Doses since boot by millis() — the 24 h limit does not depend on the RTC
    // (the FRAM history is by unix time); the larger of the two counts
    struct RamDose { uint32_t ms; uint16_t ml; };
    RamDose  ramDoses[TOPOFF_HISTORY_SIZE];
    uint8_t  ramDoseCount;
    uint8_t  ramDoseNext;

    // ---- Configuration (FRAM or defaults) ----
    TopOffConfig config;

    // ---- EMA + alarm score (persisted in FRAM) ----
    EmaBlock ema;

    // ---- Anomaly alarm disarm after a reset ----
    // true  = the red zone raises ERROR_RED_ALERT
    // false = disarmed until disarmUntilTs or the score < zone_yellow
    bool     alarmArmed;
    uint32_t disarmUntilTs;   // unix time, 0 = armed

    // ---- No-top-off watchdog ----
    // Counts from the last auto dose, restarted at the end of a period in which
    // the ATO could not dose (user Service Mode, ERROR, reservoir alarm); the boot
    // Service Mode (every midnight restart) does not restart it
    uint32_t wdRefTs;         // unix time, 0 = not set yet
    bool     wdWasActive;
    bool     noTopOffAlarm;

    // ---- Error handling ----
    ErrorCode lastError;

    // ---- State flags ----
    bool systemWasDisabled;
    bool cycleLogged;

    // ---- Private methods ----
    void handleSystemDisable();

    void startAutoPump();
    void finishPumpCycle();

    void    updateEMA(uint32_t intervalS, float rateMlH);
    uint8_t calculateAlertLevel() const;
    uint16_t scanRolling24h() const;      // FRAM history by unix time
    uint16_t ramRolling24h() const;       // doses since boot by millis()
    uint16_t rolling24h() const;          // the larger of both

    void startErrorSignal(ErrorCode error);
    void checkResetButton();
    void saveErrorState();
    void loadErrorState();
    void checkNoTopOffWatchdog();

    void loadConfigFromFRAM();
    void loadEmaFromFRAM();
    void saveEmaToFRAM();

public:
    WaterAlgorithm();

    void initFromFRAM();
    void update();

    // ---- Callbacks from water_sensors ----
    void onDebounceStart();
    void onDebounceComplete();
    void onDebounceReset();

    // ---- Status ----
    AlgorithmState getState()            const { return currentState; }
    const char*    getStateString()      const;
    String         getStateDescription() const;
    bool           isInCycle()           const;
    uint32_t       getRemainingSeconds() const;
    ErrorCode      getLastError()        const { return lastError; }
    uint16_t       getRolling24hVolume() const { return rolling24hVolumeMl; }
    uint32_t       getPumpElapsedMs()    const;
    uint16_t       getTotalCycleCount()  const { return totalCycleCount; }

    // ---- Configuration ----
    const TopOffConfig& getConfig() const { return config; }
    void setConfig(const TopOffConfig& cfg);

    // ---- EMA (read-only, for the API) ----
    const EmaBlock& getEma() const { return ema; }

    // ---- Alarm hysteresis ----
    bool     isAlarmArmed()     const { return alarmArmed; }
    uint32_t getDisarmUntilTs() const { return disarmUntilTs; }

    // ---- No-top-off watchdog (critical alarm, no blocking) ----
    bool     isNoTopOffAlarm()  const { return noTopOffAlarm; }
    uint32_t getWatchdogRefTs() const { return wdRefTs; }

    // ---- Reset / recovery ----
    // The only reset (GUI and button): error cleared, pump stopped, reserve alarm
    // cleared when the level allows; false while logging a cycle
    bool resetSystem();
    bool resetCycleHistory();
    void resetRolling24h();

    // ---- System disable ----
    bool wasSystemDisabled() const { return systemWasDisabled; }
    void clearSystemWasDisabled()  { systemWasDisabled = false; }
};

extern WaterAlgorithm waterAlgorithm;

#endif // WATER_ALGORITHM_H
