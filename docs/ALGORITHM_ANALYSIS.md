# Analiza algorytmu ATO — wnioski robocze

> Plik aktualizowany na bieżąco podczas pracy. Ostatnia aktualizacja: sesja 2026-03-12

---

## 1. Wartości startowe (domyślne — FRAM pusty / nowy magic)

### PumpSettings (config.h)
| Parametr | Wartość | Uwagi |
|---|---|---|
| `volumePerSecond` | **1.0 ml/s** | Wymaga kalibracji przed uruchomieniem! |
| `manualCycleSeconds` | 60 s | Czas ręcznego cyklu z GUI |
| `calibrationCycleSeconds` | 30 s | Czas cyklu kalibracyjnego |

### TopOffConfig (algorithm_config.h) — magic 0xA7
| Parametr | Wartość | Znaczenie |
|---|---|---|
| `dose_ml` | **150 ml** | Stała dawka wody na jeden cykl (dawniej `max_dose_ml` z dynamicznym dozowaniem) |
| `daily_limit_ml` | **2000 ml** | Limit dobowy — przekroczenie → STATE_ERROR (wymaga resetu) |
| `ema_alpha` | **0.20** | Wygładzanie EMA (nowa wartość waży 20%) |
| `rate_yellow_sigma` | **150** (1.5×) | Alert żółty — odchylenie w jednostkach sigma × 100 |
| `rate_red_sigma` | **250** (2.5×) | Alert czerwony → STATE_ERROR (wymaga resetu) |
| `history_window_s` | 86400 s | Okno rolling 24h |
| `available_ml` | **0** | Woda pozostała w zbiorniku [ml] — dekrementowana po każdym cyklu |
| `available_max_ml` | **0** | Pojemność zbiornika przy ostatnim uzupełnieniu [ml]; 0 = funkcja nieaktywna |
| `DEFAULT_MIN_BOOTSTRAP` | **5** | Min. dolewek zanim alerty sigma są aktywne |

### Debouncing wejściowy
| Parametr | Wartość | Efektywny czas |
|---|---|---|
| `DEBOUNCE_INTERVAL_MS` | 200 ms | — |
| `DEBOUNCE_COUNTER` | 5 | 5 × 200 ms = **1000 ms** |

### Czas pracy pompy
```
doseTimeS = dose_ml / volumePerSecond
= 150 / 1.0 = 150 s (kalibracja wymagana)
Po kalibracji np. 3 ml/s: 150 / 3.0 = 50 s
```

---

## 2. Maszyna stanów (aktualna)

```
STATE_IDLE
    |  (czujnik wyzwolony przez water_sensors)
    v
STATE_DEBOUNCING   ← obsługiwany przez water_sensors (callback onDebounceComplete)
    |
    v
STATE_PUMPING      ← pompa startuje na calculateDoseTime(dose_ml, volumePerSecond)
    |  (isPumpActive() == false → cykl zakończony)
    v
STATE_LOGGING      ← zapis do FRAM (TopOffRecord + EmaBlock + TopOffConfig)
    |
    v
STATE_IDLE

Wyjątki ze stanów:
    STATE_PUMPING/LOGGING → daily_limit przekroczony → STATE_ERROR
    STATE_PUMPING/LOGGING → red alert (sigma ≥ rate_red_sigma) → STATE_ERROR
    STATE_ERROR ← oczekuje resetu przez GUI lub przycisk
    STATE_MANUAL_OVERRIDE ← niezależna ścieżka (handleDirectPumpOn)
```

**Kluczowa zmiana vs stara wersja:**
- Brak faz `STATE_TRYB_1_WAIT`, `STATE_TRYB_1_DELAY`, `STATE_TRYB_2_VERIFY`, `STATE_TRYB_2_WAIT_GAP2`
- Brak weryfikacji zwolnienia czujnika po dolewce (`updateReleaseVerification` usunięte)
- Stała dawka — pompa pracuje dokładnie `doseTimeS` sekund, nie "do zwolnienia czujnika"

---

## 3. Mechanizm EMA — co śledzi (aktualna wersja)

### Trzy śledzane wartości
- `ema_rate_ml_h` — typowe tempo zużycia wody [ml/h] = `dose_ml / interval_s × 3600`
- `ema_interval_s` — typowy czas między dolewkami [s]
- `ema_dev_ml_h` — EMA absolutnych odchyleń od `ema_rate`: `EMA(|rate - ema_rate|)` — podstawa dynamicznych progów

