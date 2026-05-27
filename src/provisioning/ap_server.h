#ifndef AP_SERVER_H
#define AP_SERVER_H

#include <Arduino.h>

// ===============================
// PROVISIONING WEB SERVER
// ===============================

/**
 * Initialize and start web server for provisioning
 * Serves HTML pages and handles captive portal detection
 * 
 * @return true if web server started successfully
 */
bool startWebServer();

/**
 * Stop web server
 * Called when exiting provisioning mode
 */
void stopWebServer();

/**
 * Check if web server is running
 * 
 * @return true if server is active
 */
bool isWebServerActive();

#endif