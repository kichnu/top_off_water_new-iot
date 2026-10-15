# Plan wdrożenia: VL53L4CD jako monitor poziomu wody w rezerwie

Wersja 4 (2026-10-15) — rezerwa = licznik porcji, czujnik kontroluje dawki (sekcja 7a).
Wersja 3 (2026-10-14) — po testach na prawdziwym zbiorniku i ustaleniu procedur
kalibracji. Wersje 1–2 (pływak jako checkpoint, Kalman jako krok główny) — w historii gita.
Testy fizyczne czujnika: `~/Dokumenty/My_apps/TOOLS/distance_sensor_pololu/WNIOSKI.md`.

## 1. Cel

Rezerwa wody w zbiorniku zasilającym pompę ATO liczona w porcjach (licznik), a czujnik
odległości do lustra wody kontroluje, czy spadek poziomu odpowiada wlanym porcjom
(sekcja 7a). Dolewka zawsze w stałych porcjach o skalibrowanym czasie pompy (sekcje 5–6);
czujnik weryfikuje też sprawność przy znaczniku (sekcja 4).

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
- przełącznik „czujnik poziomu” (kontrola dawek, sekcja 7a);
- kontrola dawek: liczba porcji w bloku N (1–10, domyślnie 3) i tolerancja spadku
  [±1–30 mm, domyślnie 5] — osobna od tolerancji przy znaczniku;
- wyliczane (tylko odczyt): ml/mm, pojemność do znacznika, liczba porcji, ukryte dno,
  zakres rezerwy (progi w porcjach — sekcja 12);
- przycisk pobrania logu kontroli dawek (CSV, sekcja 7a).

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

Kropki = porcje z licznika (nie z odczytu czujnika — bez częściowych kropek): kontener
o stałej szerokości, kropki o stałym rozmiarze napełniają go od dołu jak woda; zakres
rezerwy na stałe żółty; 0 kropek = brak wody. Obok: ml, %, stan czujnika (ok / fault /
wyłączony), wiek pomiaru, „uspokajanie po pompie”, wynik ostatniej kontroli dawek.

## 7a. Kontrola dawek czujnikiem (ustalenia 2026-10-15)

Rezerwa w obu trybach = licznik porcji od pojemności z wymiarów (pole `Reserve Volume ml`
i licznik ml do usunięcia). Każda porcja odejmuje jedną kropkę.

Czujnik włączony (checkbox zaznaczony) — dodatkowo kontrola w blokach rozłącznych:
1. Pomiar odniesienia: średnia z 3 serii (SEM ~0,5 mm), poza ciszą po pompie.
2. N porcji (Settings, 1–10, domyślnie 3 — kilka porcji, żeby spadek był lepiej mierzalny;
   3 × 150 ml ≈ 7,9 mm).
3. Po 2 min ciszy pomiar „po” (3 serie); porównanie spadku z N × porcja / ml/mm.
4. W tolerancji (Settings, ±1–30 mm, domyślnie 5) → OK, pomiar „po” = nowe odniesienie.
   Poza tolerancją albo brak ważnego pomiaru (awaria czujnika) → alarm (jeden, wspólny)
   + stop systemu jak przy braku wody, czeka na interwencję użytkownika.
- Liczą się porcje ATO i ręczne. Kalibracja pompy i ręczna pompa ciągła (`direct-on`)
  unieważniają bieżący blok: licznik odejmuje ml z czasu pracy, nowe odniesienie.
- Dolanie wody do rezerwy: osobny przycisk „Rezerwa uzupełniona” → licznik pełny + nowe
  odniesienie. Bez kliknięcia czujnik zobaczy wzrost → poza tolerancją → alarm (celowo).
- Wyjście z alarmu: reset z GUI (Cycle Reset) zdejmuje alarm i ustawia nowe odniesienie,
  licznik bez zmian.
- Przy problemach użytkownik zwiększa tolerancję albo wyłącza czujnik.
- Log: każde sprawdzenie (także OK — do doboru tolerancji) w ring buffer w FRAM, rekord
  16 B: czas, liczba porcji, spadek oczekiwany i zmierzony (mm × 10), rozrzut (SEM),
  wynik (OK / poza tolerancją / brak pomiaru), stan licznika. 500 rekordów (8 KB od
  0x0AC0) — kilka miesięcy. Pobieranie jako CSV przyciskiem w Settings.
- Później (gdy czujnik przy rozsądnej tolerancji nie sypie alarmami): narzędzia analizy
  logu, potem dynamiczna korekta czasu pompy (zmiana wydajności, np. słup wody) — tylko
  przy pewności, że czujnik mierzy prawidłowo.

Czujnik wyłączony — samo liczenie porcji, bez kontroli; żółty zakres rezerwy to tylko
informacja (GUI, display); stop + alarm przy 0 kropek (ukryte dno 5%).

## 8. Buzzer

Jeden rodzaj alarmu — sygnał „sprawdź GUI” (później też display). Szczegóły w sekcji 12.
Sygnały provisioning i startowe bez zmian.

## 9. FRAM

MB85RC256V = 32 KB; zajęte do 0x0AC0 (~2,7 KB), wolne 0x0AC0–0x7FFF (~29 KB) oraz
0x05C4–0x05FF. Blok `ReservoirConfig` (magic 0xB3 + checksum) w 0x05A2: wymiary,
tolerancje, przerwa, flagi; w `_reserved`: N porcji i tolerancja kontroli dawek, czas
porcji (ms) + porcja, dla której skalibrowano, data sprawdzenia przy znaczniku.
Licznik porcji (stan rezerwy) — trwały w FRAM. Log kontroli dawek: 0x0AC0, 500 × 16 B
(sekcja 7a); reszta na późniejszy log zdarzeń.

