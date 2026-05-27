#ifndef AP_CORE_H
#define AP_CORE_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>

// ===============================
// ACCESS POINT CORE FUNCTIONS
// ===============================

/**
 * Initialize and start Access Point mode
 * Creates WiFi AP with SSID and password from prov_config.h
 * 
 * @return true if AP started successfully
 */
bool startAccessPoint();

/**
 * Initialize and start DNS server for captive portal
 * Hijacks all DNS requests to redirect to ESP32 IP
 * 
 * @return true if DNS server started successfully
 */
bool startDNSServer();

/**
 * Stop Access Point and DNS server
 * Called when exiting provisioning mode
 */
void stopAccessPoint();

/**
 * Main provisioning mode blocking loop
 * Handles DNS requests and keeps AP alive
 * Never returns until device is manually restarted
 */
void runProvisioningLoop();

/**
 * Get AP IP address (typically 192.168.4.1)
 * 
 * @return IP address of the Access Point
 */
IPAddress getAPIPAddress();

#endif