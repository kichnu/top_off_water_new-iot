# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-C6 automated water top-off (ATO) system for aquarium management. Built with PlatformIO and Arduino framework, targeting Seeed Xiao ESP32-C6. Features single float sensor detection, second sensor for available-water reservoir, fixed-dose pump algorithm with EMA trend tracking, web dashboard, and captive portal provisioning.

## Build Commands

```bash
# Build and upload firmware
pio run -e seeed_xiao_esp32c6 -t upload

# Monitor serial output (115200 baud)
pio device monitor

# Clean build
pio run -e seeed_xiao_esp32c6 -t clean

# Full rebuild
pio run -e seeed_xiao_esp32c6 -t clean && pio run -e seeed_xiao_esp32c6 -t upload
```

## Architecture

### Entry Point and Operation Modes

`src/main.cpp` implements two runtime modes determined at boot:

1. **Provisioning Mode** - Activated by holding reset button (GPIO 2) for 5+ seconds during boot. Starts WiFi AP with captive portal for initial credential setup.

2. **Production Mode** - Normal operation with water algorithm, web server.

### Core Modules (src/)

```
algorithm/          Water dosing algorithm
  algorithm_config.h   Algorithm constants, TopOffRecord, EmaBlock, TopOffConfig
  water_algorithm.*    Main WaterAlgorithm class (fixed-dose + EMA)

config/             Configuration and credential management
  config.*             Global settings, system enable/disable
  credentials_manager.*  Dynamic credential loading from FRAM

core/               Logging utilities

crypto/             AES-256 encryption for FRAM credentials
  aes.*, sha256.*, fram_encryption.*

hardware/           Hardware abstraction layer
  fram_constants.h     Shared FRAM magic, version, field size constants
  fram_controller.*    FRAM memory management (all persistent data)
  hardware_pins.h      GPIO pin definitions
  pump_controller.*    ATO pump relay control
  rtc_controller.*     DS3231 RTC with NTP sync
  status_led.*         Status LED control
  water_sensors.*      Float sensor debouncing (ATO + available-water)

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
STATE_PUMPING → (daily limit / red alert) → STATE_ERROR
STATE_* → (direct pump on) → STATE_MANUAL_OVERRIDE
```

Key parameters in `algorithm_config.h`:
- `DEBOUNCE_INTERVAL_MS` (200ms) + `DEBOUNCE_COUNTER` (5) = 1s effective debounce
- `DEFAULT_DOSE_ML` (150ml) - fixed dose per cycle
- `DEFAULT_DAILY_LIMIT_ML` (2000ml) - daily safety limit
- `DEFAULT_MIN_BOOTSTRAP` (3) - cycles before EMA alerts activate

### FRAM Memory Layout

Defined in `hardware/fram_controller.h`:
- `0x0000-0x0017`: Magic number and version
- `0x0018-0x0417`: Encrypted credentials (WiFi, admin password, device ID)
- `0x0500+`: Water system data
- `0x0560-0x0575`: TopOffConfig (20B + 2B checksum), magic=0xA7
- `0x0580-0x0591`: EmaBlock (16B + 2B checksum)
- `0x0610-0x0AC0`: TopOff ring buffer (60 × 20B = 1200B)

### Hardware Pin Configuration (Seeed Xiao ESP32-C6)

Defined in `hardware/hardware_pins.h`:
- GPIO 0:  `WATER_SENSOR_PIN` — ATO float sensor (INPUT_PULLUP, active LOW)
- GPIO 1:  `AVAILABLE_WATER_SENSOR_PIN` — reservoir level sensor (INPUT_PULLUP, active LOW)
- GPIO 2:  `RESET_PIN` — provisioning button (INPUT_PULLUP, active LOW, hold 5s)
- GPIO 18: `BUZZER_PIN` — buzzer (HIGH = ON)
- GPIO 20: `STATUS_LED` — status LED
- GPIO 21: `ATO_PUMP_RELAY_PIN` — ATO pump relay (**active LOW**: LOW = ON, HIGH = OFF)
- GPIO 22: `I2C_SDA_PIN` — I2C bus (DS3231 RTC + FRAM MB85RC256V)
- GPIO 23: `I2C_SCL_PIN` — I2C bus

> Note: ATO pump relay is **active LOW** (LOW = ON, HIGH = OFF).

### Key API Endpoints (Production Mode)

Authentication:
- `POST /api/login` — session auth (cookie)
- `POST /api/logout`
- `GET  /api/health` — no auth required

System:
- `GET  /api/status` — full system status JSON
- `POST /api/system-toggle` — enable/disable system (30-min auto-re-enable)
- `POST /api/system-reset` — reset ATO state machine

ATO pump:
- `POST /api/pump/direct-on` — manual pump on (bypass algorithm)
- `POST /api/pump/direct-off` — manual pump off
- `POST /api/pump/stop` — emergency stop
- `GET|POST /api/pump-settings` — pump calibration (flow_rate, volume_per_second)

History/volumes:
- `GET  /api/cycle-history` — ring buffer (60 records, newest-first)
- `POST /api/clear-cycle-history`
- `GET  /api/daily-volume` — rolling 24h stats
- `POST /api/reset-daily-volume`
- `GET  /api/available-volume`
- `POST /api/set-available-volume`
- `POST /api/refill-available-volume`
- `GET  /api/fill-water-max` — daily_limit_ml and dose_ml
- `POST /api/set-fill-water-max` — set daily_limit_ml
- `POST /api/set-dose` — set dose_ml
- `GET  /api/get-statistics` / `POST /api/reset-statistics`

### Dependencies (platformio.ini)

- ESPAsyncWebServer (mathieucarbou) ^3.3.23
- AsyncTCP (mathieucarbou) ^3.2.14
- ArduinoJson ^7.4.2
- RTClib ^2.1.1 (Adafruit)
- Adafruit FRAM I2C

## Important Patterns

### Credential Loading

Credentials loaded at boot from AES-256 encrypted FRAM. Fallback hardcoded values exist if FRAM empty/corrupted. Access via macros in `config.h`:
- `WIFI_SSID_DYNAMIC`, `WIFI_PASSWORD_DYNAMIC`
- `ADMIN_PASSWORD_HASH_DYNAMIC`, `VPS_AUTH_TOKEN_DYNAMIC`
- `DEVICE_ID_DYNAMIC`

### System Disable/Enable

Global system control with 30-minute auto-re-enable timeout. Functions in `config.h`:
- `setSystemState(bool enabled)`
- `isSystemDisabled()`
- `checkSystemAutoEnable()`

When system is disabled, WaterAlgorithm stops immediately.

### POST Body Parameters

ESPAsyncWebServer `request->hasParam("name", true)` only parses `application/x-www-form-urlencoded`.
All POST handlers use form-encoded body — never JSON for POST requests.

### 24-Hour Auto-Restart

System automatically restarts after 24 hours uptime (`main.cpp`). ATO pump safely stopped before restart.

### Timezone Handling

Configured for Poland (CET/CEST) with automatic DST. RTC stores UTC; all display/schedule times converted via `localtime_r()`. NTP servers use IP addresses for DNS independence.

### I2C Initialization Order

`Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)` must be called **before** `initFRAM()` / `initNVS()`.
Both provisioning and production paths in `main.cpp` call Wire.begin() first.