> **ema_volume_ml usunięta** — stała dawka (`dose_ml`) jest zawsze taka sama, śledzenie objętości EMA nie wnosi informacji.

### Wzory
```
# Rate bieżącego cyklu (od 2. dolewki gdy interval > 0):
rate = dose_ml / interval_s × 3600

# EMA (pierwsze zdarzenie — bootstrap):
ema_rate = rate (bezpośrednie)

# EMA kolejne:
ema_rate_new = alpha × rate + (1 - alpha) × ema_rate_old
ema_dev_new  = alpha × |rate - ema_rate_old| + (1 - alpha) × ema_dev_old
```

### Progi alertów (dynamiczne, oparte na sigma)
```
# Odchylenie w jednostkach sigma (× 100, przechowywane jako int8_t):
dev_sigma = (rate - ema_rate) / ema_dev × 100

# Próg żółty (GUI warning only):
|dev_sigma| >= rate_yellow_sigma   →  alert_level = 1

# Próg czerwony (ERROR state, wymaga resetu):
|dev_sigma| >= rate_red_sigma      →  alert_level = 2 → STATE_ERROR
```

**Zalety dynamicznych progów:**
- Progi automatycznie dostosowują się do naturalnej zmienności środowiska
- Mała zmienność → wąskie progi (czulszy alarm)
- Duża zmienność → szerokie progi (mniej fałszywych alarmów)
- `rate_yellow_sigma=150` ≈ 1.5σ, `rate_red_sigma=250` ≈ 2.5σ

### Przykład (alpha=0.20, dose=150ml)
| Dolewka # | interval_s | rate [ml/h] | ema_rate | ema_dev | dev_sigma | Alert |
|---|---|---|---|---|---|---|
| 1 | 0 | 0 | 0 | 0 | 0 | bootstrap |
| 2 | 3600 | 150 | 150 | 0 | 0 | bootstrap |
| 3 | 3600 | 150 | 150 | 0 | 0 | bootstrap |
| 4 | 3720 | 145 | 149 | 1 | -400 | bootstrap |
| 5 | 3480 | 155 | 150 | 2 | +250 | bootstrap |
| 6 | 1200 | **450** | 210 | 62 | **+387** | **CZERWONY** |

> Po wyjściu z bootstrapu (5 dolewek) już pierwsza anomalia (3× szybszy odpływ) wywołuje RED ALERT.

### Zbieżność EMA (bootstrap)
Alerty są **wyłączone** gdy `bootstrap_count <= DEFAULT_MIN_BOOTSTRAP (5)`.
Po 5 dolewkach EMA ma wystarczająco dużo danych by progi sigma były wiarygodne.

---

## 4. Struktura danych FRAM

### TopOffRecord (20 bajtów) — ring buffer 60 rekordów
```cpp
struct TopOffRecord {
    uint32_t timestamp;       // Unix timestamp UTC
    uint32_t interval_s;      // Czas od poprzedniej dolewki [s]
    float    rate_ml_h;       // Tempo zużycia [ml/h]
    uint16_t cycle_num;       // Sekwencyjny numer cyklu
    int8_t   dev_rate_sigma;  // Odchylenie tempa w jednostkach sigma × 100
    uint8_t  alert_level;     // 0=OK, 1=ŻÓŁTY, 2=CZERWONY
    uint8_t  hour_of_day;     // Godzina lokalna (0–23) — rytm dobowy
    uint8_t  flags;           // FLAG_MANUAL, FLAG_BOOTSTRAP, FLAG_DAILY_LIM, FLAG_RED_ALERT
    uint8_t  _pad[2];
};
```

### EmaBlock (16 bajtów)
```cpp
struct EmaBlock {
    float   ema_rate_ml_h;    // EMA tempa [ml/h]
    float   ema_interval_s;   // EMA interwału [s]
    float   ema_dev_ml_h;     // EMA absolutnych odchyleń — podstawa dynamicznych progów
    uint8_t bootstrap_count;  // Liczba zebranych dolewek (cap: 255)
    uint8_t _pad[3];
};
```

