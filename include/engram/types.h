#ifndef ENGRAM_TYPES_H
#define ENGRAM_TYPES_H

#include <stdint.h>
#include <stdatomic.h>

#define ENGRAM_MAX_SYNAPSES_PER_NEURON 64
#define ENGRAM_CLUSTER_SIZE 256
#define ENGRAM_PATTERN_DIM 512
#define ENGRAM_MAX_PATHWAYS 65536
#define ENGRAM_TOKEN_MAX_LEN 64
#define ENGRAM_VOCAB_SIZE 32768

typedef struct engram_neuron {
    uint32_t id;
    _Atomic float activation;
    float threshold;
    float fatigue;
    uint32_t last_fired_tick;
    uint32_t fire_count;
    uint16_t cluster_id;
    uint8_t refractory;
    uint8_t flags;
} engram_neuron_t;

typedef struct engram_synapse {
    uint32_t pre;
    uint32_t post;
    _Atomic float weight;
    float eligibility;
    uint32_t last_active;
    uint16_t potentiation;
    uint16_t flags;
} engram_synapse_t;

typedef struct engram_pathway {
    uint32_t id;
    uint32_t *neuron_ids;
    uint32_t neuron_count;
    float strength;
    uint32_t access_count;
    uint32_t last_access;
    uint32_t created_tick;
} engram_pathway_t;

typedef struct engram_pattern {
    float values[ENGRAM_PATTERN_DIM];
    uint32_t active_indices[64];
    uint32_t active_count;
    float magnitude;
} engram_pattern_t;

typedef struct engram_token {
    char text[ENGRAM_TOKEN_MAX_LEN];
    uint16_t len;
    uint16_t id;
    engram_pattern_t pattern;
} engram_token_t;

typedef struct engram_cluster {
    uint32_t id;
    engram_neuron_t *neurons;
    uint32_t neuron_count;
    engram_synapse_t *synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;
    float lateral_inhibition;
} engram_cluster_t;

#endif
