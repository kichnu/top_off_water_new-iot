# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-C3 automated water top-off (ATO) system for aquarium management. Built with PlatformIO and Arduino framework, targeting Seeed Xiao ESP32-C3. Features single float sensor detection, source-water reservoir monitoring (VL53L4CD level estimate + float switch for the empty stop), fixed-dose pump algorithm with EMA trend tracking, web dashboard, captive portal provisioning, and OTA (ArduinoOTA) firmware updates. Full upload commands (serial + OTA): see `README`.

## Build Commands

```bash
# Build and upload firmware (USB/serial)
pio run -e seeed_xiao_esp32c3 -t upload

# Monitor serial output (115200 baud)
pio device monitor

# Clean build
pio run -e seeed_xiao_esp32c3 -t clean

# Full rebuild
pio run -e seeed_xiao_esp32c3 -t clean && pio run -e seeed_xiao_esp32c3 -t upload
```

## Architecture

### Entry Point and Operation Modes

`src/main.cpp` implements two runtime modes determined at boot:

1. **Provisioning Mode** - Activated by holding reset button (GPIO 4) for 5+ seconds during boot. Starts WiFi AP with captive portal for initial credential setup.

2. **Production Mode** - Normal operation with water algorithm, web server.

### Core Modules (src/)

```
algorithm/          Water dosing algorithm
  algorithm_config.h   Algorithm constants, TopOffRecord, EmaBlock, TopOffConfig
  water_algorithm.*    Main WaterAlgorithm class (fixed-dose + EMA)

config/             Configuration and credential management
  config.*             Global settings, system enable/disable
  credentials_manager.*  Dynamic credential loading from FRAM
  reservoir_config.*     Reservoir geometry + pump calibration (FRAM)

core/               Logging utilities (Serial + read-only TCP log mirror, port 23)

crypto/             AES-256 encryption for FRAM credentials
  aes.*, sha256.*, fram_encryption.*

hardware/           Hardware abstraction layer
  buzzer_controller.*  Buzzer — FreeRTOS task (50ms tick), tryby: OFF/PROVISIONING/WARNING/ALARM
  fram_constants.h     Shared FRAM magic, version, field size constants
  fram_controller.*    FRAM memory management (all persistent data)
  hardware_pins.h      GPIO pin definitions
  level_sensor.*       VL53L4CD ToF reservoir level — non-blocking sample series
  level_event_log.*    Level sensor event log in FRAM (FAULT, resets, recovery) — diagnostics
  level_test.*         LVLTEST diagnostic level test (kept for diagnostics)
  pump_calibration.*   Guided portion calibration (4 runs, mean portion time in ms, FRAM in ReservoirConfig)
  pump_controller.*    ATO pump relay control (auto + direct/manual), run end cut by esp_timer (ms)
  reserve_controller.* Source-water reserve: level-sensor estimate (info) + reservoir float switch (stop)
  rtc_controller.*     DS3231 RTC with NTP sync
  water_sensors.*      Float sensor debouncing (ATO)

network/            Network connectivity
  wifi_manager.*       WiFi connection with dynamic credentials

provisioning/       Captive portal for initial setup
  ap_core.*            Access point management
  ap_server.*          Web server for provisioning
  ap_handlers.*        HTTP request handlers
  ap_html.*            Embedded HTML pages
  credentials_validator.*  Input validation for provisioning form
  prov_config.h        Provisioning constants (button pin, hold time)
  prov_detector.*      Boot button detection
  wifi_scanner.*       Network scanning

security/           Authentication and rate limiting
  auth_manager.*       Password verification
  session_manager.*    Session token management
  rate_limiter.*       Request throttling

web/                Production web interface
  web_server.*         ESPAsyncWebServer setup
  web_handlers.*       API endpoint handlers
  html_pages.*         Dashboard HTML (embedded ~60KB)
```

### Water Algorithm State Machine

Located in `algorithm/water_algorithm.cpp`, states defined in `algorithm_config.h`:

```
STATE_IDLE → STATE_DEBOUNCING → STATE_PUMPING → STATE_LOGGING → STATE_IDLE
Any state → (reservoir empty — float switch / daily limit / red alert) → STATE_ERROR
```

Direct/manual pump control (`/api/pump/direct-on`) bypasses this state machine
entirely — separate code path in `pump_controller`, not a state of
`WaterAlgorithm`.

Key parameters in `algorithm_config.h`:
- `DEBOUNCE_INTERVAL_MS` (1000ms) + `DEBOUNCE_COUNTER` (10) = 10s effective debounce
- `DEFAULT_DOSE_ML` (150ml) - fixed dose per cycle; changed only via the pump calibration, pump time = calibrated portion ms
- `DEFAULT_DAILY_LIMIT_ML` (2000ml) - daily safety limit
- No bootstrap gate — the EMA alarm score accumulates from the first cycle (see `README`)

### FRAM Memory Layout

Defined in `hardware/fram_controller.h`:
- `0x0000-0x0017`: Magic number and version
- `0x0018-0x0417`: Encrypted credentials (WiFi, admin password, device ID)
- `0x0500+`: Water system data (daily volume — see `README`)
- `0x0560-0x0589`: TopOffConfig (40B + 2B checksum), magic=0xA8
- `0x0590-0x05A1`: EmaBlock (16B + 2B checksum)
- `0x05A2-0x05C3`: ReservoirConfig incl. PumpCalibration (32B + 2B checksum), magic=0xB3
- `0x05C4-0x05FF`: free (0x05C4-0x05D5 was the reserve portion counter, removed)
- `0x0610-0x0AC0`: TopOff ring buffer (60 × 20B = 1200B)
- `0x1100-0x290F`: Level sensor event log (16B header + 48 × 128B), diagnostics

