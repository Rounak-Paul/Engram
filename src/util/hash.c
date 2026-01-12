#include "internal.h"

static uint64_t rng_state = 0x853c49e6748fea9bULL;

uint64_t hash_bytes(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t h = ENGRAM_HASH_SEED;
    for (size_t i = 0; i < len; i++) {
        h ^= bytes[i];
        h *= 0x5851f42d4c957f2dULL;
        h ^= h >> 33;
    }
    return h;
}

uint32_t hash_string(const char *str, size_t len) {
    uint32_t h = 0x811c9dc5;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)str[i];
        h *= 0x01000193;
    }
    return h;
}

static uint64_t xorshift64(void) {
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

uint64_t rand64(void) {
    return xorshift64();
}

float randf(void) {
    return (float)(xorshift64() & 0xFFFFFFFF) / (float)0xFFFFFFFF;
}
