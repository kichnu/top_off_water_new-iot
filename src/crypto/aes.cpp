#include "aes.h"

// AES S-box
static const uint8_t sbox_table[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5, 0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0, 0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0, 0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B, 0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17, 0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88, 0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6, 0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E, 0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};

// AES Inverse S-box
static const uint8_t inv_sbox_table[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89, 0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

// Round constants
static const uint8_t round_constants[15] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36, 0x6C, 0xD8, 0xAB, 0x4D
};

AES256::AES256() {
    memset(round_keys, 0, sizeof(round_keys));
}

void AES256::set_key(const uint8_t* key) {
    key_expansion(key);
}

uint8_t AES256::sbox(uint8_t byte) {
    return sbox_table[byte];
}

uint8_t AES256::inv_sbox(uint8_t byte) {
    return inv_sbox_table[byte];
}

uint8_t AES256::gf_multiply(uint8_t a, uint8_t b) {
    uint8_t result = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) {
            result ^= a;
        }
        bool high_bit = a & 0x80;
        a <<= 1;
        if (high_bit) {
            a ^= 0x1B; // AES irreducible polynomial
        }
        b >>= 1;
    }
    return result;
}

void AES256::key_expansion(const uint8_t* key) {
    // Copy original key
    memcpy(round_keys, key, 32);
    
    // Generate round keys
    for (int i = 32; i < 240; i += 4) {
        uint8_t temp[4];
        
        // Copy previous 4 bytes
        memcpy(temp, &round_keys[i-4], 4);
        
        // Every 32 bytes (8th key), apply RotWord and SubWord
        if (i % 32 == 0) {
            // RotWord
            uint8_t t = temp[0];
            temp[0] = temp[1];
            temp[1] = temp[2];
            temp[2] = temp[3];
            temp[3] = t;
            
            // SubWord
            for (int j = 0; j < 4; j++) {
                temp[j] = sbox(temp[j]);
            }
            
            // XOR with round constant
            temp[0] ^= round_constants[i/32];
        }
        // Every 16 bytes after 24, apply SubWord only
        else if (i % 32 == 16) {
            for (int j = 0; j < 4; j++) {
                temp[j] = sbox(temp[j]);
            }
        }
        
        // XOR with key 32 bytes ago
        for (int j = 0; j < 4; j++) {
            round_keys[i + j] = round_keys[i - 32 + j] ^ temp[j];
        }
    }
}

void AES256::add_round_key(uint8_t* state, const uint8_t* round_key) {
    for (int i = 0; i < 16; i++) {
        state[i] ^= round_key[i];
    }
}

void AES256::sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = sbox(state[i]);
    }
}

void AES256::inv_sub_bytes(uint8_t* state) {
    for (int i = 0; i < 16; i++) {
        state[i] = inv_sbox(state[i]);
    }
}

void AES256::shift_rows(uint8_t* state) {
    uint8_t temp;
    
    // Row 1: shift left by 1
    temp = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temp;
    
    // Row 2: shift left by 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    // Row 3: shift left by 3
    temp = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temp;
}

void AES256::inv_shift_rows(uint8_t* state) {
    uint8_t temp;
    
    // Row 1: shift right by 1
    temp = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temp;
    
    // Row 2: shift right by 2
    temp = state[2];
    state[2] = state[10];
    state[10] = temp;
    temp = state[6];
    state[6] = state[14];
    state[14] = temp;
    
    // Row 3: shift right by 3
    temp = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = state[3];
    state[3] = temp;
}

void AES256::mix_columns(uint8_t* state) {
    for (int col = 0; col < 4; col++) {
        uint8_t s0 = state[col * 4];
        uint8_t s1 = state[col * 4 + 1];
        uint8_t s2 = state[col * 4 + 2];
        uint8_t s3 = state[col * 4 + 3];
        
        state[col * 4] = gf_multiply(s0, 2) ^ gf_multiply(s1, 3) ^ s2 ^ s3;
        state[col * 4 + 1] = s0 ^ gf_multiply(s1, 2) ^ gf_multiply(s2, 3) ^ s3;
        state[col * 4 + 2] = s0 ^ s1 ^ gf_multiply(s2, 2) ^ gf_multiply(s3, 3);
        state[col * 4 + 3] = gf_multiply(s0, 3) ^ s1 ^ s2 ^ gf_multiply(s3, 2);
    }
}

