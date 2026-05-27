#include "ap_core.h"
#include "prov_config.h"
#include "../core/logging.h"

// Global DNS server instance
static DNSServer dnsServer;
static bool apActive = false;
static bool dnsActive = false;

bool startAccessPoint() {
    LOG_INFO("Starting Access Point...");
    LOG_INFO("  SSID: %s", PROV_AP_SSID);
    LOG_INFO("  Password: %s", PROV_AP_PASSWORD);
    LOG_INFO("  Channel: %d", PROV_AP_CHANNEL);
    
    // Disconnect from any existing WiFi
    WiFi.mode(WIFI_AP);
    delay(100);
    
    // Start AP with password (WPA2)
    bool success = WiFi.softAP(
        PROV_AP_SSID, 
        PROV_AP_PASSWORD, 
        PROV_AP_CHANNEL, 
        0,  // SSID not hidden
        PROV_AP_MAX_CLIENTS
    );
    
    if (!success) {
        LOG_ERROR("Failed to start Access Point!");
        return false;
    }
    
    // Wait for AP to stabilize
    delay(500);
    
    IPAddress apIP = WiFi.softAPIP();
    LOG_INFO("✓ Access Point started successfully");
    LOG_INFO("  IP Address: %s", apIP.toString().c_str());
    LOG_INFO("  Gateway: %s", apIP.toString().c_str());
    LOG_INFO("  Subnet: 255.255.255.0");
    
    apActive = true;
    return true;
}

bool startDNSServer() {
    if (!apActive) {
        LOG_ERROR("Cannot start DNS - AP not active!");
        return false;
    }
    
    LOG_INFO("Starting DNS server for captive portal...");
    
    // Start DNS server on port 53
    // Redirect ALL domains to AP IP address
    IPAddress apIP = WiFi.softAPIP();
    
    bool success = dnsServer.start(
        PROV_DNS_PORT,      // Port 53
        "*",                // Capture all domains
        apIP                // Redirect to AP IP
    );
    
    if (!success) {
        LOG_ERROR("Failed to start DNS server!");
        return false;
    }
    
    LOG_INFO("✓ DNS server started on port %d", PROV_DNS_PORT);
    LOG_INFO("  All domains redirected to: %s", apIP.toString().c_str());
    
    dnsActive = true;
    return true;
}

void stopAccessPoint() {
    LOG_INFO("Stopping Access Point and DNS server...");
    
    if (dnsActive) {
        dnsServer.stop();
        dnsActive = false;
        LOG_INFO("✓ DNS server stopped");
    }
    
    if (apActive) {
        WiFi.softAPdisconnect(true);
        apActive = false;
        LOG_INFO("✓ Access Point stopped");
    }
}

void runProvisioningLoop() {
    LOG_INFO("=== ENTERING PROVISIONING LOOP ===");
    LOG_INFO("This is a BLOCKING loop - device will not continue to production mode");
    LOG_INFO("Configuration must be completed and device manually restarted");
    LOG_INFO("");
    
    unsigned long lastStatusPrint = 0;
    const unsigned long STATUS_INTERVAL = 30000; // Print status every 30s
    
    while (true) {
        // Process DNS requests - CRITICAL for captive portal
        if (dnsActive) {
            dnsServer.processNextRequest();
        }
        
        // Print status periodically
        unsigned long now = millis();
        if (now - lastStatusPrint >= STATUS_INTERVAL) {
            int clients = WiFi.softAPgetStationNum();
            LOG_INFO("Provisioning active - %d client(s) connected", clients);
            lastStatusPrint = now;
        }
        
        // Small delay to prevent watchdog timeout
        delay(10);
        
        // TODO ETAP 1: Web server request handling will be added here
        // The loop will continue until manual restart after configuration save
    }
}

IPAddress getAPIPAddress() {
    return WiFi.softAPIP();
}