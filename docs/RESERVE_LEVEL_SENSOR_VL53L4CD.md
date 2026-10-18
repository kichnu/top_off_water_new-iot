# Plan wdrożenia: VL53L4CD jako monitor poziomu wody w rezerwie

Wersja 5 (2026-10-17) — czujnik tylko informacyjnie (szacunek ±2 cm), stop pompy przy pustym
zbiorniku = pływak (GPIO 20), kalibracja pompy 4 przebiegi / średnia, licznik porcji i procedura
„Rezerwa uzupełniona” z pomiarem usunięte.
Wersja 4 (2026-10-15) — licznik porcji + kontrola dawek czujnikiem; wersje 1–3 — w historii gita.
Testy fizyczne czujnika: `~/Dokumenty/My_apps/TOOLS/distance_sensor_pololu/WNIOSKI.md`.

## ⚠ Przypomnienia (do zrobienia)

- **Pływak rezerwy wyłączony w firmware**: `RESERVE_FLOAT_ENABLED = 0` w
  `src/hardware/reserve_controller.h`. Po zamontowaniu pływaka na GPIO 20 (do GND, LOW = brak
  wody) ustawić na 1, zbudować, sprawdzić log startu („Reserve float switch on GPIO 20: HIGH”)
  i przetestować: opuszczenie pływaka → po 3 s alarm + blokada porcji; podniesienie → alarm
  trwa do Cycle Reset. Do tego czasu NIC nie zatrzymuje pompy przy pustym zbiorniku.
- **Test `LVLTEST` (`level_test.*`) zostaje** — narzędzie diagnostyczne czujnika; usunąć dopiero,
  gdy przestanie być potrzebny.

## 1. Cel

Informacja o ilości wody w zbiorniku zasilającym pompę ATO: czujnik odległości do lustra wody →
wysokość wody → objętość (pole dna × wysokość) → szacunkowa liczba porcji jako podpowiedź dla
użytkownika. Bez alarmów z czujnika. Zatrzymanie pompy przy pustym zbiorniku — pływak.

## 2. Sprzęt i zbiornik

- VL53L4CD (Pololu #3692, `pololu/VL53L4CD`) na wspólnym I2C (SDA GPIO 5, SCL GPIO 6,
  100 kHz), adres 0x29, XSHUT/INT niepodłączone. Bez osłony przed czujnikiem.
- Otwór w obudowie: cylindryczny Ø10 mm, krawędź zewnętrzna 5,5 mm od czujnika
  (Ø7 mm dawał 100% odrzuconych próbek).
- Zbiornik testowy: 282 × 202 mm, ścianki pionowe (56,96 ml/mm), czujnik 470 mm nad dnem,
  znacznik (maksymalny poziom) 380 mm od dna. Przegroda z małym otworem — 2 min ciszy
  pomiarowej po pompie (`LEVEL_PUMP_QUIET_MS`).
- Pływak rezerwy: GPIO 20 (UART0 RX — przy starcie wejście, nie strap), INPUT_PULLUP, LOW =
  brak wody. Nie GPIO 21: ROM wysyła tam log startowy (TX w stanie HIGH), zamknięty pływak do
  GND zwierałby wyjście.

## 3. Dokładność czujnika — wnioski z testów (2026-10-17)

- W obrębie serii i między seriami tuż po sobie: rozrzut < 1–3 mm. Między pomiarami w dłuższym
  odstępie: skokowe przesunięcia 5–30 mm przy nieruszanej wodzie.
- Lustro wody: odbicie lustrzane (sygnał ~7–10 Mcps) + odbicia pośrednie (czujnik → woda →
  otoczenie → woda → czujnik); zmiana sceny / oświetlenia przestawia proporcje — skok odczytu
  razem ze zmianą `ambient` i `signal`. Bez wody odczyt zaniżony.
- Styropian jako pływak-cel: odczyt zawyżony (~+30 mm wobec gołej wody, choć wierzch jest
  ~20 mm wyżej) — podczerwień wnika w piankę (rozpraszanie podpowierzchniowe).
- Kształt zbiornika: stożek 18° przy niskim poziomie zahacza o ścianki.
- Wniosek: czujnik ±2 cm — wystarczy jako informacja o rezerwie, nie do kontroli dawek ani
  stopu pompy. Kontrola dawek (dawna sekcja 7a, kroki 3b–c) usunięta z planu.

## 4. Ustawienia zbiornika (Settings)

FRAM `ReservoirConfig` (0x05A2): długość, szerokość, odległość czujnik → dno, poziom znacznika
(od dna — wysokość do liczenia pojemności). Wyliczane: ml/mm, pojemność, porcje użytkowe,
zakres rezerwy, ukryte dno. Walidacja z parametrami czujnika (zasięg 1200 mm, prześwit ≥ 50 mm,
ostrzeżenie o stożku szerszym niż zbiornik).

Usunięte z GUI/API (pola w FRAM zostają dla układu): tolerancja przy znaczniku, przerwa między
porcjami (`portion_pause_s` — nieużywane, dolewka ręczna ma stałą przerwę 5 s), przełącznik „czujnik
poziomu” (bajt `flags`).

## 5. Kalibracja pompy porcjami (wdrożone 2026-10-17, v5)

Porcja = `Single Dose ml`, zmieniana tylko w kalibracji (pierwszy krok). Wejście = Service Mode
bez automatycznego powrotu do Auto.

1. Start kalibracji → po 5 s pompa rusza sama (przebieg 1); użytkownik zatrzymuje na znaczniku
   naczynia, firmware mierzy czas w ms.
2. 20 s przerwy (cofnięcie wody w wężyku, opróżnienie naczynia — nie do zbiornika), potem
   przycisk „Start run” dla przebiegów 2, 3, 4.