void AES256::inv_mix_columns(uint8_t* state) {
    for (int col = 0; col < 4; col++) {
        uint8_t s0 = state[col * 4];
        uint8_t s1 = state[col * 4 + 1];
        uint8_t s2 = state[col * 4 + 2];
        uint8_t s3 = state[col * 4 + 3];
        
        state[col * 4] = gf_multiply(s0, 0x0E) ^ gf_multiply(s1, 0x0B) ^ 
                         gf_multiply(s2, 0x0D) ^ gf_multiply(s3, 0x09);
        state[col * 4 + 1] = gf_multiply(s0, 0x09) ^ gf_multiply(s1, 0x0E) ^ 
                             gf_multiply(s2, 0x0B) ^ gf_multiply(s3, 0x0D);
        state[col * 4 + 2] = gf_multiply(s0, 0x0D) ^ gf_multiply(s1, 0x09) ^ 
                             gf_multiply(s2, 0x0E) ^ gf_multiply(s3, 0x0B);
        state[col * 4 + 3] = gf_multiply(s0, 0x0B) ^ gf_multiply(s1, 0x0D) ^ 
                             gf_multiply(s2, 0x09) ^ gf_multiply(s3, 0x0E);
    }
}

void AES256::encrypt_block(const uint8_t* plaintext, uint8_t* ciphertext) {
    // Copy plaintext to state
    memcpy(ciphertext, plaintext, 16);
    
    // Initial round
    add_round_key(ciphertext, round_keys);
    
    // Main rounds (1-13)
    for (int round = 1; round < 14; round++) {
        sub_bytes(ciphertext);
        shift_rows(ciphertext);
        mix_columns(ciphertext);
        add_round_key(ciphertext, &round_keys[round * 16]);
    }
    
    // Final round (14)
    sub_bytes(ciphertext);
    shift_rows(ciphertext);
    add_round_key(ciphertext, &round_keys[14 * 16]);
}

void AES256::decrypt_block(const uint8_t* ciphertext, uint8_t* plaintext) {
    // Copy ciphertext to state
    memcpy(plaintext, ciphertext, 16);
    
    // Initial round
    add_round_key(plaintext, &round_keys[14 * 16]);
    
    // Main rounds (13-1)
    for (int round = 13; round > 0; round--) {
        inv_shift_rows(plaintext);
        inv_sub_bytes(plaintext);
        add_round_key(plaintext, &round_keys[round * 16]);
        inv_mix_columns(plaintext);
    }
    
    // Final round (0)
    inv_shift_rows(plaintext);
    inv_sub_bytes(plaintext);
    add_round_key(plaintext, round_keys);
}

// AES256_CBC implementation
AES256_CBC::AES256_CBC() {
    memset(iv, 0, AES_BLOCK_SIZE);
}

void AES256_CBC::set_key(const uint8_t* key) {
    aes.set_key(key);
}

void AES256_CBC::set_iv(const uint8_t* new_iv) {
    memcpy(iv, new_iv, AES_BLOCK_SIZE);
}

bool AES256_CBC::encrypt(const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext) {
    if (plaintext_len % AES_BLOCK_SIZE != 0) {
        return false; // Must be padded to block size
    }
    
    uint8_t current_iv[AES_BLOCK_SIZE];
    memcpy(current_iv, iv, AES_BLOCK_SIZE);
    
    for (size_t i = 0; i < plaintext_len; i += AES_BLOCK_SIZE) {
        uint8_t block[AES_BLOCK_SIZE];
        memcpy(block, &plaintext[i], AES_BLOCK_SIZE);
        
        // XOR with IV (or previous ciphertext block)
        for (int j = 0; j < AES_BLOCK_SIZE; j++) {
            block[j] ^= current_iv[j];
        }
        
        // Encrypt block
        aes.encrypt_block(block, &ciphertext[i]);
        
        // Update IV for next block
        memcpy(current_iv, &ciphertext[i], AES_BLOCK_SIZE);
    }
    
    return true;
}

bool AES256_CBC::decrypt(const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext) {
    if (ciphertext_len % AES_BLOCK_SIZE != 0) {
        return false; // Must be multiple of block size
    }
    
    uint8_t current_iv[AES_BLOCK_SIZE];
    uint8_t next_iv[AES_BLOCK_SIZE];
    memcpy(current_iv, iv, AES_BLOCK_SIZE);
    
    for (size_t i = 0; i < ciphertext_len; i += AES_BLOCK_SIZE) {
        // Save current ciphertext block as next IV
        memcpy(next_iv, &ciphertext[i], AES_BLOCK_SIZE);
        
        // Decrypt block
        aes.decrypt_block(&ciphertext[i], &plaintext[i]);
        
        // XOR with IV (or previous ciphertext block)
        for (int j = 0; j < AES_BLOCK_SIZE; j++) {
            plaintext[i + j] ^= current_iv[j];
        }
        
        // Update IV for next block
        memcpy(current_iv, next_iv, AES_BLOCK_SIZE);
    }
    
    return true;
}