#include "rtc_controller.h"
#include "../core/logging.h"
#include "../hardware/hardware_pins.h"
#include <Wire.h>
#include <RTClib.h>
#include <WiFi.h>
#include <time.h>
#include "../network/wifi_manager.h"


// ===============================
// NTP & TIMEZONE CONFIGURATION
// ===============================

// Multiple NTP servers for redundancy
const char* NTP_SERVER_1 = "216.239.35.0";    // time.google.com
const char* NTP_SERVER_2 = "216.239.35.4";    // time2.google.com  
const char* NTP_SERVER_3 = "162.159.200.1";   // time.cloudflare.com

const char* POLAND_TZ = "CET-1CEST,M3.5.0,M10.5.0/3";


// ===============================
// RTC STATE
// ===============================
RTC_DS3231 rtc;
bool rtcInitialized = false;
bool useInternalRTC = false;
bool rtcNeedsSync = false;
bool batteryIssueDetected = false;
bool timezoneConfigured = false;  // 🆕 Track if TZ is set

// Internal RTC fallback structure
struct {
    int year = 2024;
    int month = 1;
    int day = 1;
    int hour = 12;
    int minute = 0;
    int second = 0;
    unsigned long lastUpdate = 0;
} internalTime;

// NTP sync tracking
unsigned long lastNTPSync = 0;
const unsigned long NTP_SYNC_INTERVAL = 3600000;  // 1 hour

// ===============================
// TIMEZONE CONFIGURATION
// ===============================

void configureTimezone() {
    if (timezoneConfigured) {
        LOG_INFO("Timezone already configured, skipping");
        return;
    }
    
    LOG_INFO("Configuring timezone: Poland (CET/CEST with automatic DST)");
    
    // ✅ Set timezone WITHOUT NTP sync first
    setenv("TZ", POLAND_TZ, 1);
    tzset();
    
    timezoneConfigured = true;
    LOG_INFO("✅ Timezone configured successfully");
}

// ===============================
// HELPER FUNCTIONS
// ===============================

