#ifndef ALGORITHM_CONFIG_H
#define ALGORITHM_CONFIG_H
#include <Arduino.h>

// ============================================================
// DEBOUNCING CZUJNIKA WEJŚCIOWEGO
// ============================================================
#define DEBOUNCE_INTERVAL_MS     200   // ms między próbkowaniami
#define DEBOUNCE_COUNTER           5   // kolejnych LOW wymaganych (= 1000 ms efektywnie)

// ============================================================
// LOGGING
// ============================================================
#define LOGGING_TIME                 5  // s — czas fazy logowania po cyklu (bez błędu)

// ============================================================
// CZUJNIK DOSTĘPNOŚCI WODY (AVAILABLE_WATER_SENSOR_PIN)
// ============================================================
#define LOW_RESERVOIR_CRITICAL_COUNT  4

// ============================================================
// DOMYŚLNE WARTOŚCI KONFIGURACJI ALGORYTMU
// ============================================================
#define DEFAULT_DOSE_ML             150     // Stała dawka wody na jeden cykl [ml]
#define DEFAULT_DAILY_LIMIT_ML     2000     // Limit dobowy [ml]
#define DEFAULT_EMA_ALPHA          0.20f    // Współczynnik wygładzania EMA [0.05–0.50]
#define DEFAULT_EMA_CLAMP          0.40f    // Maks. odchylenie EMA na krok [0.10–0.80]
#define DEFAULT_ALARM_P1           0.10f    // Waga skoku w alarm_score [0.01–2.0]
#define DEFAULT_ALARM_P2           0.80f    // Prędkość zaniku score [0.05–0.99]
#define DEFAULT_ZONE_GREEN         20.0f    // Próg score zielony/żółty
#define DEFAULT_ZONE_YELLOW        50.0f    // Próg score żółty/czerwony
#define DEFAULT_INITIAL_EMA        20.0f    // Seed EMA przed pierwszą dolewką [ml/h]
#define DEFAULT_HISTORY_WINDOW_S  86400     // Okno historii [s] — 24h

// ============================================================
// SPRAWDZENIA INTEGRALNOŚCI STAŁYCH
// ============================================================
static_assert(DEBOUNCE_INTERVAL_MS >= 50 && DEBOUNCE_INTERVAL_MS <= 2000,
    "DEBOUNCE_INTERVAL_MS must be 50-2000 ms");
static_assert(DEBOUNCE_COUNTER >= 2 && DEBOUNCE_COUNTER <= 20,
    "DEBOUNCE_COUNTER must be 2-20");
static_assert(DEFAULT_DOSE_ML >= 50 && DEFAULT_DOSE_ML <= 2000,
    "DEFAULT_DOSE_ML must be 50-2000 ml");

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
// KONFIGURACJA ALGORYTMU — persystowana w FRAM, 40 bajtów
// ============================================================
struct TopOffConfig {
    float    ema_alpha;           // Współczynnik EMA [0.05–0.50]
    float    ema_clamp;           // Maks. odchylenie EMA na krok [0.10–0.80]
    float    alarm_p1;            // Waga skoku (P1) [0.01–2.0]
    float    alarm_p2;            // Prędkość zaniku (P2) [0.05–0.99]
    float    zone_green;          // Próg score → zielony/żółty
    float    zone_yellow;         // Próg score → żółty/czerwony
    float    initial_ema;         // Seed EMA przy pierwszym starcie [ml/h]
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

inline uint16_t calculateDoseTime(uint16_t doseMl, float flowRateMlS) {
    if (flowRateMlS <= 0.01f) return 60;
    uint16_t t = (uint16_t)(doseMl / flowRateMlS);
    return (t < 1) ? 1 : t;
}

#endif // ALGORITHM_CONFIG_H
