# Plan wdrożenia: VL53L4CD jako monitor poziomu wody w rezerwie

Wersja 2 (2026-10-13) — po przejściu na ESP32-C3 i nową płytkę. Wersja 1
(2026-09-16, ESP32-C6, pływak rezerwy jako checkpoint) — w historii gita.
Testy fizyczne czujnika: `~/Dokumenty/My_apps/TOOLS/distance_sensor_pololu/WNIOSKI.md`.

## 1. Cel

Ciągły pomiar objętości wody w zbiorniku rezerwy (zasilającym pompę ATO) na
podstawie odległości z góry do lustra wody. Dodatkowo: pomiar rzeczywistej
dawki każdego cyklu i (eksperymentalnie) korekta wydajności pompy.

## 2. Sprzęt

- VL53L4CD (Pololu #3692, biblioteka `pololu/VL53L4CD`) na wspólnym I2C z RTC
  i FRAM: SDA GPIO 5, SCL GPIO 6, 100 kHz. Adres 0x29, XSHUT/INT
  niepodłączone.
- Pływak rezerwy usunięty z płytki i z kodu (commit `9bb6758`). ToF jest
  jedynym czujnikiem rezerwy.
- Montaż: ~100 mm nad lustrem przy pełnym zbiorniku, zbiornik ~300 mm →
  zakres pomiaru ~100–400 mm. Przekrój stały na całej wysokości.
- Fizyczny znacznik poziomu na zbiorniku (sekcja 8).

## 3. Stan wody — model

- `g_reserveMl` w FRAM prowadzony stale (jak dziś): po każdym cyklu
  odejmowane `q_est · t_pump`.
- Gdy czujnik jest sprawny, wartość jest nadpisywana pomiarem — **pomiar ma
  priorytet** (spadek poziomu jest fizycznie deterministyczny, błąd tylko po
  stronie pomiaru).
- Czujnik niesprawny (brak ACK / `LEVEL_FAULT_SERIES` złych serii z rzędu) →
  tryb licznika + ostrzeżenie w GUI „czujnik poziomu niesprawny”. Progi
  działają dalej na liczniku.
- Mapowanie liniowe: `ml = capacity_ml · clamp(d_empty − d, 0, range) / range`,
  `range = d_empty − d_full`.

## 4. Progi

Dwa konfigurowalne progi (% pojemności), z histerezą:
- **warning** — żółty status, buzzer WARNING;
- **error** — blokada pompy (auto + ręczna), czerwony status, buzzer ALARM.

(Tymczasowo w liczniku: warning ≤ 20% `RESERVE_WARN_PCT`, error = 0 ml.)

## 5. Pomiar dawki i korekta pompy (filtr Kalmana)

Pompa wirnikowa: zmiany wydajności powolne/systematyczne (wysokość
podnoszenia, zużycie, filtr) albo skokowe-awaryjne (zapowietrzenie,
zatkanie). Szum pomiaru — losowy, bez pamięci między cyklami. Pojedynczy
cykl: σ(Δh) ~0,7 mm ≈ 15% dawki 150 ml (przy ~33 ml/mm) — tego rzędu co
niepewność pompy, więc rozdzielenie daje dopiero filtracja w czasie.

Pomiar wokół cyklu:
1. seria `h_before` tuż przed startem pompy,
2. pompa (bez pomiaru),
3. uspokojenie 20–30 s (fale, cofka z węża), seria `h_after`,
4. `V_meas = A · (h_after − h_before)`, `σ_meas` z SEM obu serii.

Wybrany wariant: **skalarny Kalman na `q` [ml/s]**:
- stan `q_est`, `P`; start z ręcznej kalibracji `volume_per_second`;
- predykcja `P += Q` (Q — tempo dryfu pompy, jedyny parametr strojenia);
- obserwacja `q_obs = V_meas / t_pump`, `R = (σ_meas / t_pump)²`;
- bramka: `|q_obs − q_est| > 3·√(P+R)` → odrzut + flaga anomalii:
  - spadek ~0 → warning „pompa nie podaje”, K z rzędu → error z blokadą;
  - spadek ≫ oczekiwany → warning (wyciek / syfon / ręczny pobór);
- czas dawki = `dose_ml / q_est`.

Bezpieczeństwo (twarde, niezależne od filtra):
- `q_est` clamp do [0,7 … 1,3] × kalibracja ręczna → max ~1,43× czasu;
- twardy limit czasu jednego cyklu;
- limit dobowy liczony ze **zmierzonej** objętości, nie zadanej;
- **tryb cień** (przełącznik w settings): filtr liczy i loguje, czas pompy
  bez zmian — włączenie korekty dopiero po obserwacji logów.

Etap późniejszy (eksperyment): model `q = a + b·h` (RLS) — zależność
wydajności od poziomu w rezerwie.

## 6. Wykrywanie dolewania wody do rezerwy

Woda nie przybywa sama → wzrost poziomu bez pracy pompy = dolewanie.

```
STABLE --(wzrost > próg w 2 kolejnych seriach)--> REFILLING
REFILLING: blokada cyklu ATO, pomiar co 2–5 s
REFILLING --(zmiana < próg przez T_stable ~30 s)--> STABLE
   → nowa linia bazowa, log "+X ml", auto-zdjęcie warning/error
     (poziom > próg + histereza)
```

- Dolewanie w trakcie cyklu ATO → `h_after > h_before` → cykl klasyfikowany
  jako dolewanie, nie trafia do filtra.
- Spadek bez pompy (wyciek, pobór ręczny) → log + warning powyżej progu.

## 7. Harmonogram pomiaru

- Nieblokujący: jedna próbka na iterację `loop()`, seria N próbek
  (`status != 0` odrzucane, pierwsza próbka po bezczynności pomijana).
- Wyzwalacze: cykliczny (bring-up 10 s, docelowo 60 s), przed/po cyklu ATO,
  szybki w stanie REFILLING, on-demand z GUI.
- Start: N=10, budżet 50 ms. Finalne wartości po testach na zbiorniku.

## 8. Znacznik fizyczny — weryfikacja kalibracji

Na zbiorniku fizyczny znacznik poziomu o znanej objętości (`marker_ml`).
Gdy woda stoi na znaczniku, użytkownik klika w dashboardzie „poziom na
znaczniku” → system porównuje bieżący pomiar z zapamiętanym `d_marker`:
- rozjazd w granicy tolerancji → potwierdzenie kalibracji (log, timestamp);
- rozjazd większy → ostrzeżenie (przesunięty czujnik, kondensacja na
  soczewce) + opcja korekty offsetu.

Pierwsze kliknięcie (brak `d_marker`) zapisuje referencję.

## 9. FRAM

Wolne: 0x05A2–0x05FF. Nowy blok `ReserveLevelConfig` (magic inny niż 0xA8
TopOffConfig, checksum osobno): `d_empty_mm`, `d_full_mm`, `capacity_ml`,
`d_marker_mm`, `marker_ml`, progi warning/error, N, interwał, tryb korekty.
Osobny mały blok stanu filtra: `q_est`, `P`. Dokładny layout przy
implementacji kroku 2.

## 10. API / GUI (sekcja settings)

Form-encoded POST (konwencja projektu):
- `POST /api/reserve-level/calibrate-empty`, `.../calibrate-full`,
  `.../set-capacity`, `.../marker`, `.../config` (progi, N, interwał, tryb);
- `GET /api/reserve-level/status`.
Diagnostyka bring-up już w `/api/status` (`level_*`).
Dashboard: poziom rezerwy w ml i %, stan czujnika, stan REFILLING.

## 11. Kolejność wdrożenia

1. ✅ Sterownik + seria pomiarowa + diagnostyka (`level_sensor.*`, log +
   `/api/status`). **Weryfikacja: odbicie od realnej wody** (`signal_kcps`,
   `rej`, `sd`).
2. FRAM `ReserveLevelConfig` + kalibracja empty/full/capacity (API).
3. Podłączenie do `reserve_controller` (ml z pomiaru, fallback licznik,
   progi z histerezą, stan czujnika w GUI).
4. Wykrywanie dolewania (REFILLING) + blokada cyklu.
5. Pomiar dawki wokół cyklu + Kalman w trybie cień + limit dobowy z pomiaru.
6. Znacznik fizyczny.
7. Włączenie korekty pompy (po obserwacji), ewentualnie model RLS.

## 12. Otwarte

- Finalne N, budżet, interwał — po testach.
- Q (dryf pompy), K (anomalie z rzędu), progi wykrywania dolewania — po
  zebraniu danych z trybu cień.