### TopOffConfig (20 bajtów) — magic 0xA7
```cpp
struct TopOffConfig {
    float    ema_alpha;           // [0.10–0.40]
    uint32_t history_window_s;    // okno rolling [s]
    uint16_t dose_ml;             // stała dawka [ml]
    uint16_t daily_limit_ml;      // limit dobowy [ml]
    uint8_t  rate_yellow_sigma;   // próg żółty [%sigma]
    uint8_t  rate_red_sigma;      // próg czerwony [%sigma]
    uint8_t  is_configured;       // 0xA7 = ważna konfiguracja
    uint8_t  _pad[1];
    uint16_t available_ml;        // woda w zbiorniku — bieżąca [ml]
    uint16_t available_max_ml;    // woda w zbiorniku — przy ostatnim uzupełnieniu [ml]
};
```

### Adresy FRAM (0x0500+)
| Adres | Rozmiar | Zawartość |
|---|---|---|
| 0x0560 | 20 B | TopOffConfig |
| 0x0574 | 2 B | checksum konfiguracji |
| 0x0580 | 16 B | EmaBlock |
| 0x0590 | 2 B | checksum EMA |
| 0x0600 | 2 B | TOPOFF_COUNT (liczba rekordów) |
| 0x0602 | 2 B | TOPOFF_WPTR (write pointer) |
| 0x0610 | 1200 B | ring buffer 60 × TopOffRecord |

---

## 5. Śledzenie wody w zbiorniku (available_ml)

### Zasada działania
- Użytkownik wpisuje w GUI ile ml wody jest w zbiorniku i klika "Set / Refill"
- System zapisuje tę wartość jako `available_ml` i `available_max_ml`
- Po każdym automatycznym cyklu: `available_ml -= dose_ml` (min 0)
- Jeśli `available_max_ml == 0` — funkcja nieaktywna (nie dekrementuje)
- Kolumna "Available Water" w GUI pokazuje `available_ml / available_max_ml ml`

### API
- `GET /api/available-volume` → `{available_ml, available_max_ml}`
- `POST /api/set-available-volume` → ustaw oboje na `value` (100–20000 ml)
- `POST /api/refill-available-volume` → resetuj `available_ml = available_max_ml`

---

## 6. Mechanizm ERROR state

Każdy niezerowy `ErrorCode` → `STATE_ERROR`, bezwarunkowy, wymaga resetu.

| Kod | Przyczyna | Kiedy |
|---|---|---|
| `ERROR_DAILY_LIMIT` | `rolling_24h_ml >= daily_limit_ml` | po finalizacji cyklu |
| `ERROR_RED_ALERT` | `dev_sigma >= rate_red_sigma` i po bootstrapie | po finalizacji cyklu |
| `ERROR_BOTH` | oba jednocześnie | po finalizacji cyklu |

Reset przez: przycisk GPIO (krótkie naciśnięcie) lub `POST /api/system-reset` z GUI.

---

## 7. Kwestie do obserwacji w produkcji

### Kalibracja pompy (priorytet #1)
Domyślne `volumePerSecond = 1.0 ml/s` powoduje błędnie wyliczony `doseTimeS`.
```
Procedura: Pump → Manual (60s) → zmierz fizycznie ml → volumePerSecond = ml / 60
Zapisz w GUI: Pump Settings → volume per second
```

### Pierwsze 5 dolewek — bootstrap
Przez pierwsze 5 cykli rekord jest oznaczony `FLAG_BOOTSTRAP`.
Alerty sigma nie są aktywne — `ema_dev_ml_h` nie ma jeszcze wystarczającej historii.

### Wzrost ema_dev po wielu normalnych dolewkach
Jeśli środowisko jest bardzo stabilne (mała ema_dev), progi sigma mogą być wąskie
i alerty żółte mogą pojawiać się przy niewielkich wahaniach temperatury/parowania.
Można podnieść `rate_yellow_sigma` przez API (`/api/pump-settings` — do rozbudowy).

### rolling_24h reset przy zmianie magic FRAM
Zmiana `TOPOFF_CONFIG_MAGIC` (np. z 0xA6 na 0xA7) kasuje `available_ml/max` do domyślnych (0).
Ring buffer i EMA są kasowane oddzielnie przez `isTopOffRingBufferValid()` — tylko gdy `count > 60`.
Przy normalnej pracy ring buffer i EMA przeżywają zmianę magic (są chronione osobnymi checksumami).





