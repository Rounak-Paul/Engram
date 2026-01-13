#include "internal.h"
#include <math.h>
#include <string.h>

void vec_zero(float *v, size_t dim) {
    memset(v, 0, dim * sizeof(float));
}

void vec_add(float *dst, const float *a, const float *b, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        dst[i] = a[i] + b[i];
    }
}

void vec_scale(float *v, float s, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        v[i] *= s;
    }
}

float vec_dot(const float *a, const float *b, size_t dim) {
    float sum = 0.0f;
    for (size_t i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

float vec_magnitude(const float *v, size_t dim) {
    return sqrtf(vec_dot(v, v, dim));
}

void vec_normalize(float *v, size_t dim) {
    float mag = vec_magnitude(v, dim);
    if (mag > 1e-8f) {
        float inv = 1.0f / mag;
        vec_scale(v, inv, dim);
    }
}

void vec_lerp(float *dst, const float *a, const float *b, float t, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        dst[i] = a[i] + t * (b[i] - a[i]);
    }
}
