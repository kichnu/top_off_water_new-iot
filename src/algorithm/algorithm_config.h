#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H
#include <Arduino.h>

// ============================================================
// DEBOUNCING CZUJNIKA WEJŚCIOWEGO
// ============================================================
#define DEBOUNCE_INTERVAL_MS     1000   // ms między próbkowaniami
#define DEBOUNCE_COUNTER           10   // kolejnych LOW wymaganych 

// ============================================================
// LOGGING
// ============================================================
#define LOGGING_TIME                 5  // s — czas fazy logowania po cyklu (bez błędu)

// ============================================================
// DOMYŚLNE WARTOŚCI KONFIGURACJI ALGORYTMU
// ============================================================
#define DEFAULT_DOSE_ML             150     // Stała dawka wody na jeden cykl [ml]
#define DEFAULT_DAILY_LIMIT_ML     2000     // Limit dobowy [ml]
#define DEFAULT_ZONE_GREEN         20.0f    // Próg score zielony/żółty
#define DEFAULT_ZONE_YELLOW        50.0f    // Próg score żółty/czerwony
#define DEFAULT_INITIAL_EMA        20.0f    // Seed EMA przed pierwszą dolewką [ml/h]
#define DEFAULT_HISTORY_WINDOW_S  86400     // Okno historii [s] — 24h

// ============================================================
// EMA + ALARM SCORE CONSTANTS (fixed after field testing, not configurable)
// ============================================================
#define EMA_ALPHA                  0.20f    // EMA smoothing factor [0.05–0.50]
#define EMA_CLAMP                  0.40f    // Max EMA deviation per step [0.10–0.80]
#define ALARM_P1                   0.10f    // Spike weight in alarm_score [0.01–2.0]
#define ALARM_P2                   0.80f    // Score decay speed [0.05–0.99]

// ============================================================
// SPRAWDZENIA INTEGRALNOŚCI STAŁYCH
// ============================================================
static_assert(DEBOUNCE_INTERVAL_MS >= 50 && DEBOUNCE_INTERVAL_MS <= 2000,
    "DEBOUNCE_INTERVAL_MS must be 50-2000 ms");
static_assert(DEBOUNCE_COUNTER >= 2 && DEBOUNCE_COUNTER <= 20,
    "DEBOUNCE_COUNTER must be 2-20");
static_assert(DEFAULT_DOSE_ML >= 50 && DEFAULT_DOSE_ML <= 2000,
    "DEFAULT_DOSE_ML must be 50-2000 ml");
static_assert(EMA_ALPHA >= 0.05f && EMA_ALPHA <= 0.50f, "EMA_ALPHA must be 0.05-0.50");
static_assert(EMA_CLAMP >= 0.10f && EMA_CLAMP <= 0.80f, "EMA_CLAMP must be 0.10-0.80");
static_assert(ALARM_P1 >= 0.01f && ALARM_P1 <= 2.0f,    "ALARM_P1 must be 0.01-2.0");
static_assert(ALARM_P2 >= 0.05f && ALARM_P2 <= 0.99f,   "ALARM_P2 must be 0.05-0.99");

// ============================================================
// STANY ALGORYTMU
// ============================================================
enum AlgorithmState {
    STATE_IDLE = 0,
    STATE_DEBOUNCING,
    STATE_PUMPING,
    STATE_LOGGING,
    STATE_ERROR
};

// ============================================================
// KODY BŁĘDÓW
// ============================================================
enum ErrorCode {
    ERROR_NONE          = 0,
    ERROR_DAILY_LIMIT   = 1,
    ERROR_RED_ALERT     = 2,
    ERROR_BOTH          = 3,
    ERROR_LOW_RESERVOIR = 4
};

// ============================================================
// REKORD DOLEWKI — ring buffer FRAM, 20 bajtów
// Rozmiar: 4+4+4+2+1+1+1+1+2 = 20 bajtów
// ============================================================
struct TopOffRecord {
    uint32_t timestamp;       // Unix timestamp UTC
    uint32_t interval_s;      // Czas od poprzedniej dolewki [s]
    float    rate_ml_h;       // Tempo zużycia [ml/h]
    uint16_t cycle_num;       // Sekwencyjny numer cyklu
    int8_t   dev_rate_sigma;  // Nieużywane (zachowane dla spójności rozmiaru struktury, = 0)
    uint8_t  alert_level;     // 0=green 1=yellow 2=red (na podstawie alarm_score w chwili cyklu)
    uint8_t  hour_of_day;     // Godzina lokalna (0–23)
    uint8_t  flags;           // Bitmaska flag (patrz poniżej)
    uint16_t dose_ml_record;  // Dawka [ml] dla tego cyklu

    static const uint8_t FLAG_RED_ALERT = 0x08;
};
static_assert(sizeof(TopOffRecord) == 20, "TopOffRecord must be exactly 20 bytes");

// ============================================================
// BLOK EMA — persystowany w FRAM, 16 bajtów
// ============================================================
struct EmaBlock {
    float   ema_rate_ml_h;    // EMA tempa zużycia wody [ml/h]
    float   ema_interval_s;   // EMA typowego interwału między dolewkami [s]
    float   alarm_score;      // Skumulowany score alarmu (persystuje przez restart)
    uint8_t bootstrap_count;  // Liczba zebranych dolewek (informacyjnie)
    uint8_t _pad[3];
};
static_assert(sizeof(EmaBlock) == 16, "EmaBlock must be exactly 16 bytes");

// ============================================================
// ATO error latch + no-top-off watchdog reference — persisted in FRAM
// (survive the midnight restart and power loss), 12 bytes
// ============================================================
#define ALG_ERROR_STATE_MAGIC     0xE8
#define ALARM_DISARM_S            86400UL   // reset in the red zone disarms the anomaly alarm this long
#define RESET_HOLD_MS              3000UL   // reset button held this long (also the debounce)
#define NO_TOPOFF_ALARM_S         86400UL   // no auto top-off this long while the ATO could dose → critical alarm

struct AlgErrorState {
    uint8_t  last_error;          // ErrorCode, ERROR_NONE = no latched error
    uint8_t  magic;               // ALG_ERROR_STATE_MAGIC
    uint8_t  _pad[2];
    uint32_t disarm_until_ts;     // unix time; 0 = anomaly alarm armed
    uint32_t wd_ref_ts;           // no-top-off watchdog: last auto dose or end of a blocked period
};
static_assert(sizeof(AlgErrorState) == 12, "AlgErrorState must be exactly 12 bytes");

// ============================================================
// KONFIGURACJA ALGORYTMU — persystowana w FRAM, 40 bajtów
// ============================================================
struct TopOffConfig {
    float    _reserved[4];        // Formerly ema_alpha/ema_clamp/alarm_p1/alarm_p2 (now constants), keeps the FRAM layout
    float    zone_green;          // Score threshold green/yellow
    float    zone_yellow;         // Score threshold yellow/red
    float    initial_ema;         // EMA seed on first start [ml/h]
    uint32_t history_window_s;    // Okno historii [s]
    uint16_t dose_ml;             // Stała dawka [ml]
    uint16_t daily_limit_ml;      // Limit dobowy [ml]
    uint8_t  is_configured;       // 0xA8 = konfiguracja ważna
    uint8_t  _pad[3];
};
static_assert(sizeof(TopOffConfig) == 40, "TopOffConfig must be exactly 40 bytes");

#define TOPOFF_CONFIG_MAGIC  0xA8

// ============================================================
// POMOCNICZE OBLICZENIA
// ============================================================

// Portion time: getPortionMs() (hardware/pump_calibration.h)

#endif // ALGORITHM_CONFIG_H
