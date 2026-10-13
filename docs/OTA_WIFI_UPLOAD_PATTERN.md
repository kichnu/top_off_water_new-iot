# Wzorzec: upload firmware przez WiFi (ArduinoOTA)

Krótki, przenośny wyciąg z wdrożenia w `thermo_control`. Cel: nie błądzić od
zera przy powtórce w **Dolewka**/**Dozownik** (ESP32, actuator-bearing —
pompa/dozownik, więc `onStart()`-emergency-off jest tu jeszcze ważniejszy niż
w termostacie). Pełna historia i pełne uzasadnienia decyzji:
`docs/DEV_UPLOAD_LOGGING_CLI_ARCHITECTURE.md` (sekcja 1) i pamięć projektu
(`ota_erase_abort_root_cause_2026-07-24`, `ota_upload_crash_investigation_paused_2026-07-23`).

## 1. platformio.ini

```ini
[env:xxx]
default_envs = xxx          ; BEZ tego `pio run` bez -e budowałoby WSZYSTKIE
                             ; environmenty, łącznie z doomed OTA-uploadem

[env:xxx_ota]
extends = env:xxx
upload_protocol = espota
upload_flags =
    --auth=${sysenv.OTA_PASSWORD}
    --host_port=8266          ; STAŁY port zwrotny — patrz gotcha #3 niżej

build_flags =
    ...
    -DOTA_PASSWORD=\"${sysenv.OTA_PASSWORD}\"   ; też w build_flags, nie tylko upload_flags —
                                                  ; to hasło, którego ArduinoOTA.setPassword()
                                                  ; użyje NA URZĄDZENIU
```

## 2. main.cpp

```cpp
if (isWiFiConnected()) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
        // KRYTYCZNE dla pompy/dozownika: wyłączyć aktuator PRZED erase flash.
        // Erase blokuje CPU na dłużej niż jeden tick — aktuator zostawiony ON
        // zostanie ON przez cały czas trwania OTA (kilka-kilkanaście sekund).
        actuatorEmergencyOff();
        setSystemState(false);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        // OTA może się nie udać z przyczyn niezwiązanych z crashem (zły hash,
        // sieć) — wtedy urządzenie NIE restartuje się samo, więc trzeba
        // ręcznie przywrócić stan, który onStart() wyłączył.
        setSystemState(true);   // albo jawne wznowienie tego, co wyłączono
    });
    ArduinoOTA.begin();
}
// w loop(): ArduinoOTA.handle() — KAŻDĄ iterację, nie w wolniejszym ticku
```

**Pierwszy upload firmware zawierającego ten kod musi iść przez USB** — OTA nie
istnieje, dopóki firmware z `ArduinoOTA.begin()` nie jest już na urządzeniu.

## 3. Partycje — NAJWAŻNIEJSZY punkt, źródło realnego buga

OTA wymaga **dwóch prawdziwych slotów aplikacji** (`app0`+`app1`, boot flip
między nimi). Sprawdź to bezpośrednio w pliku CSV
(`~/.platformio/packages/framework-arduinoespressif32/tools/partitions/*.csv`)
— **nie ufaj samej nazwie pliku**. `huge_app.csv` brzmi bezpiecznie, a ma
tylko JEDEN slot (`app0`, self-overwrite) — w `thermo_control` to spowodowało
twardy `abort()` w `esp_flash_erase_region` przy każdej próbie OTA (host:
timeout/connection reset na 0%; serial: `abort() was called at PC ...` w
środku `esp_flash_erase_region`).

Checklist:
1. Sprawdź realny rozmiar flash z bootowego logu (`Flash: <bytes>` w
   `ESP.getFlashChipSize()`/log startowy) — **nie ufaj domyślnemu profilowi
   płytki w PlatformIO**. Board id (np. `seeed_xiao_esp32s3`) opisuje
   chip/toolchain, nie faktyczny sprzęt — ten sam wzorzec "board != realna
   płytka" jak w `thermo_control`/Dozowniku/ATO (Waveshare zamiast Seeed).
2. Wybierz gotową tabelę partycji dopasowaną do REALNEGO rozmiaru flash z
   dwoma slotami OTA (np. `app3M_fat9M_16MB.csv` dla 16MB, `default_8MB.csv`
   dla 8MB, `min_spiffs.csv` dla 4MB) — `board_build.partitions` w
   `platformio.ini`.
3. Zmiana partycji wymaga jednego reflashu po USB (nie przechodzi przez OTA).

## 4. Gotchas z sesji upload/debug

- **`OTA_PASSWORD` musi być w TEJ SAMEJ komendzie shell co `pio run`** — export
  w osobnym wywołaniu (np. osobny krok w agencie/skrypcie) nie przetrwa do
  następnego procesu. Objaw: `Authentication Failed` mimo poprawnego hasła —
  bo firmware zostało przeflashowane z pustym/innym hasłem w build_flags.
- **Hasło ze znakami specjalnymi shella (`#`, `$`, spacja) — zawsze w
  pojedynczym cudzysłowie**: `export OTA_PASSWORD='we6#...'`.
- **Różne podsieci/VLAN (izolacja IoT na routerze)**: espota po autentykacji
  wymaga połączenia TCP zainicjowanego PRZEZ urządzenie z powrotem do kompa
  (na `--host_port`) — typowa reguła firewalla "IoT może być odpytywane, nie
  może samo wchodzić do LAN" to blokuje. Potrzebna tymczasowa reguła forward
  (src=urządzenie, dst=komp, dst-port=host_port).
- **Log-socket (WiFi TCP log) NIE pokaże przyczyny crasha/paniki** — pisze się
  surowo na UART z pominięciem softwarowego loggera. Diagnoza crasha zawsze
  wymaga fizycznego USB monitora równolegle z próbą OTA.
- **Dekodowanie backtrace'u precyzyjnie, nie na oko**: `xtensa-esp32sX-elf-addr2line
  -pfiaC -e .pio/build/<env>/firmware.elf <adresy z Backtrace:>` — ELF musi być
  z TEGO SAMEGO builda co crash (nie przebudowany później). Dodatkowo
  `objdump -d` wokół adresu abort(), jeśli trzeba zobaczyć co dokładnie
  sprawdza kod przed wywołaniem `abort()`.
