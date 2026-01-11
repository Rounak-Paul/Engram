#ifndef ENGRAM_INTERNAL_H
#define ENGRAM_INTERNAL_H

#include "engram/engram.h"
#include "platform.h"
#include <stdatomic.h>
#include <stdbool.h>

#define ENGRAM_MAX_CLUSTERS 4096
#define ENGRAM_MAX_PATHWAYS 65536
#define ENGRAM_SYNAPSE_POOL_BLOCK 4096
#define ENGRAM_WORKING_MEMORY_DEFAULT 64

typedef struct synapse_pool {
    engram_synapse_t *synapses;
    uint32_t capacity;
    uint32_t count;
    uint32_t *free_list;
    uint32_t free_count;
} synapse_pool_t;

typedef struct neuron_pool {
    engram_neuron_t *neurons;
    uint32_t capacity;
    uint32_t count;
} neuron_pool_t;

typedef struct cluster_pool {
    engram_cluster_t *clusters;
    uint32_t capacity;
    uint32_t count;
} cluster_pool_t;

typedef struct pathway_pool {
    engram_pathway_t *pathways;
    uint32_t capacity;
    uint32_t count;
    uint32_t *free_list;
    uint32_t free_count;
} pathway_pool_t;

typedef struct working_memory {
    uint32_t *neuron_ids;
    float *activations;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
} working_memory_t;

typedef struct pending_cue {
    void *data;
    size_t size;
    float intensity;
    float salience;
    uint32_t modality;
    uint32_t flags;
} pending_cue_t;

typedef struct cue_queue {
    pending_cue_t *cues;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    engram_mutex_t mutex;
} cue_queue_t;

typedef struct memory_trace {
    uint64_t content_hash;
    uint32_t *neuron_ids;
    uint32_t neuron_count;
    void *original_data;
    size_t original_size;
    uint32_t modality;
    float strength;
    uint32_t created_tick;
    uint32_t replay_count;
} memory_trace_t;

typedef struct hippocampus {
    memory_trace_t *traces;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
} hippocampus_t;

typedef struct hash_bucket {
    uint64_t hash;
    uint32_t pathway_id;
} hash_bucket_t;

typedef struct memory_index {
    hash_bucket_t *buckets;
    uint32_t capacity;
    uint32_t count;
} memory_index_t;

typedef struct neocortex {
    uint32_t *consolidated_pathway_ids;
    uint32_t count;
    uint32_t capacity;
} neocortex_t;

typedef struct thalamus {
    float attention_threshold;
    float current_gate;
    uint32_t focus_cluster_id;
} thalamus_t;

typedef struct amygdala {
    float salience_threshold;
    float novelty_decay;
    uint64_t *seen_hashes;
    uint32_t seen_capacity;
    uint32_t seen_count;
} amygdala_t;

typedef struct brainstem {
    engram_thread_t thread;
    atomic_bool running;
    atomic_int arousal_state;
    atomic_int auto_arousal;
    uint64_t tick_count;
    uint64_t last_tick_time_ns;
    float current_tick_rate;
    uint32_t ticks_since_arousal_change;
} brainstem_t;

typedef struct governor {
    engram_resource_limits_t limits;
    float current_cpu_percent;
    size_t current_memory_bytes;
    uint32_t target_tick_interval_us;
    float throttle_factor;
} governor_t;

struct engram {
    engram_config_t config;
    engram_allocator_t allocator;

    neuron_pool_t neurons;
    synapse_pool_t synapses;
    cluster_pool_t clusters;
    pathway_pool_t pathways;

    hippocampus_t hippocampus;
    neocortex_t neocortex;
    thalamus_t thalamus;
    amygdala_t amygdala;
    brainstem_t brainstem;
    governor_t governor;
    memory_index_t memory_index;

    working_memory_t working_memory;
    cue_queue_t cue_queue;

    engram_mutex_t state_mutex;
    uint64_t total_memory_bytes;
};

void *engram_alloc(engram_t *eng, size_t size);
void *engram_realloc(engram_t *eng, void *ptr, size_t size);
void engram_free(engram_t *eng, void *ptr);

#endif
