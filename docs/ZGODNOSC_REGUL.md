# Zgodność z regułami globalnymi

Reguły: `~/.claude/CLAUDE.md` („Sekrety w kodzie”, „Krytyczne bloki sprzętowe”). Wzorce i stan wszystkich
projektów: `~/dev-knowledge/iot-security/secrets-in-firmware.md`, `~/dev-knowledge/esp32/critical-hardware-alarm.md`.
Punkt naprawiony → oznaczyć tu i w tabeli w dev-knowledge. Audyt 2026-10-29, kod niezmieniany.

## Sekrety — ✅

- `src/config/config.cpp`: placeholder `SETUP_REQUIRED`.

## Alarm krytyczny FRAM/RTC — ✅ w kodzie 2026-10-31 (`hw_health`), czeka na wgranie i test HW

Stan przed zmianą:

- Przy starcie tylko jednorazowe `buzzerCritical()` (`src/main.cpp` ~226: FRAM, credentials, Wi-Fi, RTC);
  alarm ATO (`BUZZER_ALARM`) obejmuje błędy algorytmu, nie awarię FRAM/RTC.
- `/api/status` ma `rtc_working`, brak stanu FRAM; brak wykrywania w trakcie pracy.
- Wymagane: zatrzaśnięty alarm z buzzerem, widoczny w GUI i na dashboardzie VPS, wykrywanie także w trakcie pracy.
