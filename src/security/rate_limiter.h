#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <Arduino.h>
#include <WiFi.h>

void initRateLimiter();
void updateRateLimiter();
bool isRateLimited(IPAddress ip);
void recordRequest(IPAddress ip);
void recordFailedAttempt(IPAddress ip);
bool isIPBlocked(IPAddress ip);

#endif