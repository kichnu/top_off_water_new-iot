# OTA (ArduinoOTA) na Seeed XIAO ESP32-C3 — analiza wykonalności

Data analizy: 2026-08-24. Kontekst: `[env:seeed_xiao_esp32c3]` w `platformio.ini`.
Ogólny, przenośny wzorzec wdrożenia (kod, gotchas) — patrz
`docs/OTA_WIFI_UPLOAD_PATTERN.md`. Ten dokument to konkretne ustalenia dla
tego projektu (Dolewka/ATO).

## Wniosek

Implementacja jest możliwa. Jeden blocker do usunięcia przed pierwszym
uploadem OTA (partycje), reszta to standardowe kroki z wzorca.

## 1. Blocker: tabela partycji

`board_build.partitions = huge_app.csv` — **tylko jeden slot aplikacji**
(`app0`, self-overwrite, 3MB). OTA z tą tabelą kończy się `abort()` w
`esp_flash_erase_region` (identyczny bug jak w `thermo_control`, patrz
wzorzec sekcja 3). Trzeba zmienić na tabelę z dwoma slotami OTA.

**Potwierdzony realny flash: 4MB** (board JSON `seeed_xiao_esp32c3.json`,
`maximum_size: 4194304`).

Zbudowany obecny firmware: **1 253 518 B** (39.8% z 3MB w `huge_app.csv`).

Porównanie tabel z dwoma slotami dla 4MB:

| Tabela | Slot app0/app1 | Zapełnienie obecnym firmware |
|---|---|---|
| `default.csv` | 2× 1.25MB (0x140000) | 95.6% — za ciasno, brak zapasu |
| `min_spiffs.csv` | 2× 1.875MB (0x1E0000), spiffs 128KB | 66.8% — bezpieczny zapas |

Projekt nie używa SPIFFS/LittleFS (trwałe dane w zewnętrznym FRAM przez
I2C, HTML dashboardu wbudowany w firmware jako string) — pusty spiffs
128KB w `min_spiffs.csv` nie przeszkadza.

**Rekomendacja:** `board_build.partitions = min_spiffs.csv`.

## 2. Bezpieczeństwo aktuatora (pompa ATO)

Zgodnie z wzorcem — `ArduinoOTA.onStart()` musi wyłączyć pompę PRZED erase
flash (erase blokuje CPU dłużej niż jeden tick pompy):

```cpp
ArduinoOTA.onStart([]() {
    stopPump();           // pump_controller.h
    setSystemState(false); // config.h — blokuje auto-cykl algorytmu
});
ArduinoOTA.onError([](ota_error_t error) {
    setSystemState(true);  // OTA może się nie udać bez restartu urządzenia
});
```

## 3. Pętla główna (`main.cpp` `loop()`)

Obecny `loop()` kończy się bezwarunkowym `delay(100)` (main.cpp:269).
`ArduinoOTA.handle()` musi być wołane co iterację, przed tym delayem —
zadziała, ale throughput OTA będzie wolniejszy niż bez delaya (akceptowalne,
transfer ~1.2MB firmware potrwa dłużej niż typowe kilka-kilkanaście sekund).

## 4. Hasło OTA

Brak dotąd mechanizmu w projekcie. Do zastosowania wzorzec z
`thermo_control`: `OTA_PASSWORD` jako `sysenv` w `build_flags` nowego
środowiska `[env:seeed_xiao_esp32c3_ota]` + `ArduinoOTA.setPassword()` na
urządzeniu. Uwaga na gotchas z wzorca (sekcja 4): export w tej samej
komendzie co `pio run`, cudzysłów dla znaków specjalnych.

## 5. Pozostałe kroki

- Pierwszy upload firmware z kodem OTA musi iść przez USB (OTA nie istnieje,
  dopóki nie jest już na urządzeniu)
- Jeśli urządzenie jest w osobnej podsieci/VLAN (izolacja IoT) — może być
  potrzebna reguła firewalla na `--host_port` (espota łączy się z powrotem
  do komputera)
- Diagnoza ewentualnych crashy podczas OTA zawsze wymaga fizycznego USB
  monitora równolegle (log-socket WiFi nie pokazuje przyczyny paniki)

## Niezwiązany drobiazg zauważony przy analizie

`main.cpp:12` — `#include "network/wifi_manager.h"!` — zbędny `!` na końcu
linii (tylko warning kompilatora, nie blokuje builda).
