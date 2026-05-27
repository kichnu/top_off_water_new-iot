#ifndef AES_H
#define AES_H

#include <Arduino.h>
#include <cstring>

#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16

class AES256 {
private:
    uint8_t round_keys[240]; // 15 round keys * 16 bytes each
    
    uint8_t sbox(uint8_t byte);
    uint8_t inv_sbox(uint8_t byte);
    uint8_t gf_multiply(uint8_t a, uint8_t b);
    void key_expansion(const uint8_t* key);
    void add_round_key(uint8_t* state, const uint8_t* round_key);
    void sub_bytes(uint8_t* state);
    void inv_sub_bytes(uint8_t* state);
    void shift_rows(uint8_t* state);
    void inv_shift_rows(uint8_t* state);
    void mix_columns(uint8_t* state);
    void inv_mix_columns(uint8_t* state);
    
public:
    AES256();
    void set_key(const uint8_t* key);
    void encrypt_block(const uint8_t* plaintext, uint8_t* ciphertext);
    void decrypt_block(const uint8_t* ciphertext, uint8_t* plaintext);
};

class AES256_CBC {
private:
    AES256 aes;
    uint8_t iv[AES_BLOCK_SIZE];
    
public:
    AES256_CBC();
    void set_key(const uint8_t* key);
    void set_iv(const uint8_t* new_iv);
    bool encrypt(const uint8_t* plaintext, size_t plaintext_len, uint8_t* ciphertext);
    bool decrypt(const uint8_t* ciphertext, size_t ciphertext_len, uint8_t* plaintext);
};

#endif