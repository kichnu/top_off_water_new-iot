#include "wifi_scanner.h"
#include "../core/logging.h"
#include <WiFi.h>
#include <ArduinoJson.h>

String getEncryptionTypeName(uint8_t encryptionType) {
    switch (encryptionType) {
        case WIFI_AUTH_OPEN:
            return "Open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-Enterprise";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3";
        default:
            return "Unknown";
    }
}

String scanWiFiNetworksJSON(bool sortByRSSI, bool removeDuplicates) {
    LOG_INFO("Starting WiFi network scan...");
    
    // Start scan (blocking)
    int networkCount = WiFi.scanNetworks();
    
    if (networkCount < 0) {
        LOG_ERROR("WiFi scan failed!");
        return "{\"networks\":[],\"count\":0,\"error\":\"Scan failed\"}";
    }
    
    LOG_INFO("Found %d networks", networkCount);
    
    // Create JSON document (allocate enough for ~20 networks)
    JsonDocument doc;
    JsonArray networks = doc["networks"].to<JsonArray>();
    
    // Track seen SSIDs for duplicate removal
    String seenSSIDs[50];  // Max 50 unique SSIDs
    int seenCount = 0;
    
    for (int i = 0; i < networkCount && i < 50; i++) {
        String ssid = WiFi.SSID(i);
        int32_t rssi = WiFi.RSSI(i);
        uint8_t encryption = WiFi.encryptionType(i);
        int32_t channel = WiFi.channel(i);
        
        // Skip hidden networks (empty SSID)
        if (ssid.length() == 0) {
            continue;
        }
        
        // Check for duplicates if requested
        bool isDuplicate = false;
        if (removeDuplicates) {
            for (int j = 0; j < seenCount; j++) {
                if (seenSSIDs[j] == ssid) {
                    isDuplicate = true;
                    break;
                }
            }
        }
        
        if (isDuplicate) {
            continue;  // Skip duplicate SSID
        }
        
        // Add to seen list
        if (seenCount < 50) {
            seenSSIDs[seenCount++] = ssid;
        }
        
        // Add network to JSON
        JsonObject network = networks.add<JsonObject>();
        network["ssid"] = ssid;
        network["rssi"] = rssi;
        network["encryption"] = encryption;
        network["encryptionName"] = getEncryptionTypeName(encryption);
        network["channel"] = channel;
        
        // Add signal quality percentage (0-100)
        int quality = 0;
        if (rssi >= -50) {
            quality = 100;
        } else if (rssi <= -100) {
            quality = 0;
        } else {
            quality = 2 * (rssi + 100);
        }
        network["quality"] = quality;
    }
    
    doc["count"] = networks.size();
    
    // Serialize to string
    String jsonString;
    serializeJson(doc, jsonString);
    
    LOG_INFO("Scan complete - returning %d unique networks", networks.size());
    
    // Clean up WiFi scan
    WiFi.scanDelete();
    
    return jsonString;
}