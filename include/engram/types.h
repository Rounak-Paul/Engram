#ifndef ENGRAM_TYPES_H
#define ENGRAM_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef struct engram engram_t;

typedef struct engram_neuron {
    uint32_t id;
    float activation;
    float threshold;
    float fatigue;
    uint32_t last_fired_tick;
    uint32_t fire_count;
    uint16_t cluster_id;
    uint8_t refractory_remaining;
    uint8_t flags;
} engram_neuron_t;

#define ENGRAM_NEURON_FLAG_NONE       0x00
#define ENGRAM_NEURON_FLAG_INHIBITORY 0x01
#define ENGRAM_NEURON_FLAG_ACTIVE     0x02
#define ENGRAM_NEURON_FLAG_REFRACTORY 0x04

typedef struct engram_synapse {
    uint32_t pre_neuron_id;
    uint32_t post_neuron_id;
    float weight;
    float eligibility_trace;
    uint32_t last_active_tick;
    uint16_t potentiation_count;
    uint16_t flags;
} engram_synapse_t;

#define ENGRAM_SYNAPSE_FLAG_NONE      0x0000
#define ENGRAM_SYNAPSE_FLAG_LOCKED    0x0001
#define ENGRAM_SYNAPSE_FLAG_PENDING   0x0002

typedef struct engram_cluster {
    uint32_t id;
    uint32_t neuron_start;
    uint32_t neuron_count;
    uint32_t synapse_start;
    uint32_t synapse_count;
    float avg_activation;
    uint32_t last_active_tick;
} engram_cluster_t;

typedef struct engram_pathway {
    uint32_t id;
    uint32_t *neuron_ids;
    uint32_t neuron_count;
    float strength;
    uint32_t activation_count;
    uint32_t last_active_tick;
    uint32_t created_tick;
    uint64_t content_hash;
    void *original_data;
    size_t original_size;
    uint32_t modality;
} engram_pathway_t;

typedef enum {
    ENGRAM_MODALITY_RAW = 0,
    ENGRAM_MODALITY_TEXT,
    ENGRAM_MODALITY_NUMERIC,
    ENGRAM_MODALITY_BINARY,
    ENGRAM_MODALITY_COUNT
} engram_modality_t;

typedef struct engram_cue {
    const void *data;
    size_t size;
    float intensity;
    float salience;
    uint32_t modality;
    uint32_t flags;
} engram_cue_t;

#define ENGRAM_CUE_FLAG_NONE    0x00000000
#define ENGRAM_CUE_FLAG_RECALL  0x00000001
#define ENGRAM_CUE_FLAG_LEARN   0x00000002
#define ENGRAM_CUE_FLAG_URGENT  0x00000004

typedef struct engram_recall {
    void *data;
    size_t data_size;
    float *pattern;
    size_t pattern_size;
    float confidence;
    float familiarity;
    uint32_t pathway_id;
    uint32_t age_ticks;
    uint32_t modality;
} engram_recall_t;

typedef enum {
    ENGRAM_AROUSAL_WAKE = 0,
    ENGRAM_AROUSAL_DROWSY,
    ENGRAM_AROUSAL_SLEEP,
    ENGRAM_AROUSAL_REM
} engram_arousal_t;

typedef struct engram_stats {
    uint64_t tick_count;
    uint32_t neuron_count;
    uint32_t active_neuron_count;
    uint32_t synapse_count;
    uint32_t cluster_count;
    uint32_t pathway_count;
    float avg_activation;
    float memory_usage_bytes;
    float cpu_usage_percent;
    float current_tick_rate;
    engram_arousal_t arousal_state;
} engram_stats_t;

typedef struct engram_resource_usage {
    float cpu_percent;
    size_t memory_bytes;
    uint32_t ticks_per_second;
    uint64_t total_ticks;
} engram_resource_usage_t;

#endif