3. Od razu po 4. przebiegu wynik: średnia, min/max, sd; podpowiedź „powtórz”, gdy max − min
   > 20% średniej. Save → czas porcji = średnia (FRAM), `volume_per_second` = porcja / czas;
   Reject → bez zmian.
- Bez pomiaru poziomu w trakcie kalibracji (`sensor_ml` w FRAM zawsze „brak”).
- Średnia z 4 (4 × porcja): błąd zatrzymania rozkłada się na większą objętość — nie jest to
  pomiar super dokładny, ale lepszy niż pojedynczy. Nie uwzględnia zmiany wydajności pompy
  przy pełnym vs prawie pustym zbiorniku (słup wody).
- Warunek startu: pływak nie zgłasza braku wody; jeśli czujnik ma odczyt — szacunek ≥ 4 + 1 porcji.
- Przebieg > 300 s albo zatrzymanie pompy spoza kalibracji → przerwanie procedury.
- Bez kalibracji dla bieżącej porcji algorytm liczy z ml/s (ostrzeżenie „NOT calibrated”).

## 6. Dolewka ręczna porcjami (wdrożone 2026-10-18)

- Przyciski System Control: Pump ON / OFF (zastępują pompę ciągłą 60 s i tryb monostabilny).
- Każde ON dodaje porcję do kolejki (max 20, `MANUAL_QUEUE_MAX`); pierwsze ON startuje od razu.
  Czas porcji = portion time z kalibracji (`getPortionMs()`).
- Między porcjami stała przerwa 5 s (`MANUAL_PORTION_PAUSE_MS`) — tylko zaakcentowanie
  pompowania porcjami; `portion_pause_s` z FRAM nieużywane.
- OFF dokańcza bieżącą porcję i zeruje kolejkę; OFF w przerwie kończy od razu. Emergency stop
  (`/api/pump/stop`) przerywa porcję natychmiast.
- Blokady startu bez zmian: pusta rezerwa (`isReserveEmpty`) i trwająca kalibracja. Pływak jak
  dotąd: nowa porcja z kolejki nie startuje, rozpoczęta się kończy.
- Porcje ręczne omijają algorytm, Service Mode i limit dobowy; nie liczą się do „Added in 24h”
  i nie trafiają do historii cykli (zaburzałyby EMA).
- W trakcie sesji (porcje + przerwy) cykl ATO jest odkładany, kalibracja nie startuje.
- GUI: „Portion 2/5” / „Pause 2/5”; `/api/status`: `direct_pump_mode`, `manual_left`
  (z bieżącą), `manual_done`, `manual_pause`.

## 7. Monitor poziomu (ATO Process Monitor) — wdrożone v5

- Prosty pasek postępu: 0% = ukryte dno, 100% = znacznik; kreska = górna granica zakresu
  rezerwy; kolor: niebieski / żółty (rezerwa) / czerwony (nic powyżej ukrytego dna).
- Objętość = długość × szerokość × (czujnik → dno − odczyt); użytkowe ml = objętość − ukryte
  dno (10% pojemności do znacznika, w pełnych porcjach); zakres rezerwy = 30% użytkowej.
- Tekst: „≈ N / M portions”, ml, %, wysokość wody / znacznik, wiek pomiaru, „settling after
  pump”, stan pływaka (gdy nie „ok”). Brak ważnego odczytu → „no reading”.
- Aktualizacja jak dotąd: serie co 10 s, cisza 2 min po pompie. Bez alarmów i buzzera —
  w zakresie rezerwy żółty znaczek w statusie.

## 8. Pływak rezerwy — stop pompy (wdrożone v5, tymczasowo wyłączone)

- GPIO 20, debounce 3 s. LOW → brak wody: `BUZZER_ALARM`, czerwony znaczek, nowa porcja (auto
  i ręczna) nie startuje; rozpoczęta się kończy.
- Zatrzask: po podniesieniu pływaka alarm trwa do Cycle Reset (GUI) albo przycisku sprzętowego;
  reset przy wciąż opuszczonym pływaku nie kasuje alarmu.
- `/api/status`: `reserve_empty`, `reserve_float` (ok / low / latched / disabled).

## 9. Buzzer

Jeden wzorzec `BUZZER_ALARM` — 2 piknięcia 50 ms co 120 s: pływak (brak wody) i każdy błąd
ATO. Zakres rezerwy z czujnika — bez buzzera.

## 10. FRAM

`ReservoirConfig` 0x05A2–0x05C3 (z `PumpCalibration`). Licznik porcji (`ReserveState`,
0x05C4–0x05D5) usunięty — wolne 0x05C4–0x05FF oraz 0x0AC0–0x7FFF.

## 11. API

- `GET|POST /api/reserve-config` — wymiary i znacznik; wyliczane ml/mm, pojemność, progi.
- `POST /api/pump/calibration` — `start&dose_ml`, `run`, `stop`, `confirm`, `cancel`.
- `GET /api/status` — `res_*` (szacunek z czujnika), `reserve_low`, `reserve_empty`,
  `reserve_float`, `cal_*`.
- Usunięte: `POST /api/reserve/refill`.

## 12. Później (świadomie odłożone)

- Włączenie pływaka (patrz przypomnienia na górze).
- Wyświetlacz OLED (`docs/DISPLAY_OLED_091.md`) — dane z GUI (rezerwa, stan pływaka).
- Ewentualnie lepszy cel dla czujnika (nieprzezroczysty matowy pływak w kominie) — jeśli
  kiedyś czujnik miałby wrócić do kontroli dawek.
- Log zdarzeń w FRAM + eksport CSV.
