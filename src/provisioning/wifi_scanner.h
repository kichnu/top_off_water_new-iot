#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>

// ===============================
// WIFI NETWORK SCANNER
// ===============================

/**
 * Scan for available WiFi networks
 * Returns JSON array with network information
 * 
 * Format:
 * {
 *   "networks": [
 *     {
 *       "ssid": "NetworkName",
 *       "rssi": -45,
 *       "encryption": 3,
 *       "channel": 6
 *     },
 *     ...
 *   ],
 *   "count": 5
 * }
 * 
 * @param sortByRSSI if true, sort networks by signal strength (strongest first)
 * @param removeDuplicates if true, keep only strongest RSSI for duplicate SSIDs
 * @return JSON string with network list
 */
String scanWiFiNetworksJSON(bool sortByRSSI = true, bool removeDuplicates = true);

/**
 * Get encryption type name as string
 * 
 * @param encryptionType WiFi encryption type
 * @return Human-readable encryption name
 */
String getEncryptionTypeName(uint8_t encryptionType);

#endif