### Hardware Pin Configuration (Seeed Xiao ESP32-C3)

Defined in `hardware/hardware_pins.h`. Only 11 GPIOs available on this board
(2,3,4,5,6,7,8,9,10,20,21):
- GPIO 3:  `WATER_SENSOR_PIN` — ATO float sensor (INPUT_PULLUP, active LOW)
- GPIO 4:  `RESET_PIN` — provisioning button (INPUT_PULLUP, active LOW, hold 5s)
- GPIO 5:  `I2C_SDA_PIN` — I2C bus (DS3231 RTC + FRAM MB85RC256V + VL53L4CD ToF)
- GPIO 6:  `I2C_SCL_PIN` — I2C bus
- GPIO 7:  `BUZZER_PIN` — buzzer (**active HIGH**)
- GPIO 10: `ATO_PUMP_RELAY_PIN` — ATO pump relay (**active HIGH**: HIGH = ON, LOW = OFF)
- GPIO 8:  `RESERVOIR_FLOAT_PIN` — reservoir float switch (INPUT_PULLUP, LOW = no water);
  temporarily disabled (`RESERVE_FLOAT_ENABLED = 0` in `reserve_controller.h`) until installed
- GPIO 20: `LEVEL_XSHUT_PIN` — VL53L4CD XSHUT (open-drain only, active LOW; module pull-up to 2.8 V)
- Free: GPIO 2, 9, 21 (21 = UART0 TX, driven HIGH by the ROM boot log — no switch to GND)

> Note: output levels are defined as `PUMP_RELAY_ON/OFF` and `BUZZER_LEVEL_ON/OFF` in
> `hardware_pins.h`. Reserve: the VL53L4CD level is an estimate (~±2 cm, information only,
> no alarms); the pump stop on an empty reservoir is the float switch on GPIO 8.
> GPIO 9 is the BOOT strap pin (LOW during boot → download mode) — must stay unconnected.

### Key API Endpoints (Production Mode)

Authentication:
- `POST /api/login` — session auth (cookie)
- `POST /api/logout`
- `GET  /api/health` — no auth required

System:
- `GET  /api/status` — full system status JSON
- `GET|POST /api/system-toggle` — enable/disable system (Service Mode auto-re-enables after 15 min)
- `POST /api/system-reset` — reset ATO state machine

ATO pump:
- `POST /api/pump/direct-on` — manual dosing: +1 portion to the queue (max 20, 5 s pause between portions)
- `POST /api/pump/direct-off` — manual dosing: finish the running portion, clear the queue
- `POST /api/pump/stop` — emergency stop
- `GET  /api/pump-settings` — `volume_per_second` (derived from the calibration)
- `POST /api/pump/calibration` — guided portion calibration (`start&dose_ml`, `run`, `stop`, `confirm`, `cancel`); the only way to change dose_ml

History/volumes:
- `GET  /api/cycle-history` — ring buffer (60 records, newest-first)
- `POST /api/clear-cycle-history`
- `GET  /api/daily-volume` — rolling 24h stats
- `POST /api/reset-daily-volume`
- `GET  /api/fill-water-max` — daily_limit_ml and dose_ml
- `POST /api/set-fill-water-max` — set daily_limit_ml
- `GET|POST /api/alg-config` — alarm-score zones and initial EMA (EMA alpha/clamp, P1/P2 are constants)
- `GET|POST /api/reserve-config` — reservoir geometry (length, width, sensor → bottom, marker level)
- `GET|POST /api/level-events` — level sensor event log from FRAM (GET: plain text, POST: clear)
- `GET  /api/get-statistics` / `POST /api/reset-statistics`

Full parameter-level reference: see `README`.

### Dependencies (platformio.ini)

- ESPAsyncWebServer (mathieucarbou) ^3.3.23
- AsyncTCP (mathieucarbou) ^3.2.14
- ArduinoJson ^7.4.2
- RTClib ^2.1.1 (Adafruit)
- Adafruit FRAM I2C
- VL53L4CD ^1.0.0 (Pololu)

## Important Patterns

### Credential Loading

Credentials loaded at boot from AES-256 encrypted FRAM. Fallback hardcoded values exist if FRAM empty/corrupted. Access via macros in `config.h`:
- `WIFI_SSID_DYNAMIC`, `WIFI_PASSWORD_DYNAMIC`
- `ADMIN_PASSWORD_HASH_DYNAMIC`
- `DEVICE_ID_DYNAMIC`

### System Disable/Enable

Boots in Service Mode (`systemDisabled = true`), auto-switches to Auto Mode after 15 minutes (`SERVICE_MODE_AUTO_ENABLE_MS`). Functions in `config.h`:
- `setSystemState(bool enabled)`
- `isSystemDisabled()`
- `checkServiceAutoEnable()`

When system is disabled, WaterAlgorithm stops immediately (direct/manual pump control still works).

### POST Body Parameters

ESPAsyncWebServer `request->hasParam("name", true)` only parses `application/x-www-form-urlencoded`.
All POST handlers use form-encoded body — never JSON for POST requests.

### Midnight Auto-Restart

System automatically restarts at the next local midnight (`main.cpp`, computed once at boot from RTC time; guarded by a 30-minute minimum uptime and a valid RTC year). ATO pump safely stopped before restart.

### Timezone Handling

Configured for Poland (CET/CEST) with automatic DST. RTC stores UTC; all display/schedule times converted via `localtime_r()`. NTP servers use IP addresses for DNS independence.

### I2C Initialization Order

`Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)` must be called **before** `initFRAM()` / `initNVS()`.
Both provisioning and production paths in `main.cpp` call Wire.begin() first.
