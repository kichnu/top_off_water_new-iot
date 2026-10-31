
#include "config.h"
#include "../core/logging.h"
#include "../hardware/fram_controller.h"

// ===============================
// 🔒 SECURE PLACEHOLDER VALUES 
// ===============================
const char* WIFI_SSID = "SETUP_REQUIRED";
const char* WIFI_PASSWORD = "SETUP_REQUIRED";

const char* ADMIN_PASSWORD_HASH = nullptr;  // ✅ Force FRAM setup!

// Trusted proxy IP — NOT in ALLOWED_IPS[] (separate auto-auth path)
const IPAddress TRUSTED_PROXY_IP(10, 99, 0, 1);

const IPAddress ALLOWED_IPS[] = {
    IPAddress(192, 168, 2, 10),
    IPAddress(192, 168, 2, 20),
    IPAddress(192, 168, 2, 187)
};
const int ALLOWED_IPS_COUNT = sizeof(ALLOWED_IPS) / sizeof(ALLOWED_IPS[0]);

// 🔒 SECURE PLACEHOLDER VALUES - NO REAL CREDENTIALS!
const char* DEVICE_ID = "UNCONFIGURED_DEVICE";

// ============== SYSTEM DISABLE/ENABLE ==============
bool systemDisabled = true;   // Boot in Service Mode; auto-switch to Auto Mode after SERVICE_MODE_AUTO_ENABLE_MS
unsigned long serviceBootStartMs = 0;  // Service Mode timer start; 0 = no timer active
static uint32_t serviceTimeoutMs  = SERVICE_MODE_AUTO_ENABLE_MS;

// ============== LEGACY PUMP CONTROL (to be removed) ==============
bool pumpGlobalEnabled = true;  // Default ON
unsigned long pumpDisabledTime = 0;
const unsigned long PUMP_AUTO_ENABLE_MS = 30 * 60 * 1000; // 30 minutes

PumpSettings currentPumpSettings;

// ================= FRAM Storage Functions =================

void initNVS() {
    // The name stays for compatibility, but FRAM is used
    LOG_INFO("Initializing storage (FRAM)...");
    if (initFRAM()) {
        LOG_INFO("Storage system ready (FRAM)");
    } else {
        LOG_ERROR("Storage initialization failed!");
    }
    // Start Service Mode boot timer — auto-switch to Auto Mode after SERVICE_MODE_AUTO_ENABLE_MS
    serviceBootStartMs = millis() | 1u;   // |1 — 0 means no timer
    serviceTimeoutMs   = SERVICE_MODE_AUTO_ENABLE_MS;
    LOG_INFO("Service Mode boot timer started (%lu ms)", SERVICE_MODE_AUTO_ENABLE_MS);
}

void loadVolumeFromNVS() {
    float savedVolume = currentPumpSettings.volumePerSecond; // default
    
    if (loadVolumeFromFRAM(savedVolume)) {
        currentPumpSettings.volumePerSecond = savedVolume;
    } else {
        LOG_INFO("Using default volumePerSecond: %.1f ml/s", 
                 currentPumpSettings.volumePerSecond);
    }
}

void saveVolumeToNVS() {
    if (!saveVolumeToFRAM(currentPumpSettings.volumePerSecond)) {
        LOG_ERROR("Failed to save volume to storage");
    }
}

// ============== SYSTEM DISABLE/ENABLE FUNCTIONS ==============

void setSystemState(bool enabled, uint32_t timeoutMs) {
    systemDisabled = !enabled;
    // Any state change replaces the timer: Service Mode with a timeout starts a new one
    serviceBootStartMs = (!enabled && timeoutMs) ? (millis() | 1u) : 0;
    serviceTimeoutMs   = timeoutMs;
    if (enabled) {
        LOG_INFO("====================================");
        LOG_INFO("AUTO MODE — system enabled");
        LOG_INFO("Algorithm will resume operation");
        LOG_INFO("====================================");
        LOG_INFO("");
    } else {
        LOG_INFO("====================================");
        LOG_INFO("SERVICE MODE — system disabled");
        LOG_INFO("Algorithm will pause at safe point");
        if (timeoutMs) LOG_INFO("Back to Auto Mode in %lu min", (unsigned long)(timeoutMs / 60000UL));
        LOG_INFO("====================================");
        LOG_INFO("");
    }
}

void checkServiceAutoEnable() {
    if (systemDisabled && serviceBootStartMs > 0) {
        if (millis() - serviceBootStartMs >= serviceTimeoutMs) {
            serviceBootStartMs = 0;
            systemDisabled = false;
            LOG_INFO("====================================");
            LOG_INFO("AUTO MODE — Service Mode timeout elapsed (%lu min)", (unsigned long)(serviceTimeoutMs / 60000UL));
            LOG_INFO("Algorithm resuming normal operation");
            LOG_INFO("====================================");
            LOG_INFO("");
        }
    }
}

bool isSystemDisabled() {
    return systemDisabled;
}

bool isBootServiceMode() {
    return systemDisabled && serviceBootStartMs != 0 && serviceTimeoutMs == SERVICE_MODE_AUTO_ENABLE_MS;
}

uint32_t getServiceRemainingMs() {
    if (!systemDisabled || serviceBootStartMs == 0) return 0;
    uint32_t el = millis() - serviceBootStartMs;
    return el < serviceTimeoutMs ? serviceTimeoutMs - el : 0;
}

// ============== LEGACY PUMP CONTROL (to be removed) ==============

void checkPumpAutoEnable() {
    if (!pumpGlobalEnabled && pumpDisabledTime > 0) {
        if (millis() - pumpDisabledTime >= PUMP_AUTO_ENABLE_MS) {
            pumpGlobalEnabled = true;
            pumpDisabledTime = 0;
            LOG_INFO("Pump auto-enabled after 30 minutes");
            LOG_INFO("");
        }
    }
}

void setPumpGlobalState(bool enabled) {
    pumpGlobalEnabled = enabled;
    if (!enabled) {
        pumpDisabledTime = millis();
        LOG_INFO("Pump globally disabled for 30 minutes");
        LOG_INFO("");
    } else {
        pumpDisabledTime = 0;
        LOG_INFO("Pump globally enabled");
        LOG_INFO("");
    }
}