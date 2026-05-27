#include "rate_limiter.h"
#include "../config/config.h"
#include "../security/auth_manager.h"
#include "../core/logging.h"
#include <map>
#include <vector>

static std::map<uint32_t, String> ipStringCache;

struct RateLimitData {
    std::vector<unsigned long> requestTimes;
    int failedAttempts;
    unsigned long blockUntil;
    unsigned long lastRequest;
};

std::map<String, RateLimitData> rateLimitData;

void initRateLimiter() {
    rateLimitData.clear();
    LOG_INFO("Rate limiter initialized");
}

void updateRateLimiter() {
    unsigned long now = millis();
    
    // Clean old data every 5 minutes
    static unsigned long lastCleanup = 0;
    if (now - lastCleanup > 300000) {
        for (auto it = rateLimitData.begin(); it != rateLimitData.end();) {
            if (now - it->second.lastRequest > 300000) {
                it = rateLimitData.erase(it);
            } else {
                ++it;
            }
        }
        lastCleanup = now;
    }
}

bool isRateLimited(IPAddress ip) {
    if (isIPAllowed(ip)) {
        return false; // No rate limiting for whitelisted IPs
    }
    
    String ipStr = ip.toString();
    if (rateLimitData.find(ipStr) == rateLimitData.end()) {
        return false;
    }
    
    RateLimitData& data = rateLimitData[ipStr];
    unsigned long now = millis();
    
    // Check if blocked
    if (data.blockUntil > now) {
        return true;
    }
    
    // Clean old requests
    data.requestTimes.erase(
        std::remove_if(data.requestTimes.begin(), data.requestTimes.end(),
                      [now](unsigned long time) { return now - time > RATE_LIMIT_WINDOW_MS; }),
        data.requestTimes.end());
    
    return data.requestTimes.size() >= MAX_REQUESTS_PER_SECOND;
}

void recordRequest(IPAddress ip) {
        // ✅ CACHE dla IP string conversion
    uint32_t ipUint = ip;
 
    String ipStr = ip.toString();
    unsigned long now = millis();
    
    auto cachedIP = ipStringCache.find(ipUint);
    if (cachedIP != ipStringCache.end()) {
        ipStr = cachedIP->second;
    } else {
        ipStr = ip.toString();
        ipStringCache[ipUint] = ipStr;
        
        // Limit cache size (max 10 IPs)
        if (ipStringCache.size() > 10) {
            auto oldest = ipStringCache.begin();
            ipStringCache.erase(oldest);
        }
    }

    RateLimitData& data = rateLimitData[ipStr];
    
    // 🔍 DEBUGGING - rozmiar PRZED czyszczeniem
    size_t sizeBefore = data.requestTimes.size();
    size_t totalIPsBefore = rateLimitData.size();
    
    // ✅ CZYŚĆ STARE przed dodaniem nowego
    data.requestTimes.erase(
        std::remove_if(data.requestTimes.begin(), data.requestTimes.end(),
                      [now](unsigned long time) { 
                          return now - time > RATE_LIMIT_WINDOW_MS; 
                      }),
        data.requestTimes.end());
    
    size_t sizeAfterCleanup = data.requestTimes.size();
    size_t cleaned = sizeBefore - sizeAfterCleanup;
    
    // ✅ HARD LIMIT - maksymalnie 20 timestampów
    size_t hardLimitCleaned = 0;
    if (data.requestTimes.size() >= 20) {
        hardLimitCleaned = 10;
        data.requestTimes.erase(data.requestTimes.begin(), 
                               data.requestTimes.begin() + 10);
    }
    
    data.requestTimes.push_back(now);
    data.lastRequest = now;
    
    // 🔍 DEBUGGING - rozmiar PO wszystkich operacjach
    size_t sizeFinal = data.requestTimes.size();
    
    // 🔍 LOG co 50 requestów LUB gdy było czyszczenie
    static int requestCounter = 0;
    requestCounter++;
}

void recordFailedAttempt(IPAddress ip) {
    if (isIPAllowed(ip)) {
        return; // No blocking for whitelisted IPs
    }
    
    String ipStr = ip.toString();
    unsigned long now = millis();
    
    RateLimitData& data = rateLimitData[ipStr];
    data.failedAttempts++;
    
    if (data.failedAttempts >= MAX_FAILED_ATTEMPTS) {
        data.blockUntil = now + BLOCK_DURATION_MS;
        data.failedAttempts = 0;
        LOG_WARNING("IP %s blocked for failed attempts", ipStr.c_str());
    }
}

bool isIPBlocked(IPAddress ip) {
    if (isIPAllowed(ip)) {
        return false;
    }
    String ipStr = ip.toString();
    if (rateLimitData.find(ipStr) == rateLimitData.end()) {
        return false;
    }
    return rateLimitData[ipStr].blockUntil > millis();
}