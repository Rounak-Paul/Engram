#ifndef ENGRAM_TYPES_H
#define ENGRAM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ENGRAM_VECTOR_DIM 256
#define ENGRAM_MAX_ACTIVATIONS 64

typedef uint64_t engram_id_t;
typedef float engram_vec_t[ENGRAM_VECTOR_DIM];

typedef struct engram engram_t;

typedef struct {
    bool enable_gpu;
    size_t max_neurons;
    size_t max_synapses;
    float decay_rate;
    float activation_threshold;
    float learning_rate;
    float noise_threshold;
} engram_config_t;

typedef struct {
    engram_id_t *ids;
    float *relevance;
    size_t count;
} engram_response_t;

typedef enum {
    ENGRAM_OK = 0,
    ENGRAM_ERR_MEMORY,
    ENGRAM_ERR_INVALID,
    ENGRAM_ERR_NOT_FOUND,
    ENGRAM_ERR_GPU
} engram_status_t;

#endif
