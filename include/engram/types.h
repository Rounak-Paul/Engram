#ifndef ENGRAM_TYPES_H
#define ENGRAM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ENGRAM_VECTOR_DIM 256
#define ENGRAM_MAX_ACTIVATIONS 64
#define ENGRAM_MAX_CONTENT_LEN 4096

typedef uint64_t engram_id_t;
typedef float engram_vec_t[ENGRAM_VECTOR_DIM];

typedef struct engram engram_t;

typedef struct {
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
    float *activations;
    const char **content;
    size_t count;
} engram_response_t;

typedef enum {
    ENGRAM_OK = 0,
    ENGRAM_ERR_MEMORY,
    ENGRAM_ERR_INVALID,
    ENGRAM_ERR_NOT_FOUND,
    ENGRAM_ERR_GPU,
    ENGRAM_ERR_IO
} engram_status_t;

typedef enum {
    ENGRAM_DEVICE_CPU,
    ENGRAM_DEVICE_VULKAN
} engram_device_t;

typedef struct {
    size_t neuron_count;
    size_t neuron_capacity;
    size_t synapse_count;
    size_t synapse_capacity;
    size_t content_count;
    size_t memory_neurons;
    size_t memory_synapses;
    size_t memory_content;
    size_t memory_total;
    engram_device_t device;
} engram_stats_t;

#endif
