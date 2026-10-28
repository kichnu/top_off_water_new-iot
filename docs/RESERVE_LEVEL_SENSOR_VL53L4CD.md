# Plan wdrożenia: VL53L4CD jako monitor poziomu wody w rezerwie

Wersja 5 (2026-10-17) — czujnik tylko informacyjnie (szacunek ±2 cm), stop pompy przy pustym
zbiorniku = pływak (GPIO 8), kalibracja pompy 4 przebiegi / średnia, licznik porcji i procedura
„Rezerwa uzupełniona” z pomiarem usunięte.
Wersja 4 (2026-10-15) — licznik porcji + kontrola dawek czujnikiem; wersje 1–3 — w historii gita.
Testy fizyczne czujnika: `~/Dokumenty/My_apps/TOOLS/distance_sensor_pololu/WNIOSKI.md`.

## ⚠ Przypomnienia (do zrobienia)

- **Stop na poziomie alarmowym z ToF (2026-10-28, krok 3 planu `AUDYT_I_PLAN_REZERWY_TOF_1.0.md`)
  — nietestowany na sprzęcie.** Przetestować: zejście do poziomu alarmowego (alarm + buzzer,
  blokada porcji), reset poniżej alarm + 1 porcja (alarm zostaje), odłączony czujnik (po 5 min
  szacunek, po 30 min żółty status, „Reservoir refilled”), restart w alarmie (wraca z FRAM).
- **Test `LVLTEST` (`level_test.*`) zostaje** — narzędzie diagnostyczne czujnika; usunąć dopiero,
  gdy przestanie być potrzebny.

## 1. Cel

Informacja o ilości wody w zbiorniku zasilającym pompę ATO: czujnik odległości do lustra wody →
wysokość wody → objętość (pole dna × wysokość) → szacunkowa liczba porcji jako podpowiedź dla
użytkownika. Bez alarmów z czujnika. Zatrzymanie pompy przy pustym zbiorniku — dawniej pływak (usunięty),
w planie poziom alarmowy na ToF (`AUDYT_I_PLAN_REZERWY_TOF_1.0.md`).

## 2. Sprzęt i zbiornik

- VL53L4CD (Pololu #3692, `pololu/VL53L4CD`) na wspólnym I2C (SDA GPIO 5, SCL GPIO 6,
  100 kHz), adres 0x29, XSHUT na GPIO 20 (open-drain — reset sprzętowy przy starcie i w FAULT), INT
  niepodłączone. Bez osłony przed czujnikiem.
- Otwór w obudowie: cylindryczny Ø10 mm, krawędź zewnętrzna 5,5 mm od czujnika
  (Ø7 mm dawał 100% odrzuconych próbek).
- Zbiornik testowy: 282 × 202 mm, ścianki pionowe (56,96 ml/mm), czujnik 470 mm nad dnem,
  znacznik (maksymalny poziom) 380 mm od dna. Przegroda z małym otworem — 2 min ciszy
  pomiarowej po pompie (`LEVEL_PUMP_QUIET_MS`).
- Pływak rezerwy (GPIO 8) usunięty 2026-10-25 — GPIO 8 wolne.

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

Porcja = `Single Dose ml`, zmieniana tylko w kalibracji (pierwszy krok). Wejście (bez okna
potwierdzenia w GUI — ATO się zatrzymuje) = Service Mode
bez automatycznego powrotu do Auto.

1. Start kalibracji → przebieg 1 uruchamia użytkownik („Start run”, bez automatycznego startu
   pompy — zmiana 2026-10-18); zatrzymuje na znaczniku
   naczynia, firmware mierzy czas w ms.
2. 10 s przerwy (`CAL_PAUSE_MS`, było 20 s; cofnięcie wody w wężyku, opróżnienie naczynia — nie do zbiornika), potem
   przycisk „Start run” dla przebiegów 2, 3, 4.
3. Od razu po 4. przebiegu wynik: średnia, min/max, sd; podpowiedź „powtórz”, gdy max − min
   > 20% średniej. Save → czas porcji = średnia (FRAM), `volume_per_second` = porcja / czas;
   Reject → bez zmian.
- Bez pomiaru poziomu w trakcie kalibracji (`sensor_ml` w FRAM zawsze „brak”).
- Średnia z 4 (4 × porcja): błąd zatrzymania rozkłada się na większą objętość — nie jest to
  pomiar super dokładny, ale lepszy niż pojedynczy. Nie uwzględnia zmiany wydajności pompy
  przy pełnym vs prawie pustym zbiorniku (słup wody).
