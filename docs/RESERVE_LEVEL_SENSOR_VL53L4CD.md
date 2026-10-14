# Plan wdrożenia: VL53L4CD jako monitor poziomu wody w rezerwie

Wersja 3 (2026-10-14) — po testach na prawdziwym zbiorniku i ustaleniu procedur
kalibracji. Wersje 1–2 (pływak jako checkpoint, Kalman jako krok główny) — w historii gita.
Testy fizyczne czujnika: `~/Dokumenty/My_apps/TOOLS/distance_sensor_pololu/WNIOSKI.md`.

## 1. Cel

Ciągły pomiar objętości wody w zbiorniku rezerwy (zasilającym pompę ATO) na podstawie
odległości z góry do lustra wody. Dolewka zawsze w stałych porcjach o skalibrowanym czasie
pompy (sekcje 5–6); czujnik weryfikuje sprawność (znacznik, sekcja 4) i pokazuje rezerwę.

## 2. Sprzęt i zbiornik (stan z testów 2026-10-14)

- VL53L4CD (Pololu #3692, `pololu/VL53L4CD`) na wspólnym I2C (SDA GPIO 5, SCL GPIO 6,
  100 kHz), adres 0x29, XSHUT/INT niepodłączone. Pływak rezerwy usunięty (`9bb6758`).
- Otwór w obudowie: cylindryczny Ø10 mm, krawędź zewnętrzna 5,5 mm od czujnika.
  Ø7 mm dawał 100% odrzuconych próbek — ścianka w stożkach Tx 29° / Rx 25°
  (datasheet rys. 21; Rx = mniejszy otwór Ø0,20, Tx = większy Ø0,40 przy pinie 1).
- Zbiornik testowy: 282 × 202 mm, ścianki pionowe (56,96 ml/mm), czujnik 470 mm nad dnem,
  maksymalny poziom (znacznik) 380 mm od dna → odległość przy pełnym 90 mm.
- **Przegroda z małym otworem** — poziom po stronie czujnika wyrównuje się powoli:
  po pracy pompy 2 min ciszy pomiarowej (`LEVEL_PUMP_QUIET_MS`, wdrożone).
- Szum: seria 10 próbek sd ~2–3 mm, SEM ~0,7–1 mm; kolejne serie przy stojącej wodzie
  ±1 mm. Pojedyncza seria → ±60 ml; do decyzji uśredniać kilka serii.

Wnioski z testów (`LVLTEST`, przebiegi 29–60 s): stosunek czujnik/kalibracja 0,75–0,94,
dopasowanie `ml ≈ 10,2·t + 182` — przesunięcie to ekstrapolacja poza zakres krótkich
przebiegów. Dlatego kalibracja **w punkcie pracy** (porcja = dawka), nie model ml/s
(sekcja 5) — start pompy i cofka z węża są wtedy w czasie porcji.

## 3. Ustawienia zbiornika (Settings) — KROK 1

Parametry w FRAM (nowy blok, sekcja 9), edycja w Settings:
- długość, szerokość [mm] (przekrój prostokątny, ścianki pionowe);
- odległość czujnik → dno [mm];
- poziom znacznika = maksymalny poziom od dna [mm];
- tolerancja sprawdzenia przy znaczniku [±1–30 mm, domyślnie 5];
- przerwa między porcjami [15–120 s, domyślnie 30];
- wyłącznik alarmów czujnika poziomu;
- wyliczane (tylko odczyt): ml/mm, pojemność do znacznika, liczba porcji, progi
  (na sztywno: niska < 30%, brak < 5%).

Walidacja (parametry czujnika): odległość czujnik → dno ≤ 1200 mm (zasięg VL53L4CD),
prześwit czujnik → lustro przy znaczniku ≥ 50 mm; ostrzeżenie (bez blokady), gdy
plama pola widzenia 18° na dnie jest szersza niż krótszy bok zbiornika.

## 4. Sprawdzenie przy znaczniku

