#include "internal.h"
#include <math.h>

void vec_zero(engram_vec_t v) {
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        v[i] = 0.0f;
    }
}

void vec_add(engram_vec_t dst, const engram_vec_t a, const engram_vec_t b) {
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        dst[i] = a[i] + b[i];
    }
}

void vec_scale(engram_vec_t v, float s) {
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        v[i] *= s;
    }
}

float vec_dot(const engram_vec_t a, const engram_vec_t b) {
    float sum = 0.0f;
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

float vec_magnitude(const engram_vec_t v) {
    return sqrtf(vec_dot(v, v));
}

void vec_normalize(engram_vec_t v) {
    float mag = vec_magnitude(v);
    if (mag > 1e-8f) {
        float inv = 1.0f / mag;
        vec_scale(v, inv);
    }
}

void vec_lerp(engram_vec_t dst, const engram_vec_t a, const engram_vec_t b, float t) {
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        dst[i] = a[i] + t * (b[i] - a[i]);
    }
}