- Warunek startu: jeśli czujnik ma odczyt — szacunek ≥ 4 + 1 porcji.
- Przebieg > 300 s albo zatrzymanie pompy spoza kalibracji → przerwanie procedury.
- Bez kalibracji dla bieżącej porcji (nowy sprzęt albo kalibracja innej porcji) czas porcji =
  stałe 5 s (`DEFAULT_PORTION_MS`) — krótko i bezpiecznie; zapisane ml/s może nie istnieć
  (domyślne 1 ml/s dałoby 250 s dla 250 ml) albo być nieaktualne. GUI: żółte „Calibrate the
  pump”; po kalibracji zielone „Pump calibrated”, a linia „Single Dose … • portion time …” w
  zwykłym kolorze tekstu.

## 6. Dolewka ręczna porcjami (wdrożone 2026-10-18)

- Przyciski System Control: Pump ON / OFF (zastępują pompę ciągłą 60 s i tryb monostabilny).
- Każde ON dodaje porcję do kolejki (max 20, `MANUAL_QUEUE_MAX`); pierwsze ON startuje od razu.
  Czas porcji = portion time z kalibracji (`getPortionMs()`).
- Między porcjami stała przerwa 5 s (`MANUAL_PORTION_PAUSE_MS`) — tylko zaakcentowanie
  pompowania porcjami; `portion_pause_s` z FRAM nieużywane.
- OFF dokańcza bieżącą porcję i zeruje kolejkę; OFF w przerwie kończy od razu. Emergency stop
  (`/api/pump/stop`) przerywa porcję natychmiast.
- Blokada startu: trwająca kalibracja; każda porcja (pierwsza i kolejne z kolejki) startuje tylko,
  gdy mieści się ponad poziomem alarmowym (`reserveCanDose()`), inaczej reszta kolejki odpada.
- Porcje ręczne omijają algorytm, Service Mode i limit dobowy; nie liczą się do „Added in 24h”
  i nie trafiają do historii cykli (zaburzałyby EMA).
- W trakcie sesji (porcje + przerwy) cykl ATO jest odkładany, kalibracja nie startuje.
- GUI: „Portion 2/5” / „Pause 2/5”; `/api/status`: `direct_pump_mode`, `manual_left`
  (z bieżącą), `manual_done`, `manual_pause`.

## 7. Monitor poziomu (ATO Process Monitor) — wdrożone v5

- Pasek: 0% = dno, 100% = znacznik; zakres rezerwy żółty, zakres alarmowy czerwony (słabe tło
  na całej szerokości, pełny kolor tam, gdzie jest woda).
- Objętość = długość × szerokość × (czujnik → dno − odczyt). Progi (2026-10-28, krok 2 planu
  `AUDYT_I_PLAN_REZERWY_TOF_1.0.md`; ukryte dno 10% usunięte — zastąpione poziomem alarmowym):
  rezerwa `reserve_pct` (domyślnie 30%) i poziom alarmowy `alarm_pct` (domyślnie 15%) — % pojemności
  do znacznika, w górę do pełnych porcji; rezerwa ≥ alarm + 2 porcje. W FRAM w dawnym
  `_reserved` (0 = domyślne). Ostrzeżenie w Settings, gdy alarm < 30 mm wody (skok czujnika).
- Strefy `res_zone`: 0 OK, 1 REZERWA, 2 ALARM, 3 NIEZNANY. Porcje „Left” = ponad poziomem
  alarmowym (tyle da się jeszcze dolać); kalibracja wymaga ≥ 4 + 1 takich porcji.
- Tekst: „≈ N / M portions”, ml, %, wysokość wody / znacznik, wiek pomiaru, „settling after
  pump”. Brak ważnego odczytu → „no reading”.
- Aktualizacja jak dotąd: serie co 10 s, cisza 2 min po pompie. Buzzer tylko przy błędach
  krytycznych (decyzja 2026-10-28): rezerwa — żółty status; poziom alarmowy — alarm krytyczny.

## 7a. Stop na poziomie alarmowym i szacunek bez odczytu (2026-10-28)

`reserve_controller`. Decyzje z rozmowy 2026-10-28 (propozycja użytkownika, doprecyzowana):

- Poziom do decyzji (tryb pomiaru): minimum z 3 ostatnich ważnych serii (poza ciszą po pompie)
  minus woda pobrana od tego odczytu. Pobrana woda: każde zakończenie pracy pompy — auto i ręczne
  wg czasu (`czas × dawka / czas porcji`), kalibracja 1 porcja na przebieg.
