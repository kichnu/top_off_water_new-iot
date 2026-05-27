#ifndef SHA256_H
#define SHA256_H

#include <Arduino.h>
#include <cstring>

#define SHA256_HASH_SIZE 32

class SHA256 {
private:
    uint32_t m_state[8];
    uint32_t m_data[16];
    size_t m_blocklen;
    uint64_t m_bitlen;
    
    void transform();
    uint32_t rotr(uint32_t x, uint32_t n);
    uint32_t choose(uint32_t e, uint32_t f, uint32_t g);
    uint32_t majority(uint32_t a, uint32_t b, uint32_t c);
    uint32_t sig0(uint32_t x);
    uint32_t sig1(uint32_t x);
    
public:
    SHA256();
    void init();
    void update(const uint8_t data[], size_t len);
    void final(uint8_t hash[]);
};

// Convenience functions
void sha256_hash(const uint8_t* data, size_t len, uint8_t hash[32]);
void sha256_hash(const String& str, uint8_t hash[32]);

#endif