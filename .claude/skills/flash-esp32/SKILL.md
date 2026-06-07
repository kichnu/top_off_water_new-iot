---
name: flash-esp32
description: Buduje i wgrywa firmware na Seeed XIAO ESP32-C6, opcjonalnie otwiera monitor
allowed-tools:
  - Bash(pio run *)
  - Bash(pio device monitor)
---

# Flash ESP32-C6

Zbuduj i wgraj firmware, następnie otwórz monitor szeregowy jeśli użytkownik tego chce.

## Środowisko
- Platforma: seeed_xiao_esp32c6
- Baud: 115200
- Projekt: /home/krzysztof/Dokumenty/My_apps/IOT/top_off_water_new-iot/

## Komendy

Build + upload:
```
pio run -e seeed_xiao_esp32c6 -t upload
```

Monitor:
```
pio device monitor
```

Pełny rebuild:
```
pio run -e seeed_xiao_esp32c6 -t clean && pio run -e seeed_xiao_esp32c6 -t upload
```

## Procedura

1. Uruchom build+upload i pokaż wynik.
2. Zapytaj czy otworzyć monitor szeregowy.
3. Jeśli tak — uruchom monitor. Aby wyjść: Ctrl+C.
