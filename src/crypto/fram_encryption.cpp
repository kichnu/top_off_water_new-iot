#include "fram_encryption.h"
#include "../core/logging.h"
#include <cstring>
#include <new> 

// AES-256-CBC instance
AES256_CBC aes_cbc;

bool generateEncryptionKey(const String& device_name, uint8_t* key) {
    // Create key material: device_name + salt + seed
    String key_material = device_name + ENCRYPTION_SALT + ENCRYPTION_SEED;
    
    // Hash to create 256-bit key
    return sha256Hash(key_material, key);
}

bool generateRandomIV(uint8_t* iv) {
    // Generate random IV using built-in random functions
    randomSeed(millis() ^ analogRead(A0));
    
    for (int i = 0; i < 8; i++) {  // Note: using 8-byte IV
        iv[i] = random(0, 256);
    }
    
    // Add more entropy
    for (int i = 0; i < 8; i++) {
        iv[i] ^= (micros() & 0xFF);
        delay(1);
    }
    
    return true;
}

// ✅ FIX 1: Replace encryptData function with this safer version
bool encryptData(const uint8_t* plaintext, size_t plaintext_len,
                 const uint8_t* key, const uint8_t* iv,
                 uint8_t* ciphertext, size_t* ciphertext_len) {
    
    // ✅ Input validation
    if (!plaintext || !key || !iv || !ciphertext || !ciphertext_len) {
        LOG_ERROR("Invalid parameters for encryption");
        return false;
    }
    
    if (plaintext_len == 0) {
        LOG_ERROR("Empty plaintext");
        return false;
    }
    
    // Calculate padded length (must be multiple of 16)
    size_t padded_len = ((plaintext_len + 15) / 16) * 16;
    
    // IMPORTANT: If plaintext_len is already multiple of 16, add full block
    if (plaintext_len % AES_BLOCK_SIZE == 0) {
        padded_len += AES_BLOCK_SIZE;
    }
    
    if (padded_len > *ciphertext_len) {
        LOG_ERROR("Padded data larger than field size!");
        return false;
    }
    
    // ✅ FIX 1: Use std::nothrow to prevent exceptions
    uint8_t* padded_data = new(std::nothrow) uint8_t[*ciphertext_len];
    if (!padded_data) {
        LOG_ERROR("Failed to allocate %zu bytes for encryption buffer", *ciphertext_len);
        return false; // Memory allocation failed
    }
    
    // ✅ Initialize entire buffer with zeros
    memset(padded_data, 0, *ciphertext_len);
    
    // Copy data and add PKCS7 padding
    memcpy(padded_data, plaintext, plaintext_len);
    size_t actual_padded_len = addPKCS7Padding(padded_data, plaintext_len, AES_BLOCK_SIZE);
    
    // Set up AES-256-CBC
    aes_cbc.set_key(key);
    
    // Extend 8-byte IV to 16-byte IV for AES
    uint8_t full_iv[16];
    for (int i = 0; i < 16; i++) {
        full_iv[i] = iv[i % 8];  // Note: 8-byte IV extended
    }
    aes_cbc.set_iv(full_iv);
    
    // ✅ FIX 1: Encrypt with proper cleanup on failure
    bool success = aes_cbc.encrypt(padded_data, *ciphertext_len, ciphertext);
    
    // ✅ FIX 1: Secure cleanup - always clean sensitive data
    secureZeroMemory(padded_data, *ciphertext_len);
    delete[] padded_data;
    
    if (!success) {
        LOG_ERROR("AES encryption failed");
        return false;
    }
    
    // ciphertext_len stays the same (full field size)
    return true;
}