void initInternalTimeFromCompileTime() {
    char month_str[4];
    int day, year, hour, min, sec;
    sscanf(__DATE__, "%s %d %d", month_str, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);
    
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    int month = 1;
    for (int i = 0; i < 12; i++) {
        if (strncmp(month_str, months[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }
    
    internalTime.year = year;
    internalTime.month = month;
    internalTime.day = day;
    internalTime.hour = hour;
    internalTime.minute = min;
    internalTime.second = sec;
    internalTime.lastUpdate = millis();
    
    LOG_INFO("Internal RTC set to compile time: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, min, sec);
}

// ===============================
// NTP SYNCHRONIZATION
// ===============================

bool syncTimeFromNTP() {
    if (!isWiFiConnected()) {
        LOG_WARNING("Cannot sync NTP - WiFi not connected");
        return false;
    }
    
    // ✅ Ensure timezone is configured BEFORE NTP sync
    configureTimezone();
    
    LOG_INFO("Attempting NTP synchronization...");
    LOG_INFO("NTP servers: %s, %s, %s", NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    
    // ✅ Use configTzTime with multiple servers
    configTzTime(POLAND_TZ, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    
    // ✅ Increased timeout - wait up to 20 seconds
    int attempts = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (attempts < 40) {  // 40 * 500ms = 20 seconds
        time(&now);
        if (now > 1600000000) {  // Sprawdź czy timestamp jest sensowny (after 2020)
            localtime_r(&now, &timeinfo);  // ✅ Automatyczna konwersja na lokalny czas
            
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S %Z", &timeinfo);
            LOG_INFO("✅ NTP sync successful: %s", timeStr);
            LOG_INFO("UTC timestamp: %lu", (unsigned long)now);
            
            lastNTPSync = millis();
            return true;
        }
        delay(500);
        attempts++;
        
        // Log progress every 5 seconds
        if (attempts % 10 == 0) {
            LOG_INFO("NTP sync in progress... (%d/40)", attempts);
        }
    }
    
    LOG_ERROR("❌ NTP synchronization failed after 20 seconds");
    LOG_WARNING("Possible causes: Firewall blocking port 123 (UDP), DNS issues, or slow connection");
    return false;
}

bool setRTCFromNTP() {
    if (!syncTimeFromNTP()) {
        return false;
    }
    
    // ✅ Pobierz UTC timestamp z systemu (NTP już zsynchronizowany)
    time_t ntp_time;
    time(&ntp_time);
    
    LOG_INFO("NTP returned UTC timestamp: %lu", (unsigned long)ntp_time);
    
    // ✅ Zapisz UTC BEZPOŚREDNIO do RTC (bez żadnych offsetów)
    rtc.adjust(DateTime(ntp_time));
    
    // Weryfikacja z konwersją na lokalny czas dla loga
    DateTime rtc_utc = rtc.now();
    time_t rtc_timestamp = rtc_utc.unixtime();
    struct tm local_time;
    localtime_r(&rtc_timestamp, &local_time);
    
    char localStr[64];
    strftime(localStr, sizeof(localStr), "%Y-%m-%d %H:%M:%S %Z", &local_time);
    
    LOG_INFO("✅ RTC updated with UTC: %04d-%02d-%02d %02d:%02d:%02d",
             rtc_utc.year(), rtc_utc.month(), rtc_utc.day(),
             rtc_utc.hour(), rtc_utc.minute(), rtc_utc.second());
    LOG_INFO("Local time equivalent: %s", localStr);
    
    return true;
}

// ===============================
// RTC INITIALIZATION
// ===============================

void initializeRTC() {
    LOG_INFO("Starting RTC initialization...");
    
    // ✅ KLUCZOWE: Ustaw strefę czasową NA POCZĄTKU, niezależnie od NTP
    configureTimezone();
    
    // Reset flags
    rtcNeedsSync = false;
    batteryIssueDetected = false;
    
    LOG_INFO("Attempting to initialize external DS3231 RTC...");

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000);

    // I2C bus scan: log all responding addresses (debug, remove after HW verified)
    LOG_INFO("I2C scan (SDA=GPIO%d, SCL=GPIO%d):", I2C_SDA_PIN, I2C_SCL_PIN);
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            LOG_INFO("  I2C device found at 0x%02X", addr);
        }
    }

    Wire.beginTransmission(0x68);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
        LOG_INFO("DS3231 detected on I2C bus");
        
        if (rtc.begin()) {
            // Sprawdź czy RTC stracił zasilanie (bateria wyczerpana)
            if (rtc.lostPower()) {
                LOG_WARNING("⚠️ RTC lost power - battery dead or removed");
                LOG_INFO("Setting RTC to compile time to clear OSF flag...");
                
                batteryIssueDetected = true;
                rtcNeedsSync = true;
                
                // Ustaw na compile time aby wyczyścić flagę OSF
                DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
                rtc.adjust(compileTime);
                delay(100);
                
                if (rtc.lostPower()) {
                    LOG_ERROR("Failed to clear RTC OSF flag - hardware problem");
                    useInternalRTC = true;
                    rtcInitialized = true;
                    initInternalTimeFromCompileTime();
                    return;
                }
                
                LOG_INFO("OSF flag cleared");
                LOG_WARNING("⚠️ Battery issue detected - replace CR2032 battery");
                
                // Próba synchronizacji z NTP
                LOG_INFO("Attempting immediate NTP sync after battery loss...");
                
                if (setRTCFromNTP()) {
                    LOG_INFO("✅ RTC synchronized with NTP after battery replacement");
                    // LOG_INFO("Battery warning cleared");
                    // batteryIssueDetected = false;  // Clear flag after successful NTP sync
                    rtcNeedsSync = false;
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("⚠️ NTP sync failed - RTC will use compile time until next sync");
                    LOG_INFO("System will retry NTP sync every hour");
                }
            }
            
            // Weryfikacja czasu RTC
            DateTime now = rtc.now();
            DateTime compileTime = DateTime(F(__DATE__), F(__TIME__));
            
            LOG_INFO("RTC current time (UTC): %04d-%02d-%02d %02d:%02d:%02d", 
                     now.year(), now.month(), now.day(),
                     now.hour(), now.minute(), now.second());
            
            // Walidacja komponentów czasu
            bool timeValid = true;
            if (now.year() < 2020 || now.year() > 2035) {
                LOG_ERROR("RTC year invalid: %d", now.year());
                timeValid = false;
            }
            if (now.month() < 1 || now.month() > 12) {
                LOG_ERROR("RTC month invalid: %d", now.month());
                timeValid = false;
            }
            if (now.day() < 1 || now.day() > 31) {
                LOG_ERROR("RTC day invalid: %d", now.day());
                timeValid = false;
            }
            
            if (!timeValid) {
                LOG_ERROR("RTC has invalid time components");
                rtcNeedsSync = true;
                
                if (setRTCFromNTP()) {
                    LOG_INFO("✅ RTC restored from NTP");
                    rtcNeedsSync = false;
                    batteryIssueDetected = false;
                    rtcInitialized = true;
                    useInternalRTC = false;
                    return;
                } else {
                    LOG_WARNING("NTP sync failed, switching to fallback");
                    useInternalRTC = true;
                    rtcInitialized = true;
                    initInternalTimeFromCompileTime();
                    return;
                }
            }
            
            // RTC OK
            LOG_INFO("✅ RTC time verification successful");
            rtcInitialized = true;
            useInternalRTC = false;
            
            if (!batteryIssueDetected && !rtcNeedsSync) {
                LOG_INFO("RTC appears accurate and battery OK");
            } else if (rtcNeedsSync) {
                LOG_INFO("RTC operational but will sync with NTP when available");
            }
            
            return;
            
        } else {
            LOG_ERROR("Failed to initialize DS3231 RTC");
        }
        
    } else {
        LOG_WARNING("DS3231 not found on I2C bus (error: %d)", error);
    }
    
    // Fallback do wewnętrznego RTC ESP32
    LOG_INFO("Falling back to ESP32 system time");
    useInternalRTC = true;
    rtcInitialized = true;
    rtcNeedsSync = true;
    
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        LOG_WARNING("Failed to get system time, using compile time");
        initInternalTimeFromCompileTime();
    } else {
        internalTime.year = timeinfo.tm_year + 1900;
        internalTime.month = timeinfo.tm_mon + 1;
        internalTime.day = timeinfo.tm_mday;
        internalTime.hour = timeinfo.tm_hour;
        internalTime.minute = timeinfo.tm_min;
        internalTime.second = timeinfo.tm_sec;
        internalTime.lastUpdate = millis();
        
        LOG_INFO("Internal RTC set from system time");
    }
}

// ===============================
// PUBLIC API
// ===============================

String getCurrentTimestamp() {
    static String lastValidTimestamp = "";
    static uint32_t lastValidTime = 0;
    
    if (!rtcInitialized) {
        static uint32_t lastWarning = 0;
        if (millis() - lastWarning > 30000) {
            LOG_ERROR("RTC not initialized in getCurrentTimestamp()");
            lastWarning = millis();
        }
        return lastValidTimestamp.length() > 0 ? lastValidTimestamp : "RTC_NOT_INITIALIZED";
    }

    if (useInternalRTC) {
        time_t ts = (time_t)getUnixTimestamp();
        struct tm timeinfo;
        localtime_r(&ts, &timeinfo);
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buffer);
    }

    const int MAX_RETRIES = 3;
    DateTime now;
    bool validRead = false;

    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        now = rtc.now();
        
        if (now.year() >= 2024 && now.year() <= 2030) {
            validRead = true;
            break;
        }
        
        if (retry == 0) {
            LOG_WARNING("RTC read invalid (attempt %d/%d): %04d-%02d-%02d", 
                       retry + 1, MAX_RETRIES,
                       now.year(), now.month(), now.day());
        }
        
        delay(10);
    }
    
    if (!validRead) {
        static uint32_t lastError = 0;
        if (millis() - lastError > 10000) {
            LOG_ERROR("RTC read failed after %d retries", MAX_RETRIES);
            lastError = millis();
        }
        
        if (lastValidTimestamp.length() > 0 && 
            (millis() - lastValidTime) < 10000) {
            LOG_WARNING("Using cached timestamp (age: %dms)", millis() - lastValidTime);
            return lastValidTimestamp;
        }
        
        return "RTC_ERROR";
    }
    
    // Convert UTC → Local
    time_t utc = now.unixtime();
    struct tm timeinfo;
    localtime_r(&utc, &timeinfo);
    
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    lastValidTimestamp = String(buffer);
    lastValidTime = millis();
    
    return String(buffer);
}