1. Użytkownik dolewa wodę do znacznika i klika „Woda na znaczniku”.
2. System czeka na ustabilizowanie (przegroda) i robi długi pomiar: kilkanaście serii,
   do ~2 min; pokazuje postęp.
3. Wynik: zmierzona odległość vs oczekiwana (czujnik → dno − znacznik), odchyłka w mm i ml,
   rozrzut serii; ocena względem tolerancji z Settings.
4. Użytkownik zatwierdza → rezerwa = pełna (reset), zapis daty sprawdzenia
   (i ewentualnie offsetu — do ustalenia).

## 5. Kalibracja pompy porcjami (GUI: sekcja Pump Calibration, prowadzenie krok po kroku)

Wielkość porcji = `Single Dose ml` (jedna wartość dla algorytmu ATO, dolewki ręcznej
i kalibracji).

1. Warunki: wymiary wprowadzone, czujnik sprawny, system w Service Mode, zapas wody
   na N porcji + margines.
2. Pomiar poziomu „przed” (kilka serii).
3. N przebiegów (domyślnie 10): Start → pompa → użytkownik zatrzymuje, gdy naczynie
   osiągnie porcję. Czas mierzy firmware (ms). Przerwa między przebiegami — sekcja 12.
4. Pomiar poziomu „po” (2 min ciszy + kilka serii) → całkowity ubytek wg czujnika
   (przy 10 × 250 ml ~44 mm — bardzo dokładnie).
5. Wynik: mediana i rozrzut czasów, czas porcji, porównanie N × porcja vs czujnik.
   Zatwierdź → zapis czasu porcji (ms) do FRAM.

Zmiana `Single Dose ml` po kalibracji → kalibracja nieważna (ostrzeżenie w GUI).

## 6. Dolewka ręczna porcjami (System Control)

Zamiast przełącznika ON/OFF:
- **ON** (monostabilny): każde kliknięcie dodaje jedną porcję do kolejki; pierwsze
  uruchamia pompę. Etykieta: porcja bieżąca / łącznie w kolejce.
- **OFF**: dokończ bieżącą porcję i wyzeruj kolejkę.
- Porcja = skalibrowany czas pompy; między porcjami przerwa (sekcja 12).
- Rezerwa: odjęcie porcji po każdej porcji.

## 7. Monitor poziomu (ATO Process Monitor)

Graficzny zbiornik z poziomem: kolor wg stref (OK / niska / pusta), wartości ml, %, mm,
stan czujnika (ok / fault / brak), wiek pomiaru, informacja „uspokajanie po pompie”.

## 8. Buzzer

Jeden rodzaj alarmu — sygnał „sprawdź GUI” (później też display). Szczegóły w sekcji 12.
Sygnały provisioning i startowe bez zmian.

## 9. FRAM

MB85RC256V = 32 KB; zajęte do 0x0AC0 (~2,7 KB), wolne 0x0AC0–0x7FFF (~29 KB) oraz
0x05A2–0x05FF (94 B). Nowy blok `ReserveLevelConfig` (osobny magic + checksum) w 0x05A2:
wymiary, tolerancja, progi, czas porcji (ms) + porcja, dla której skalibrowano, data
sprawdzenia przy znaczniku. Obszar 0x0AC0+ zostaje na późniejszy log zdarzeń.

## 10. API / GUI

Form-encoded POST (konwencja projektu):
- `GET|POST /api/reserve-config` — wymiary, tolerancja, progi;
- `POST /api/reserve/marker-check` (start), `GET` (postęp/wynik), `POST .../accept`;
- `POST /api/pump/calibration/...` — start / run-start / run-stop / finish / accept / abort;
- `POST /api/pump/portion-add`, `POST /api/pump/portion-stop`;
- `GET /api/status` — dodatkowo poziom w ml/%, strefa, kolejka porcji, stan procedur.

## 11. Harmonogram pomiaru

