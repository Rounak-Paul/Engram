#include "internal.h"
#include <string.h>
#include <math.h>

#define FNV_OFFSET 2166136261u
#define FNV_PRIME 16777619u

uint32_t engram_hash(const void *data, size_t len) {
    const uint8_t *bytes = data;
    uint32_t hash = FNV_OFFSET;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

static uint32_t hash_with_seed(const void *data, size_t len, uint32_t seed) {
    const uint8_t *bytes = data;
    uint32_t hash = seed;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    return hash;
}

float engram_pattern_similarity(const engram_pattern_t *a, const engram_pattern_t *b) {
    float dot = 0.0f;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
        dot += a->values[i] * b->values[i];
    }
    float mag = a->magnitude * b->magnitude;
    if (mag < 1e-8f) return 0.0f;
    return dot / mag;
}

void engram_pattern_add(engram_pattern_t *dst, const engram_pattern_t *src, float scale) {
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
        dst->values[i] += src->values[i] * scale;
    }
}

void engram_pattern_normalize(engram_pattern_t *p) {
    float sum = 0.0f;
    float max_val = 0.0f;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
        sum += p->values[i] * p->values[i];
        if (p->values[i] > max_val) max_val = p->values[i];
    }
    p->magnitude = sqrtf(sum);
    float inv = 1.0f;
    if (p->magnitude > 1e-8f) {
        inv = 1.0f / p->magnitude;
        for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
            p->values[i] *= inv;
        }
        p->magnitude = 1.0f;
    }
    
    float threshold = max_val * inv * 0.2f;
    p->active_count = 0;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM && p->active_count < 64; i++) {
        if (p->values[i] > threshold) {
            p->active_indices[p->active_count++] = i;
        }
    }
}

void engram_pattern_sparse_encode(const float *dense, engram_pattern_t *out) {
    memcpy(out->values, dense, ENGRAM_PATTERN_DIM * sizeof(float));
    
    float threshold = 0.0f;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
        if (out->values[i] > threshold) threshold = out->values[i];
    }
    threshold *= 0.3f;
    
    out->active_count = 0;
    float sum = 0.0f;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM && out->active_count < 64; i++) {
        if (out->values[i] > threshold) {
            out->active_indices[out->active_count++] = i;
        }
        sum += out->values[i] * out->values[i];
    }
    out->magnitude = sqrtf(sum);
}

void engram_pattern_from_text(const char *text, size_t len, engram_pattern_t *out) {
    memset(out, 0, sizeof(*out));
    
    for (size_t i = 0; i < len; i++) {
        uint32_t h1 = hash_with_seed(text + i, 1, 0x12345678);
        uint32_t h2 = hash_with_seed(text + i, 1, 0x87654321);
        
        uint32_t idx1 = h1 % ENGRAM_PATTERN_DIM;
        uint32_t idx2 = h2 % ENGRAM_PATTERN_DIM;
        
        out->values[idx1] += 1.0f;
        out->values[idx2] += 0.5f;
        
        if (i + 1 < len) {
            uint32_t h3 = hash_with_seed(text + i, 2, 0xABCDEF01);
            uint32_t idx3 = h3 % ENGRAM_PATTERN_DIM;
            out->values[idx3] += 0.75f;
        }
        
        if (i + 2 < len) {
            uint32_t h4 = hash_with_seed(text + i, 3, 0xFEDCBA98);
            uint32_t idx4 = h4 % ENGRAM_PATTERN_DIM;
            out->values[idx4] += 0.5f;
        }
    }
    
    engram_pattern_normalize(out);
    
    float threshold = 0.0f;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
        if (out->values[i] > threshold) threshold = out->values[i];
    }
    threshold *= 0.2f;
    
    out->active_count = 0;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM && out->active_count < 64; i++) {
        if (out->values[i] > threshold) {
            out->active_indices[out->active_count++] = i;
        }
    }
}
