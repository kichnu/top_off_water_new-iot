
#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>

struct Session {
    String token;
    IPAddress ip;
    unsigned long createdAt;
    unsigned long lastActivity;
    bool isValid;
};

void initSessionManager();
void updateSessionManager();
String createSession(IPAddress ip);
bool validateSession(const String& token, IPAddress ip);
void destroySession(const String& token);

// ✅ FIX 3: Add session statistics function
void getSessionStats(size_t& totalSessions, size_t& maxSessions);

#endif