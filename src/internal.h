#ifndef ENGRAM_INTERNAL_H
#define ENGRAM_INTERNAL_H

#include "engram/types.h"
#include "engram/platform.h"
#include "engram/engram.h"
#include <stdatomic.h>

typedef struct engram_vocab {
    engram_token_t *tokens;
    uint32_t count;
    uint32_t capacity;
    engram_mutex_t *lock;
} engram_vocab_t;

typedef struct engram_substrate {
    engram_neuron_t *neurons;
    uint32_t neuron_count;
    engram_synapse_t *synapses;
    uint32_t synapse_count;
    uint32_t synapse_capacity;
    engram_cluster_t *clusters;
    uint32_t cluster_count;
    engram_pathway_t *pathways;
    uint32_t pathway_count;
    uint32_t pathway_capacity;
    engram_mutex_t *lock;
} engram_substrate_t;

typedef struct engram_hippocampus {
    engram_thread_t *thread;
    engram_mutex_t *lock;
    engram_cond_t *cond;
    engram_pattern_t *working_memory;
    uint32_t working_count;
    uint32_t working_capacity;
    _Atomic int running;
    uint32_t tick_ms;
} engram_hippocampus_t;

typedef struct engram_cortex {
    engram_thread_t *thread;
    engram_mutex_t *lock;
    engram_cond_t *cond;
    _Atomic int running;
    uint32_t tick_ms;
} engram_cortex_t;

typedef struct engram_wernicke {
    engram_vocab_t vocab;
    engram_pattern_t *embeddings;
    uint32_t embedding_count;
} engram_wernicke_t;

struct engram {
    engram_config_t config;
    engram_substrate_t substrate;
    engram_hippocampus_t hippocampus;
    engram_cortex_t cortex;
    engram_wernicke_t wernicke;
    _Atomic uint64_t tick;
    _Atomic int alive;
    engram_mutex_t *cue_lock;
    char *response_buffer;
    size_t response_capacity;
};

void engram_substrate_init(engram_substrate_t *s, uint32_t neuron_count, uint32_t synapse_capacity);
void engram_substrate_free(engram_substrate_t *s);

void engram_hippocampus_start(engram_t *eng);
void engram_hippocampus_stop(engram_t *eng);
void engram_hippocampus_encode(engram_t *eng, const engram_pattern_t *pattern);

void engram_cortex_start(engram_t *eng);
void engram_cortex_stop(engram_t *eng);

void engram_wernicke_init(engram_wernicke_t *w);
void engram_wernicke_free(engram_wernicke_t *w);
void engram_wernicke_tokenize(engram_wernicke_t *w, const char *text, size_t len,
                               uint16_t **ids, uint32_t *count);
void engram_wernicke_detokenize(engram_wernicke_t *w, const uint16_t *ids, uint32_t count,
                                 char *out, size_t out_size);
void engram_wernicke_embed(engram_wernicke_t *w, uint16_t id, engram_pattern_t *out);

void engram_encode_text(engram_t *eng, const char *text, size_t len, engram_pattern_t *out);
void engram_propagate(engram_t *eng, const engram_pattern_t *input, engram_pattern_t *output);
void engram_recall(engram_t *eng, const engram_pattern_t *cue, engram_pattern_t *result);
void engram_learn(engram_t *eng, const engram_pattern_t *pattern);
void engram_decay_tick(engram_t *eng);
void engram_consolidate(engram_t *eng);

void engram_synapse_create(engram_substrate_t *s, uint32_t pre, uint32_t post, float weight);
void engram_synapse_strengthen(engram_synapse_t *syn, float amount);
void engram_synapse_weaken(engram_synapse_t *syn, float amount);
void engram_synapse_prune(engram_substrate_t *s, float threshold);

void engram_pathway_create(engram_substrate_t *s, uint32_t *neurons, uint32_t count, float strength);
void engram_pathway_strengthen(engram_pathway_t *p, float amount);
void engram_pathway_decay(engram_pathway_t *p, float rate);
void engram_pathway_activate(engram_substrate_t *s, uint32_t *neurons, uint32_t count, float strength);

uint32_t engram_hash(const void *data, size_t len);
float engram_pattern_similarity(const engram_pattern_t *a, const engram_pattern_t *b);
void engram_pattern_add(engram_pattern_t *dst, const engram_pattern_t *src, float scale);
void engram_pattern_normalize(engram_pattern_t *p);
void engram_pattern_sparse_encode(const float *dense, engram_pattern_t *out);
void engram_pattern_from_text(const char *text, size_t len, engram_pattern_t *out);

float engram_random_float(void);
uint32_t engram_random_uint(void);
void engram_random_seed(uint64_t seed);

#ifdef ENGRAM_VULKAN_ENABLED
typedef struct engram_vk_context engram_vk_context_t;
engram_vk_context_t *engram_vk_init(void);
void engram_vk_destroy(engram_vk_context_t *ctx);
void engram_vk_propagate(engram_vk_context_t *ctx, engram_substrate_t *s,
                          const engram_pattern_t *input, engram_pattern_t *output);
void engram_vk_decay(engram_vk_context_t *ctx, engram_substrate_t *s, float rate);
#endif

#endif