❯ Wróćmy do wykresu zdarzeń i EMA Opisz mi jak algorytm liczy pozycje(y) punktów na osi oraz wartość EMA po każdym dolaniu                      

  Searched for 1 pattern, read 1 file (ctrl+o to expand)

● Mam pełny obraz. Teraz mogę dokładnie opisać całą mechanikę.

  ---
  Oś X — pozycja pozioma

  Każdy punkt ląduje na osi czasu wg swojego timestamp (Unix UTC z RTC):

  function xOf(ts) { return p.l + (ts - minTs) / tsRange * cw; }

  minTs = timestamp najstarszego punktu (lub maxTs - 6h jeśli historia krótsza). Punkty są rozmieszczone proporcjonalnie do czasu — nie do
  numeru cyklu.

  ---
  Oś Y — pozycja pionowa (rate_ml_h)

  function yOf(v) { return p.t + (1 - (v - yMin) / (yMax - yMin)) * ch; }

  Zakres [yMin, yMax] jest dynamiczny — obliczany przed rysowaniem:

  rates = visPts.filter(rate > 0).map(rate_ml_h)
  yMin = min(wszystkich rate)
  yMax = max(wszystkich rate)

  // Rozszerzenie o strefy EMA (jeśli EMA dojrzała):
  yMin = min(yMin, max(0, ema_rate - red_sigma/100 * ema_dev))
  yMax = max(yMax, ema_rate + red_sigma/100 * ema_dev)

  // Padding 18%:
  rpad = (yMax - yMin) * 0.18
  yMin = max(0, yMin - rpad)
  yMax += rpad

  Każdy punkt to po prostu:
  rate_ml_h = dose_ml / interval_s × 3600
  czyli 150 ml / 300 s × 3600 = 1800 ml/h dla 5-minutowego interwału.

  ---
  EMA — jak jest liczona po każdym dolaniu

  W finishPumpCycle() → updateEMA(intervalS, rateMlH):

  // Zdarzenie 1 (bootstrap_count == 0):
  ema_rate  = rateMlH          // inicjalizacja bezpośrednio
  ema_interval = intervalS
  ema_dev   = 0.0

  // Zdarzenie 2+ (bootstrap_count > 0):
  dev           = |rateMlH - ema_rate|         // odchylenie absolutne
  ema_rate      = α × rateMlH  + (1-α) × ema_rate
  ema_interval  = α × intervalS + (1-α) × ema_interval
  ema_dev       = α × dev       + (1-α) × ema_dev

  // α = 0.30 (default)
  bootstrap_count++

  Na wykresie rysowana jest pozioma przerywana linia na stałej wysokości yOf(ema_rate) — ta sama wartość dla całego zakresu czasu. EMA nie jest
  historyzowana per-punkt, to tylko aktualny stan z pamięci.

  ---
  Problem który widziałeś na wykresie — liczbowo

  Przykład: normalny interwał 4h → rate = 150/14400×3600 = 37.5 ml/h

  ┌───────────┬──────────┬──────┬────────────────────────────────────┐
  │ Zdarzenie │ interval │ rate │         ema_rate (α=0.30)          │
  ├───────────┼──────────┼──────┼────────────────────────────────────┤
  │ 1         │ 4h       │ 37.5 │ 37.5 (init)                        │
  ├───────────┼──────────┼──────┼────────────────────────────────────┤
  │ 2         │ 4h       │ 37.5 │ 37.5                               │
  ├───────────┼──────────┼──────┼────────────────────────────────────┤
  │ 3         │ 5 min    │ 1800 │ 0.3×1800 + 0.7×37.5 = **566 ml/h** │
  ├───────────┼──────────┼──────┼────────────────────────────────────┤
  │ 4         │ 15 min   │ 600  │ 0.3×600 + 0.7×566 = **576 ml/h**   │
  ├───────────┼──────────┼──────┼────────────────────────────────────┤
  │ 5         │ 4h       │ 37.5 │ 0.3×37.5 + 0.7×576 = **414 ml/h**  │
  └───────────┴──────────┴──────┴────────────────────────────────────┘

  Punkt 5 (rate=37.5) jest teraz daleko poniżej EMA=414 → na wykresie wygląda jak przepaść, choć to całkowicie normalna dolewka. EMA będzie
  wracać do baseline przez wiele cykli.

✻ Cogitated for 33s