## 10. API / GUI

Form-encoded POST (konwencja projektu):
- `GET|POST /api/reserve-config` — wymiary, tolerancje, N porcji kontroli, progi;
- `POST /api/reserve/refilled` — rezerwa uzupełniona (licznik pełny, nowe odniesienie);
- `GET /api/reserve/check-log.csv` — log kontroli dawek;
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
- Rezerwa = licznik porcji (sekcja 7a; bez pływaka). Progi na sztywno, w pełnych porcjach
  (`reservoirLevels()`): pojemność z wymiarów zaokrąglona w dół do pełnych porcji;
  5% z niej, zaokrąglone w górę do porcji = ukryte dno bezpieczeństwa (wysokość zasysu
  pompy) — dla użytkownika niewidoczne, jego 0 kropek = brak wody; z pozostałej (użytkowej)
  pojemności 30%, zaokrąglone w górę do porcji = zakres rezerwy (tylko informacja, żółty).
  Progi liczone przy każdym odczycie, nie zapisywane — idą za zmianą `Single Dose ml`.
- 0 kropek = brak wody (w obu trybach): alarm, nowa porcja nie startuje; rozpoczęta porcja
  zawsze się kończy (pompa ciągnie do ~2%).
- Kalibracja: GUI ostrzega, żeby nie wlewać wody z naczynia z powrotem do zbiornika do
  końca procedury. Zawsze 10 porcji, bez wpisywania zebranej objętości.
- Wlot pompy / otwór w przegrodzie: bez parametru (5% pokrywa wlot; otwór zostanie powiększony).
- Walidacja wymiarów w Settings uwzględnia parametry czujnika (zasięg, minimalny prześwit,
  stożek pola widzenia).
- Tolerancja przy znaczniku: Settings ±1–30 mm, domyślnie ±5 mm.
- Settings: przełącznik „czujnik poziomu” — wyłączony = samo liczenie porcji bez kontroli
  dawek; czujnik dalej mierzy (GUI), ale nie generuje alarmów; stop + alarm przy 0 kropek
  (jak z czujnikiem — zmiana 2026-10-15, wcześniej stop przy 30%).
- Porcje ręczne nie liczą się do limitu dobowego (nadal w ml), ale odejmują się z rezerwy.
- Kolejka porcji ręcznych: max 20 (kolejne kliknięcia ignorowane bez komunikatu); kolejka
  przerywana, gdy licznik dojdzie do 0 kropek albo kontrola dawek da alarm.
- Buzzer: jeden wzorzec `BUZZER_ALARM` — 2 piknięcia 50 ms z przerwą 100 ms, co 120 s
  (wdrożone). Zdarzenia: 0 kropek (brak wody), kontrola dawek poza tolerancją / brak
  pomiaru (tylko z czujnikiem), każdy błąd ATO (wdrożone). Alarm trwa do usunięcia
  przyczyny (albo wyłączenia czujnika) i resetu z GUI / display. Bez ciszy nocnej.
  Sygnały startowe bez zmian. Zakres rezerwy (żółty) — bez buzzera, tylko GUI / display.

## 13. Później (świadomie odłożone)

- Log zdarzeń w FRAM (za logiem kontroli dawek) + eksport CSV, rozróżnienie testy / praca.
- Narzędzia analizy logu kontroli dawek, potem korekta czasu pompy (sekcja 7a).
- Wykrywanie dolania wody do rezerwy (REFILLING) i spadku bez pompy (wyciek).
- Filtr (Kalman) pomiaru dawek, tryb cień.
- Wyświetlacz OLED (`docs/DISPLAY_OLED_091.md`) — tylko dane obecne w GUI.
- Usunięcie testu `LVLTEST` (`level_test.*`), gdy procedury z sekcji 4–5 go zastąpią.

## 14. Kolejność wdrożenia

1. ✅ Sterownik + serie + diagnostyka; ✅ cisza pomiarowa po pompie; ✅ test `LVLTEST`.
2. Settings zbiornika (sekcja 3) — FRAM, API, GUI.
3. Monitor poziomu (sekcja 7) + rezerwa z czujnika (licznik jako fallback) + progi.
   ✅ bargraf w ATO Process Monitor (kropki o stałym rozmiarze, kropka = porcja
   `Single Dose ml`, napełniane od dołu jak woda; pojemność użytkowa bez ukrytego dna,
   zakres rezerwy na żółto, źródło, wiek pomiaru, „settling”) —
   dane z `getReserveView()` (`reserve_controller`), pola `res_*` w `/api/status`.
   Następny krok (sekcja 7a, implementacja od 2026-10-16):
   a) licznik porcji z wymiarów w obu trybach (usunięcie `Reserve Volume ml` / licznika ml),
      widok kropek z licznika, stop + alarm przy 0 kropek, przycisk „Rezerwa uzupełniona”,
      nowa etykieta checkboxa;
   b) kontrola dawek w blokach N porcji + tolerancja (Settings), alarm + stop, reset;
   c) log kontroli w FRAM (ring buffer) + pobieranie CSV z Settings.
   Potem display i dopracowanie GUI; narzędzia analizy dopiero, gdy kontrola nie sypie
   alarmami.
4. Sprawdzenie przy znaczniku (sekcja 4).
5. Kalibracja porcjami (sekcja 5) + czas pompy w ms w algorytmie.
6. Dolewka ręczna porcjami (sekcja 6).
7. Buzzer — jeden alarm (sekcja 8).