// ✅ FIX 1: Enhanced decryptData with better input validation
bool decryptData(const uint8_t* ciphertext, size_t ciphertext_len,
                 const uint8_t* key, const uint8_t* iv,
                 uint8_t* plaintext, size_t* plaintext_len) {
    
    // ✅ Input validation
    if (!ciphertext || !key || !iv || !plaintext || !plaintext_len) {
        LOG_ERROR("Invalid parameters for decryption");
        return false;
    }
    
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        LOG_ERROR("Ciphertext length not multiple of block size");
        return false;
    }
    
    if (ciphertext_len == 0) {
        LOG_ERROR("Empty ciphertext");
        return false;
    }
    
    // ✅ Check if output buffer is large enough
    if (*plaintext_len < ciphertext_len) {
        LOG_ERROR("Output buffer too small for decryption");
        return false;
    }
    
    // Set up AES-256-CBC
    aes_cbc.set_key(key);
    
    // Extend 8-byte IV to 16-byte IV for AES
    uint8_t full_iv[16];
    for (int i = 0; i < 16; i++) {
        full_iv[i] = iv[i % 8];  // Note: 8-byte IV extended
    }
    aes_cbc.set_iv(full_iv);
    
    // Decrypt data
    if (!aes_cbc.decrypt(ciphertext, ciphertext_len, plaintext)) {
        LOG_ERROR("AES decryption failed");
        // ✅ Clear sensitive data before returning error
        secureZeroMemory(plaintext, ciphertext_len);
        return false;
    }
    
    // Try to find valid PKCS7 padding by looking for non-zero data
    size_t actual_data_len = 0;
    
    // Try to remove padding from blocks
    for (size_t try_len = AES_BLOCK_SIZE; try_len <= ciphertext_len; try_len += AES_BLOCK_SIZE) {
        size_t unpadded_len = removePKCS7Padding(plaintext, try_len);
        if (unpadded_len > 0) {
            actual_data_len = unpadded_len;
            break;
        }
    }
     if (actual_data_len == 0) {
        LOG_ERROR("No valid PKCS7 padding found");
        // ✅ Clear sensitive data before returning error
        secureZeroMemory(plaintext, ciphertext_len);
        return false;
    }
    
    *plaintext_len = actual_data_len;
    return true;
}
// ✅ FIX 1: Add secure memory cleanup helper
void secureZeroMemory(void* ptr, size_t size) {
    if (ptr && size > 0) {
        volatile uint8_t* vptr = (volatile uint8_t*)ptr;
        for (size_t i = 0; i < size; i++) {
            vptr[i] = 0;
        }
    }
}
bool sha256Hash(const String& input, uint8_t* hash) {
    return sha256Hash((const uint8_t*)input.c_str(), input.length(), hash);
}

bool sha256Hash(const uint8_t* data, size_t len, uint8_t* hash) {
    sha256_hash(data, len, hash);
    return true;
}

size_t addPKCS7Padding(uint8_t* data, size_t data_len, size_t block_size) {
    size_t padding_bytes = block_size - (data_len % block_size);
    
    // IMPORTANT: If data_len is already a multiple of block_size,
    // we still need to add a full block of padding (PKCS7 requirement)
    if (padding_bytes == 0) {
        padding_bytes = block_size;
    }
    
    for (size_t i = 0; i < padding_bytes; i++) {
        data[data_len + i] = (uint8_t)padding_bytes;
    }
    
    return data_len + padding_bytes;
}

size_t removePKCS7Padding(const uint8_t* data, size_t data_len) {
    if (data_len == 0) return 0;
    
    uint8_t padding_bytes = data[data_len - 1];
    
    // Validate padding
    if (padding_bytes == 0 || padding_bytes > AES_BLOCK_SIZE) {
        return 0; // Invalid padding
    }
    
    if (data_len < padding_bytes) {
        return 0; // Invalid padding
    }
    
    // Check all padding bytes
    for (size_t i = data_len - padding_bytes; i < data_len; i++) {
        if (data[i] != padding_bytes) {
            return 0; // Invalid padding
        }
    }
    
    return data_len - padding_bytes;
}

