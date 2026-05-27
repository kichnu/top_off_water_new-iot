#include "session_manager.h"
#include "../config/config.h"
#include "../core/logging.h"

// ✅ FIX 3: Add session limits to prevent memory exhaustion
static const size_t MAX_TOTAL_SESSIONS = 10;       // Maximum total sessions
static const size_t MAX_SESSIONS_PER_IP = 3;       // Maximum sessions per IP

std::vector<Session> activeSessions;

void initSessionManager() {
    activeSessions.clear();
    activeSessions.reserve(MAX_TOTAL_SESSIONS); // Pre-allocate to avoid reallocations
    LOG_INFO("Session manager initialized (max %zu sessions)", MAX_TOTAL_SESSIONS);
}
// ✅ Helper function to remove oldest session if needed
void removeOldestSessionIfNeeded() {
    if (activeSessions.size() >= MAX_TOTAL_SESSIONS) {
        // Find and remove the oldest session
        auto oldestIt = activeSessions.begin();
        for (auto it = activeSessions.begin(); it != activeSessions.end(); ++it) {
            if (it->createdAt < oldestIt->createdAt) {
                oldestIt = it;
            }
        }
        
        if (oldestIt != activeSessions.end()) {
            LOG_WARNING("Session limit reached, removing oldest session for IP: %s", 
                       oldestIt->ip.toString().c_str());
            activeSessions.erase(oldestIt);
        }
    }
}

// ✅ Helper function to count sessions for specific IP
size_t countSessionsForIP(IPAddress ip) {
    size_t count = 0;
    for (const auto& session : activeSessions) {
        if (session.ip == ip && session.isValid) {
            count++;
        }
    }
    return count;
}

void updateSessionManager() {
    unsigned long now = millis();
    
    for (auto it = activeSessions.begin(); it != activeSessions.end();) {
        if (!it->isValid || (now - it->lastActivity > SESSION_TIMEOUT_MS)) {
            LOG_INFO("Removing expired session for IP: %s", it->ip.toString().c_str());
            it = activeSessions.erase(it);
        } else {
            ++it;
        }
    }
}

String createSession(IPAddress ip) {
    // ✅ FIX 3: Check session limits before creating new session
    // Check per-IP limit
    size_t sessionsForIP = countSessionsForIP(ip);
    if (sessionsForIP >= MAX_SESSIONS_PER_IP) {
        LOG_WARNING("Too many sessions for IP %s (%zu/%zu), removing oldest", 
                   ip.toString().c_str(), sessionsForIP, MAX_SESSIONS_PER_IP);
        
        // Remove oldest session for this IP
        auto oldestForIP = activeSessions.end();
        for (auto it = activeSessions.begin(); it != activeSessions.end(); ++it) {
            if (it->ip == ip && it->isValid) {
                if (oldestForIP == activeSessions.end() || it->createdAt < oldestForIP->createdAt) {
                    oldestForIP = it;
                }
            }
        }
        
        if (oldestForIP != activeSessions.end()) {
            activeSessions.erase(oldestForIP);
        }
    }
    
    // Check total session limit
    if (activeSessions.size() >= MAX_TOTAL_SESSIONS) {
        removeOldestSessionIfNeeded();
    }
    
    // Create new session
    Session newSession;
    newSession.token = "";
    
    // ✅ Improved token generation with better randomness
    randomSeed(millis() ^ micros() ^ analogRead(A0));
    for (int i = 0; i < 32; i++) {
        newSession.token += String(random(0, 16), HEX);
    }
    
    newSession.ip = ip;
    newSession.createdAt = millis();
    newSession.lastActivity = millis();
    newSession.isValid = true;
    
    // ✅ Double-check we're not exceeding limits (paranoid check)
    if (activeSessions.size() >= MAX_TOTAL_SESSIONS) {
        LOG_ERROR("Critical: Session creation would exceed limits!");
        return ""; // Return empty token to indicate failure
    }
    
    activeSessions.push_back(newSession);
    
    LOG_INFO("Session created for IP: %s (total sessions: %zu/%zu)", 
             ip.toString().c_str(), activeSessions.size(), MAX_TOTAL_SESSIONS);
    return newSession.token;
}

bool validateSession(const String& token, IPAddress ip) {
    if (token.length() == 0) {
        return false;
    }
    
    for (auto& session : activeSessions) {
        if (session.token == token && session.ip == ip && session.isValid) {
            if (millis() - session.lastActivity > SESSION_TIMEOUT_MS) {
                session.isValid = false;
                LOG_INFO("Session expired for IP: %s", ip.toString().c_str());
                return false;
            }
            session.lastActivity = millis();
            return true;
        }
    }
    return false;
}

void destroySession(const String& token) {
    for (auto it = activeSessions.begin(); it != activeSessions.end(); ++it) {
        if (it->token == token) {
            LOG_INFO("Session destroyed for IP: %s", it->ip.toString().c_str());
            activeSessions.erase(it);
            break;
        }
    }
}

// ✅ New diagnostic function for monitoring
void getSessionStats(size_t& totalSessions, size_t& maxSessions) {
    totalSessions = activeSessions.size();
    maxSessions = MAX_TOTAL_SESSIONS;
}