#include "status_led.h"
#include "hardware_pins.h"
#include "../config/config.h"
#include "../hardware/pump_controller.h"
#include "../hardware/mixing_pump.h"
#include "../hardware/peristaltic_pump.h"
#include "../network/wifi_manager.h"
#include "../algorithm/water_algorithm.h"
#include "../algorithm/algorithm_config.h"

// Tracks whether the boot WiFi connection phase is over (connected or gave up).
static bool wifiConnectingDone = false;

void initStatusLed() {
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);
}

// Returns true = LED on, based on current system state (priority order, highest first).
static bool calcLedState() {
    uint32_t now = millis();

    // Priority 1: Error — 3× flash (100ms on/off), then 2s pause
    if (waterAlgorithm.getState() == STATE_ERROR) {
        uint32_t phase = now % 2600;
        return (phase < 100) || (phase >= 200 && phase < 300) || (phase >= 400 && phase < 500);
    }

    // Priority 2: Any pump active — solid on
    if (isPumpActive() || isMixingPumpActive() || isPeristalticPumpRunning()) {
        return true;
    }

    // Priority 3: Service Mode — 2× flash every 3s
    if (isSystemDisabled()) {
        uint32_t phase = now % 3000;
        return (phase < 100) || (phase >= 200 && phase < 300);
    }

    // Priority 4: WiFi connecting (boot phase, not yet connected or timed out)
    if (!wifiConnectingDone && now < 50000UL && !isWiFiConnected()) {
        return (now % 400) < 200;
    }

    // Priority 5: Auto Mode + WiFi OK — heartbeat 100ms every 3s
    if (isWiFiConnected()) {
        wifiConnectingDone = true;
        return (now % 3000) < 100;
    }

    // Priority 6: Auto Mode + no WiFi — heartbeat every 3s,
    // but every 3rd pulse replaced by 3× fast blink (200ms on/off).
    // Full pattern period: 9000ms (3 beats × 3000ms)
    wifiConnectingDone = true;
    uint32_t phase = now % 9000;
    if (phase < 100) return true;                    // beat 1: heartbeat
    if (phase >= 3000 && phase < 3100) return true;  // beat 2: heartbeat
    // beat 3: 3× fast blink at 200ms
    if (phase >= 6000 && phase < 6200) return true;
    if (phase >= 6400 && phase < 6600) return true;
    if (phase >= 6800 && phase < 7000) return true;
    return false;
}

void updateStatusLed() {
    digitalWrite(STATUS_LED, calcLedState() ? HIGH : LOW);
}