- Nieblokujący: jedna próbka na iterację `loop()`, seria N=10, budżet 50 ms.
- Cyklicznie co 10 s (bring-up; docelowo do ustalenia), brak pomiarów w trakcie pompy
  i 2 min po niej; serie na żądanie (procedury) omijają ciszę.

## 12. Ustalenia (2026-10-14) i otwarte punkty

Ustalone:
- Przerwa między porcjami (kalibracja i dolewka ręczna): domyślnie 30 s, ustawiana w GUI
  15–120 s. Algorytm ATO ma naturalnie długie przerwy.
- Czas pompy z dokładnością 0,1 s (dziś pełne sekundy w `triggerPump` / `calculateDoseTime`).
- `Single Dose ml` = porcja wszędzie; panel ustawień przerabiany wg potrzeb.
- OFF dokańcza porcję — bez osobnego stopu awaryjnego (porcja < 250 ml nie przeleje
  akwarium, wężyk można zatkać).
- Rezerwa tylko z czujnika (bez pływaka). Progi na sztywno: niska < 30% pojemności,
  brak < 5%. GUI: bargraf podzielony na porcje, strefy kolorami. Pole `Reserve Volume ml`
  i licznik programowy — do przerobienia (pojemność z wymiarów).
- Poziom < 5% = brak wody: alarm, nowa porcja nie startuje; rozpoczęta porcja zawsze się
  kończy (pompa ciągnie do ~2%).
- Kalibracja: GUI ostrzega, żeby nie wlewać wody z naczynia z powrotem do zbiornika do
  końca procedury. Zawsze 10 porcji, bez wpisywania zebranej objętości.
- Wlot pompy / otwór w przegrodzie: bez parametru (5% pokrywa wlot; otwór zostanie powiększony).
- Walidacja wymiarów w Settings uwzględnia parametry czujnika (zasięg, minimalny prześwit,
  stożek pola widzenia).
- Tolerancja przy znaczniku: Settings ±1–30 mm, domyślnie ±5 mm.
- Settings: wyłącznik alarmów czujnika poziomu (GUI / display / buzzer) — pomiary i funkcje
  działają dalej, żeby nie zaburzać algorytmu.
- Porcje ręczne nie liczą się do limitu dobowego (nadal w ml), ale odejmują się z rezerwy.

Otwarte:
- Wyłączone alarmy czujnika: czy blokada pompy przy „brak” (< 5%) też jest wyłączona?
- Buzzer: zdarzenia, wzorzec, wyciszanie (potwierdzenie w GUI?), godziny ciszy.
- Limit długości kolejki porcji ręcznych (np. do porcji dostępnych powyżej progu „brak”).

## 13. Później (świadomie odłożone)

- Log zdarzeń w FRAM (0x0AC0+, ~1000 rekordów) + eksport CSV, rozróżnienie testy / praca.
- Wykrywanie dolania wody do rezerwy (REFILLING) i spadku bez pompy (wyciek).
- Pomiar dawki wokół każdego cyklu ATO, filtr (Kalman) i korekta pompy, tryb cień.
- Wyświetlacz OLED (`docs/DISPLAY_OLED_091.md`) — tylko dane obecne w GUI.
- Usunięcie testu `LVLTEST` (`level_test.*`), gdy procedury z sekcji 4–5 go zastąpią.

## 14. Kolejność wdrożenia

1. ✅ Sterownik + serie + diagnostyka; ✅ cisza pomiarowa po pompie; ✅ test `LVLTEST`.
2. Settings zbiornika (sekcja 3) — FRAM, API, GUI.
3. Monitor poziomu (sekcja 7) + rezerwa z czujnika (licznik jako fallback) + progi.
4. Sprawdzenie przy znaczniku (sekcja 4).
5. Kalibracja porcjami (sekcja 5) + czas pompy w ms w algorytmie.
6. Dolewka ręczna porcjami (sekcja 6).
7. Buzzer — jeden alarm (sekcja 8).
