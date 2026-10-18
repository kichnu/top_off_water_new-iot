#include "ap_handlers.h"
#include "credentials_validator.h"
#include "../core/logging.h"
#include "../crypto/fram_encryption.h"
#include "../hardware/fram_controller.h"
#include "../hardware/fram_constants.h"
#include <stddef.h>

// ── Helper: build FRAMCredentials with optional partial update ────────────────
//   newWifiPassword / newAdminPassword empty → copy encrypted blob from existing
static bool buildFRAMCredentials(
    const String& deviceName,
    const String& wifiSSID,
    const String& newWifiPassword,
    const String& newAdminPassword,
    const FRAMCredentials* existing,
    FRAMCredentials& out)
{
    bool allNew = (newWifiPassword.length() > 0 && newAdminPassword.length() > 0);

    if (allNew) {
        DeviceCredentials creds;
        creds.device_name    = deviceName;
        creds.wifi_ssid      = wifiSSID;
        creds.wifi_password  = newWifiPassword;
        creds.admin_password = newAdminPassword;
        creds.vps_token      = "";
        creds.vps_url        = "";
        return encryptCredentials(creds, out);
    }

    // Partial update — device_name unchanged (caller enforces), reuse existing IV+key
    memset(&out, 0, sizeof(FRAMCredentials));
    out.magic   = FRAM_MAGIC_NUMBER;
    out.version = 0x0003;
    strncpy(out.device_name, deviceName.c_str(), 31);
    out.device_name[31] = '\0';

    memcpy(out.iv, existing->iv, 8);

    uint8_t key[AES_KEY_SIZE];
    if (!generateEncryptionKey(deviceName, key)) return false;

    // WiFi SSID — always re-encrypt (may have changed)
    size_t clen = 64;
    if (!encryptData((const uint8_t*)wifiSSID.c_str(), wifiSSID.length(),
                     key, out.iv, out.encrypted_wifi_ssid, &clen)) return false;

    // WiFi password
    if (newWifiPassword.length() > 0) {
        clen = 128;
        if (!encryptData((const uint8_t*)newWifiPassword.c_str(), newWifiPassword.length(),
                         key, out.iv, out.encrypted_wifi_password, &clen)) return false;
    } else {
        memcpy(out.encrypted_wifi_password, existing->encrypted_wifi_password, 128);
    }

    // Admin password
    if (newAdminPassword.length() > 0) {
        uint8_t hash[SHA256_HASH_SIZE];
        if (!sha256Hash(newAdminPassword, hash)) return false;
        String hashHex = "";
        for (int i = 0; i < SHA256_HASH_SIZE; i++) {
            if (hash[i] < 16) hashHex += "0";
            hashHex += String(hash[i], HEX);
        }
        clen = 96;
        if (!encryptData((const uint8_t*)hashHex.c_str(), hashHex.length(),
                         key, out.iv, out.encrypted_admin_hash, &clen)) return false;
    } else {
        memcpy(out.encrypted_admin_hash, existing->encrypted_admin_hash, 96);
    }

    // VPS fields — preserve existing (not exposed in provisioning)
    memcpy(out.encrypted_vps_token, existing->encrypted_vps_token, 160);
    memcpy(out.encrypted_vps_url,   existing->encrypted_vps_url,   128);

    size_t checksum_offset = offsetof(FRAMCredentials, checksum);
    out.checksum = calculateChecksum((uint8_t*)&out, checksum_offset);
    return true;
}

// ── GET /api/prov/config — non-sensitive current values for form pre-fill ─────
void handleProvConfig(AsyncWebServerRequest *request) {
    FRAMCredentials existingFram;
    bool hasExisting = readCredentialsFromFRAM(existingFram);

    JsonDocument doc;
    doc["device_name"]   = hasExisting ? String(existingFram.device_name) : "";
    doc["is_configured"] = hasExisting;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ── POST /api/configure ───────────────────────────────────────────────────────
void handleConfigureSubmit(AsyncWebServerRequest *request, JsonVariant &json) {
    LOG_INFO("Configuration submission from: %s", request->client()->remoteIP().toString().c_str());

    JsonObject obj = json.as<JsonObject>();

    String deviceName    = obj["device_name"]    | "";
    String wifiSSID      = obj["wifi_ssid"]      | "";
    String wifiPassword  = obj["wifi_password"]  | "";
    String adminPassword = obj["admin_password"] | "";

    // Helper: send error response
    auto sendError = [&](int code, const String& msg, const String& field = "") {
        JsonDocument doc;
        doc["success"] = false;
        doc["error"]   = msg;
        if (field.length()) doc["field"] = field;
        String body; serializeJson(doc, body);
        request->send(code, "application/json", body);
    };

    // --- Validate credentials ------------------------------------------------
    ValidationResult v = prov_validateAllCredentials(deviceName, wifiSSID, wifiPassword, adminPassword);
    if (!v.valid) { sendError(400, v.errorMessage, v.errorField); return; }

    // --- Read existing FRAM --------------------------------------------------
    FRAMCredentials existingFram;
    bool hasExisting = readCredentialsFromFRAM(existingFram);

    bool anyPassBlank = (wifiPassword.length() == 0 || adminPassword.length() == 0);

    if (anyPassBlank && !hasExisting) {
        sendError(400, "First-time setup: both passwords are required", "wifi_password"); return;
    }

    if (anyPassBlank && String(existingFram.device_name) != deviceName) {
        sendError(400, "Both passwords are required when changing device name", "wifi_password"); return;
    }

    // --- Build and write FRAMCredentials -------------------------------------
    FRAMCredentials newFram;
    if (!buildFRAMCredentials(deviceName, wifiSSID, wifiPassword, adminPassword,
                              hasExisting ? &existingFram : nullptr, newFram)) {
        sendError(500, "Failed to encrypt credentials"); return;
    }

    if (!writeCredentialsToFRAM(newFram)) {
        sendError(500, "Failed to save credentials to FRAM"); return;
    }

    if (!verifyCredentialsInFRAM()) {
        sendError(500, "FRAM verification failed"); return;
    }

    LOG_INFO("Credentials saved OK for device: %s", deviceName.c_str());

    // --- Success -------------------------------------------------------------
    JsonDocument respDoc;
    respDoc["success"]     = true;
    respDoc["message"]     = "Configuration saved. Please restart the device.";
    respDoc["device_name"] = deviceName;
    String body; serializeJson(respDoc, body);
    request->send(200, "application/json", body);
}
