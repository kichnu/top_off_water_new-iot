#include "ap_server.h"
#include "ap_html.h"
#include "wifi_scanner.h"
#include "ap_handlers.h"
#include "prov_config.h"
#include "../core/logging.h"
#include <AsyncJson.h>
#include <ESPAsyncWebServer.h>

// Global web server instance
static AsyncWebServer* webServer = nullptr;
static bool serverActive = false;

bool startWebServer() {
    LOG_INFO("Starting web server on port %d...", PROV_WEB_PORT);
    
    // Create web server instance
    webServer = new AsyncWebServer(PROV_WEB_PORT);
    
    if (!webServer) {
        LOG_ERROR("Failed to allocate web server!");
        return false;
    }
    
    // ===== MAIN SETUP PAGE =====
    webServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("Serving setup page to client: %s", request->client()->remoteIP().toString().c_str());
        request->send(200, "text/html", getSetupPageHTML());
    });

    // ===== API: Current config (non-sensitive, for pre-population) =====
    webServer->on("/api/prov/config", HTTP_GET, [](AsyncWebServerRequest *request){
        handleProvConfig(request);
    });

    // ===== API: WiFi Network Scan =====
    webServer->on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("WiFi scan requested by: %s", request->client()->remoteIP().toString().c_str());
        
        // Run scan and get JSON
        String networksJSON = scanWiFiNetworksJSON(true, true);
        
        // Send JSON response
        request->send(200, "application/json", networksJSON);
        
        LOG_INFO("WiFi scan results sent");
    });

    // ===== API: Configuration Submit =====
    AsyncCallbackJsonWebHandler* configHandler = new AsyncCallbackJsonWebHandler(
        "/api/configure", 
        [](AsyncWebServerRequest *request, JsonVariant &json) {
            handleConfigureSubmit(request, json);
        }
    );
    webServer->addHandler(configHandler);
    
    // ===== CAPTIVE PORTAL DETECTION - iOS =====
    webServer->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("iOS captive portal detection from: %s", request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    webServer->on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("iOS captive portal check from: %s", request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    // ===== CAPTIVE PORTAL DETECTION - Android =====
    webServer->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("Android captive portal detection from: %s", request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    webServer->on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("Android captive portal check from: %s", request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    // ===== CAPTIVE PORTAL DETECTION - Windows =====
    webServer->on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("Windows captive portal detection from: %s", request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    webServer->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request){
        LOG_INFO("Windows NCSI check from: %s", request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    // ===== CATCH-ALL for any other requests =====
    webServer->onNotFound([](AsyncWebServerRequest *request){
        LOG_INFO("Catch-all redirect for: %s from %s", 
                 request->url().c_str(), 
                 request->client()->remoteIP().toString().c_str());
        request->redirect("/");
    });
    
    // Start the server
    webServer->begin();
    serverActive = true;
    
    LOG_INFO("✓ Web server started successfully");
    LOG_INFO("  Serving on: http://192.168.4.1/");
    LOG_INFO("  Captive portal endpoints configured");
    
    return true;
}

void stopWebServer() {
    if (webServer && serverActive) {
        LOG_INFO("Stopping web server...");
        webServer->end();
        delete webServer;
        webServer = nullptr;
        serverActive = false;
        LOG_INFO("✓ Web server stopped");
    }
}

bool isWebServerActive() {
    return serverActive;
}