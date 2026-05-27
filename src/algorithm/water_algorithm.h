#ifndef WATER_ALGORITHM_H
#define WATER_ALGORITHM_H

#include "algorithm_config.h"
#include "../hardware/fram_controller.h"

// ============================================================
// WaterAlgorithm — maszyna stanów sterująca pompą top-off.
//
// Stany: IDLE → DEBOUNCING → PUMPING → LOGGING → IDLE
//                                     ↘ STATE_ERROR (daily limit / red alert)
//
// Algorytm alarmu (score-based):
//   Każda dolewka liczy odchylenie d = (rate - EMA) / EMA.
//   Przy d > 0: alarm_score rośnie (P1 × log2(1+d) × 100).
//   Przy d ≤ 0: alarm_score zanika (mnożnik P2).
//   Strefy: score < zone_green → zielony, < zone_yellow → żółty, else → czerwony.
//   Czerwona strefa → STATE_ERROR (pompa blokowana), alarm SOUND.
//   Po resecie: alarmArmed = false — alarm nie odpali ponownie dopóki
//   score nie spadnie poniżej zone_yellow (re-arm w zielonej/żółtej strefie).
// ============================================================

class WaterAlgorithm {
private:
    AlgorithmState currentState;

    // ---- Timing ----
    uint32_t stateStartMs;
    uint32_t pumpStartMs;

    // ---- Tracking interwałów ----
    uint32_t lastTopOffTimestamp;
    uint16_t totalCycleCount;

    // ---- Rolling 24h (cache) ----
    uint16_t rolling24hVolumeMl;
    uint32_t manualResetTs;

    // ---- Konfiguracja (FRAM lub defaults) ----
    TopOffConfig config;

    // ---- EMA + alarm score (persystowane w FRAM) ----
    EmaBlock ema;

    // ---- Alarm hystereza ----
    // true  = alarm może odpalić przy wejściu w czerwoną strefę
    // false = alarm wyciszony po resecie; re-arm gdy score < zone_yellow
    bool alarmArmed;

    // ---- Obsługa błędów ----
    ErrorCode lastError;

    // ---- Czujnik dostępności wody ----
    uint8_t lowReservoirCount;

    // ---- Flagi stanu ----
    bool systemWasDisabled;
    bool cycleLogged;

    // ---- Metody prywatne ----
    void handleSystemDisable();

    void startAutoPump();
    void finishPumpCycle();

    void checkAvailableWaterSensor();

    void    updateEMA(uint32_t intervalS, float rateMlH);
    uint8_t calculateAlertLevel() const;
    uint16_t scanRolling24h() const;

    void startErrorSignal(ErrorCode error);
    void checkResetButton();

    void loadConfigFromFRAM();
    void loadEmaFromFRAM();
    void saveEmaToFRAM();

public:
    WaterAlgorithm();

    void initFromFRAM();
    void update();

    // ---- Callbacki z water_sensors ----
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

    // ---- Czujnik dostępności wody ----
    uint8_t getLowReservoirCount()   const { return lowReservoirCount; }
    bool    isLowReservoirWarning()  const { return lowReservoirCount > 0 && lowReservoirCount < LOW_RESERVOIR_CRITICAL_COUNT; }

    // ---- Konfiguracja ----
    const TopOffConfig& getConfig() const { return config; }
    void setConfig(const TopOffConfig& cfg);

    // ---- EMA (tylko do odczytu dla API) ----
    const EmaBlock& getEma() const { return ema; }

    // ---- Alarm hystereza ----
    bool isAlarmArmed() const { return alarmArmed; }

    // ---- Reset / odzyskiwanie ----
    void resetFromError();
    bool resetSystem();
    bool resetCycleHistory();
    void resetRolling24h();

    // ---- System disable ----
    bool wasSystemDisabled() const { return systemWasDisabled; }
    void clearSystemWasDisabled()  { systemWasDisabled = false; }
};

extern WaterAlgorithm waterAlgorithm;

#endif // WATER_ALGORITHM_H