bool encryptCredentials(const DeviceCredentials& creds, FRAMCredentials& fram_creds) {
    LOG_INFO("Encrypting credentials...");

    // Clear the structure
    memset(&fram_creds, 0, sizeof(FRAMCredentials));

    // Set magic and version
    fram_creds.magic = FRAM_MAGIC_NUMBER;
    fram_creds.version = 0x0003;
    
    // Copy device name (plain text)
    strncpy(fram_creds.device_name, creds.device_name.c_str(), 31);
    fram_creds.device_name[31] = '\0';
    
    // Generate encryption key from device name
    uint8_t encryption_key[AES_KEY_SIZE];
    if (!generateEncryptionKey(creds.device_name, encryption_key)) {
        LOG_ERROR("Failed to generate encryption key");
        return false;
    }
    
    // Generate random IV
    if (!generateRandomIV(fram_creds.iv)) {
        LOG_ERROR("Failed to generate IV");
        return false;
    }
    
    // Hash admin password
    String admin_password = creds.admin_password;
    uint8_t admin_hash[SHA256_HASH_SIZE];
    if (!sha256Hash(admin_password, admin_hash)) {
        LOG_ERROR("Failed to hash admin password");
        return false;
    }
    
    // Convert hash to hex string
    String admin_hash_hex = "";
    for (int i = 0; i < SHA256_HASH_SIZE; i++) {
        if (admin_hash[i] < 16) admin_hash_hex += "0";
        admin_hash_hex += String(admin_hash[i], HEX);
    }
    
    // Encrypt WiFi SSID
    size_t ciphertext_len = 64;
    if (!encryptData((const uint8_t*)creds.wifi_ssid.c_str(), creds.wifi_ssid.length(),
                     encryption_key, fram_creds.iv,
                     fram_creds.encrypted_wifi_ssid, &ciphertext_len)) {
        LOG_ERROR("Failed to encrypt WiFi SSID");
        return false;
    }
    
    // Encrypt WiFi password
    ciphertext_len = 128;
    if (!encryptData((const uint8_t*)creds.wifi_password.c_str(), creds.wifi_password.length(),
                     encryption_key, fram_creds.iv,
                     fram_creds.encrypted_wifi_password, &ciphertext_len)) {
        LOG_ERROR("Failed to encrypt WiFi password");
        return false;
    }
    
    // Encrypt admin hash
    ciphertext_len = 96;
    if (!encryptData((const uint8_t*)admin_hash_hex.c_str(), admin_hash_hex.length(),
                     encryption_key, fram_creds.iv,
                     fram_creds.encrypted_admin_hash, &ciphertext_len)) {
        LOG_ERROR("Failed to encrypt admin hash");
        return false;
    }
    
    // VPS token and URL fields are unused — leave zeroed (set by memset above)

    // Calculate checksum (only bytes before checksum field)
    size_t checksum_offset = offsetof(FRAMCredentials, checksum);
    uint16_t temp_checksum = calculateChecksum((uint8_t*)&fram_creds, checksum_offset);
    fram_creds.checksum = temp_checksum;

    LOG_INFO("SUCCESS: Credentials encrypted");
    return true;
}
     
bool decryptCredentials(const FRAMCredentials& fram_creds, DeviceCredentials& creds) {
    LOG_INFO("Decrypting credentials...");
    
    // Generate encryption key from device name
    uint8_t encryption_key[AES_KEY_SIZE];
    String device_name = String(fram_creds.device_name);
    
    if (!generateEncryptionKey(device_name, encryption_key)) {
        LOG_ERROR("Failed to generate encryption key");
        return false;
    }
    
    // Set device name
    creds.device_name = device_name;
    
    uint8_t plaintext_buffer[256];
    size_t plaintext_len;
    
    // Decrypt WiFi SSID
    plaintext_len = sizeof(plaintext_buffer);
    if (!decryptData(fram_creds.encrypted_wifi_ssid, 64,
                     encryption_key, fram_creds.iv,
                     plaintext_buffer, &plaintext_len)) {
        LOG_ERROR("Failed to decrypt WiFi SSID");
        return false;
    }
    plaintext_buffer[plaintext_len] = '\0';
    creds.wifi_ssid = String((char*)plaintext_buffer);
    
    // Decrypt WiFi password
    plaintext_len = sizeof(plaintext_buffer);
    if (!decryptData(fram_creds.encrypted_wifi_password, 128,
                     encryption_key, fram_creds.iv,
                     plaintext_buffer, &plaintext_len)) {
        LOG_ERROR("Failed to decrypt WiFi password");
        return false;
    }
    plaintext_buffer[plaintext_len] = '\0';
    creds.wifi_password = String((char*)plaintext_buffer);
    
    // Decrypt admin hash
    plaintext_len = sizeof(plaintext_buffer);
    if (!decryptData(fram_creds.encrypted_admin_hash, 96,
                     encryption_key, fram_creds.iv,
                     plaintext_buffer, &plaintext_len)) {
        LOG_ERROR("Failed to decrypt admin hash");
        creds.admin_password = "";
    } else {
        plaintext_buffer[plaintext_len] = '\0';
        creds.admin_password = String((char*)plaintext_buffer);
    }
    
    // VPS token and URL fields are unused — skip silently
    creds.vps_token = "";
    creds.vps_url = "";

    LOG_INFO("SUCCESS: Credentials decrypted");
    return true;
}

bool validateDeviceName(const String& name) {
    if (name.length() == 0 || name.length() > MAX_DEVICE_NAME_LEN) {
        return false;
    }
    
    // Check for valid characters (alphanumeric and underscore)
    for (int i = 0; i < name.length(); i++) {
        char c = name.charAt(i);
        if (!isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

bool validateWiFiSSID(const String& ssid) {
    return (ssid.length() > 0 && ssid.length() <= MAX_WIFI_SSID_LEN);
}

bool validateWiFiPassword(const String& password) {
    return (password.length() > 0 && password.length() <= MAX_WIFI_PASSWORD_LEN);
}

uint16_t calculateChecksum(const uint8_t* data, size_t size) {
    uint16_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum;
}