unsigned long getUnixTimestamp() {
    if (!rtcInitialized) {
        return 1609459200;  // 2021-01-01 00:00:00 UTC
    }
    
    if (useInternalRTC) {
        struct tm tm_time;
        tm_time.tm_year = internalTime.year - 1900;
        tm_time.tm_mon = internalTime.month - 1;
        tm_time.tm_mday = internalTime.day;
        tm_time.tm_hour = internalTime.hour;
        tm_time.tm_min = internalTime.minute;
        tm_time.tm_sec = internalTime.second;
        tm_time.tm_isdst = 0;
        
        return mktime(&tm_time);
    } else {
        // ✅ RETRY MECHANISM - jak w getCurrentTimestamp()
        const int MAX_RETRIES = 3;
        DateTime now;
        bool validRead = false;
        
        for (int retry = 0; retry < MAX_RETRIES; retry++) {
            now = rtc.now();
            
            // ✅ Walidacja roku
            if (now.year() >= 2024 && now.year() <= 2035) {
                validRead = true;
                break;
            }
            
            if (retry < MAX_RETRIES - 1) {
                delay(10);
            }
        }
        
        if (!validRead) {
            static uint32_t lastError = 0;
            if (millis() - lastError > 10000) {
                LOG_ERROR("RTC read failed in getUnixTimestamp(), year: %d", now.year());
                lastError = millis();
            }
            // ✅ Fallback: zwróć sensowną wartość zamiast śmieci
            return 1735689600;  // 2025-01-01 00:00:00 UTC
        }
        
        return now.unixtime();
    }
}

bool isRTCWorking() {
    if (!rtcInitialized) {
        return false;
    }
    
    if (useInternalRTC) {
        return true;  // Internal RTC zawsze "działa"
    } else {
        DateTime now = rtc.now();
        return (now.year() >= 2020 && now.year() <= 2035);
    }
}

String getRTCInfo() {

    if (!rtcInitialized) {
        return "RTC not initialized";
    }
    
    if (useInternalRTC) {
        if (rtcNeedsSync) {
            return "ESP32 system time (needs NTP sync)";
        }
        return "ESP32 system time (fallback)";
    } else {
        if (batteryIssueDetected) {
            return "DS3231 RTC (⚠️ BATTERY DEAD - replace CR2032!)";
        } else if (rtcNeedsSync) {
            return "DS3231 RTC (syncing with NTP...)";
        }
        return "DS3231 RTC (synchronized)";
    }
}

String getTimeSourceInfo() {
    return getRTCInfo();
}

bool isRTCHardware() {
    return rtcInitialized && !useInternalRTC;
}

bool rtcNeedsSynchronization() {
    return batteryIssueDetected || rtcNeedsSync;
}

bool isBatteryIssueDetected() {
    return batteryIssueDetected;
}