- Alarm krytyczny (zatrzask, buzzer `BUZZER_ALARM`, `ERROR_LOW_RESERVOIR` przy próbie cyklu):
  poziom ≤ alarm; dawka auto, która by się nie zmieściła; brak jakiegokolwiek odczytu 60 s po
  starcie (pierwsze uruchomienie / niezapisany zbiornik). Porcja ręczna i przebieg kalibracji,
  które się nie mieszczą — odmowa bez zatrzasku.
- Kasowanie: Cycle Reset / przycisk tylko w trybie pomiaru i przy poziomie ≥ alarm + 1 porcja;
  bez samoistnego kasowania.
- Szacunek (tryb bez odczytu): po 5 min bez ważnej serii poza ciszą po pompie oraz po każdym
  starcie aż do 3 ważnych serii z rzędu. `zapas = (ostatni_odczyt − alarm) / 2 − pobrane od
  odczytu` — połowa zapasu nad alarmem = każda porcja liczona podwójnie (zapas na błąd kalibracji
  i odczytu). Zapas ≤ 0 → alarm krytyczny. Po 30 min bez odczytu żółty status (bez buzzera).
  Szacunek liczy faktyczne dawki, nie EMA; EMA tylko do informacji „≈ N h / dni do alarmu”.
- „Reservoir refilled” (GUI, z potwierdzeniem; `POST /api/reserve/refill`) — tylko bez odczytu:
  szacunek od pełnego zbiornika (do znacznika, dalej połowa zapasu), kasuje alarm rezerwy i
  `ERROR_LOW_RESERVOIR`. W trybie pomiaru 409 (poziom i tak mierzony).
- FRAM `ReserveTrack` (0x05C4): ostatni odczyt (ml, czas, źródło: czujnik / refill), pobrane od
  niego, przyczyna zatrzasku — restart (także nocny) kontynuuje od nich, zatrzask przeżywa
  restart.
- GUI: pasek kreskowany w szacunku („Left: ≥ … (estimate)”), notatki: przyczyna alarmu, czas
  bez odczytu, czas do alarmu z EMA. `/api/status`: `reserve_empty` (zatrzask), `reserve_alarm`
  (kod 0–4), `reserve_alarm_text`, `reserve_no_reading`, `res_estimated`, `res_no_reading_s`,
  `res_avail_ml`, `res_hours_left`.

## 8. Pływak rezerwy — usunięty (2026-10-25)

Wdrożony w v5, nigdy nie zamontowany (`RESERVE_FLOAT_ENABLED = 0`), usunięty z kodu. Pole
`reserve_float` usunięte z API. Następca: poziom alarmowy na ToF (§7a).

## 9. Buzzer

Jeden wzorzec `BUZZER_ALARM` — 2 piknięcia 50 ms co 120 s: każdy błąd ATO i alarm rezerwy
(poziom alarmowy / szacunek / brak odczytu). Zakres rezerwy i brak odczytu 30 min — bez buzzera.

## 10. FRAM

`ReservoirConfig` 0x05A2–0x05C3 (z `PumpCalibration`). `ReserveTrack` 0x05C4–0x05D5 (16 B + 2 B
sumy, magic 0xC5; dawniej w tym miejscu licznik porcji `ReserveState`). Wolne 0x05D6–0x05FF oraz
0x0AC0–0x7FFF.

## 11. API

- `GET|POST /api/reserve-config` — wymiary i znacznik; wyliczane ml/mm, pojemność, progi.
- `POST /api/pump/calibration` — `start&dose_ml`, `run`, `stop`, `confirm`, `cancel`.
- `GET /api/status` — `res_*` (szacunek z czujnika; `res_reserve_ml`, `res_alarm_ml`, `res_zone`),
  `reserve_low` (strefa REZERWA lub ALARM), `reserve_empty` (zatrzask alarmu), pola z §7a, `cal_*`.
- `POST /api/reserve/refill` — „zbiornik uzupełniony” bez odczytu (§7a).

## 12. Później (świadomie odłożone)

- Wyświetlacz OLED (`docs/DISPLAY_OLED_091.md`) — dane z GUI (rezerwa).
- Ewentualnie lepszy cel dla czujnika (nieprzezroczysty matowy pływak w kominie) — jeśli
  kiedyś czujnik miałby wrócić do kontroli dawek.
- Log zdarzeń w FRAM + eksport CSV.
