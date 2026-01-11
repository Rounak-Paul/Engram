#include "internal.h"
#include <stdatomic.h>

static _Atomic uint64_t rng_state = 0x853c49e6748fea9bULL;

void engram_random_seed(uint64_t seed) {
    atomic_store(&rng_state, seed);
}

static uint64_t xorshift64(void) {
    uint64_t x = atomic_load(&rng_state);
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    atomic_store(&rng_state, x);
    return x;
}

uint32_t engram_random_uint(void) {
    return (uint32_t)(xorshift64() >> 32);
}

float engram_random_float(void) {
    return (float)(xorshift64() >> 40) / (float)(1ULL << 24);
}
