#include "internal.h"
#include <stdlib.h>
#include <string.h>

#define NEURON_BUCKET_COUNT 16384
#define SYNAPSE_BUCKET_COUNT 65536
#define INVALID_INDEX ((size_t)-1)
#define INVALID_INDEX32 ((uint32_t)-1)

static inline size_t neuron_hash(engram_id_t id, size_t bucket_count) {
    return (size_t)(id * 2654435761U) % bucket_count;
}

static inline size_t synapse_hash(engram_id_t src, engram_id_t dst, size_t bucket_count) {
    uint64_t h = ((uint64_t)src << 32) | dst;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    return (size_t)(h % bucket_count);
}

static void neuron_index_init(neuron_index_t *idx, size_t capacity) {
    idx->bucket_count = NEURON_BUCKET_COUNT;
    idx->buckets = malloc(idx->bucket_count * sizeof(size_t));
    idx->next = malloc(capacity * sizeof(size_t));
    for (size_t i = 0; i < idx->bucket_count; i++) {
        idx->buckets[i] = INVALID_INDEX;
    }
    for (size_t i = 0; i < capacity; i++) {
        idx->next[i] = INVALID_INDEX;
    }
}

static void neuron_index_destroy(neuron_index_t *idx) {
    free(idx->buckets);
    free(idx->next);
    idx->buckets = NULL;
    idx->next = NULL;
}

static void neuron_index_insert(neuron_index_t *idx, engram_id_t id, size_t neuron_idx) {
    size_t bucket = neuron_hash(id, idx->bucket_count);
    idx->next[neuron_idx] = idx->buckets[bucket];
    idx->buckets[bucket] = neuron_idx;
}

static size_t neuron_index_find(neuron_index_t *idx, neuron_t *neurons, engram_id_t id) {
    size_t bucket = neuron_hash(id, idx->bucket_count);
    size_t i = idx->buckets[bucket];
    while (i != INVALID_INDEX) {
        if (neurons[i].id == id) return i;
        i = idx->next[i];
    }
    return INVALID_INDEX;
}

static void synapse_index_init(synapse_index_t *idx, size_t capacity) {
    idx->bucket_count = SYNAPSE_BUCKET_COUNT;
    idx->buckets = malloc(idx->bucket_count * sizeof(uint32_t));
    idx->next = malloc(capacity * sizeof(uint32_t));
    for (size_t i = 0; i < idx->bucket_count; i++) {
        idx->buckets[i] = INVALID_INDEX32;
    }
    for (size_t i = 0; i < capacity; i++) {
        idx->next[i] = INVALID_INDEX32;
    }
}

static void synapse_index_destroy(synapse_index_t *idx) {
    free(idx->buckets);
    free(idx->next);
    idx->buckets = NULL;
    idx->next = NULL;
}

static void synapse_index_insert(synapse_index_t *idx, engram_id_t src, engram_id_t dst, uint32_t syn_idx) {
    size_t bucket = synapse_hash(src, dst, idx->bucket_count);
    idx->next[syn_idx] = idx->buckets[bucket];
    idx->buckets[bucket] = syn_idx;
}

static uint32_t synapse_index_find(synapse_index_t *idx, synapse_t *synapses, engram_id_t src, engram_id_t dst) {
    size_t bucket = synapse_hash(src, dst, idx->bucket_count);
    uint32_t i = idx->buckets[bucket];
    while (i != INVALID_INDEX32) {
        if (synapses[i].source == src && synapses[i].target == dst) return i;
        i = idx->next[i];
    }
    return INVALID_INDEX32;
}

static void synapse_index_grow(synapse_index_t *idx, size_t old_cap, size_t new_cap) {
    uint32_t *new_next = realloc(idx->next, new_cap * sizeof(uint32_t));
    if (new_next) {
        for (size_t i = old_cap; i < new_cap; i++) {
            new_next[i] = INVALID_INDEX32;
        }
        idx->next = new_next;
    }
}

static void neuron_index_grow(neuron_index_t *idx, size_t old_cap, size_t new_cap) {
    size_t *new_next = realloc(idx->next, new_cap * sizeof(size_t));
    if (new_next) {
        for (size_t i = old_cap; i < new_cap; i++) {
            new_next[i] = INVALID_INDEX;
        }
        idx->next = new_next;
    }
}

substrate_t substrate_create(size_t neuron_cap, size_t synapse_cap) {
    substrate_t s = {0};
    s.neurons = calloc(neuron_cap, sizeof(neuron_t));
    s.synapses = calloc(synapse_cap, sizeof(synapse_t));
    s.neuron_capacity = neuron_cap;
    s.synapse_capacity = synapse_cap;
    s.next_id = 1;
    s.tick = 0;
    neuron_index_init(&s.neuron_idx, neuron_cap);
    synapse_index_init(&s.synapse_idx, synapse_cap);
    return s;
}

void substrate_destroy(substrate_t *s) {
    neuron_index_destroy(&s->neuron_idx);
    synapse_index_destroy(&s->synapse_idx);
    free(s->neurons);
    free(s->synapses);
    memset(s, 0, sizeof(*s));
}

neuron_t *substrate_alloc_neuron(substrate_t *s) {
    if (s->neuron_count >= s->neuron_capacity) {
        size_t old_cap = s->neuron_capacity;
        size_t new_cap = s->neuron_capacity * 2;
        neuron_t *new_neurons = realloc(s->neurons, new_cap * sizeof(neuron_t));
        if (!new_neurons) return NULL;
        memset(new_neurons + old_cap, 0, (new_cap - old_cap) * sizeof(neuron_t));
        s->neurons = new_neurons;
        s->neuron_capacity = new_cap;
        neuron_index_grow(&s->neuron_idx, old_cap, new_cap);
    }
    size_t idx = s->neuron_count++;
    neuron_t *n = &s->neurons[idx];
    memset(n, 0, sizeof(*n));
    n->id = s->next_id++;
    n->last_access = s->tick;
    n->importance = 1.0f;
    neuron_index_insert(&s->neuron_idx, n->id, idx);
    return n;
}

neuron_t *substrate_alloc_neuron_raw(substrate_t *s) {
    if (s->neuron_count >= s->neuron_capacity) {
        size_t old_cap = s->neuron_capacity;
        size_t new_cap = s->neuron_capacity * 2;
        neuron_t *new_neurons = realloc(s->neurons, new_cap * sizeof(neuron_t));
        if (!new_neurons) return NULL;
        memset(new_neurons + old_cap, 0, (new_cap - old_cap) * sizeof(neuron_t));
        s->neurons = new_neurons;
        s->neuron_capacity = new_cap;
        neuron_index_grow(&s->neuron_idx, old_cap, new_cap);
    }
    return &s->neurons[s->neuron_count++];
}

synapse_t *substrate_alloc_synapse(substrate_t *s) {
    if (s->synapse_count >= s->synapse_capacity) {
        size_t old_cap = s->synapse_capacity;
        size_t new_cap = s->synapse_capacity * 2;
        synapse_t *new_synapses = realloc(s->synapses, new_cap * sizeof(synapse_t));
        if (!new_synapses) return NULL;
        memset(new_synapses + old_cap, 0, (new_cap - old_cap) * sizeof(synapse_t));
        s->synapses = new_synapses;
        s->synapse_capacity = new_cap;
        synapse_index_grow(&s->synapse_idx, old_cap, new_cap);
    }
    synapse_t *syn = &s->synapses[s->synapse_count++];
    memset(syn, 0, sizeof(*syn));
    return syn;
}

synapse_t *substrate_alloc_synapse_raw(substrate_t *s) {
    if (s->synapse_count >= s->synapse_capacity) {
        size_t old_cap = s->synapse_capacity;
        size_t new_cap = s->synapse_capacity * 2;
        synapse_t *new_synapses = realloc(s->synapses, new_cap * sizeof(synapse_t));
        if (!new_synapses) return NULL;
        memset(new_synapses + old_cap, 0, (new_cap - old_cap) * sizeof(synapse_t));
        s->synapses = new_synapses;
        s->synapse_capacity = new_cap;
        synapse_index_grow(&s->synapse_idx, old_cap, new_cap);
    }
    return &s->synapses[s->synapse_count++];
}

void substrate_rebuild_indices(substrate_t *s) {
    for (size_t i = 0; i < s->neuron_idx.bucket_count; i++) {
        s->neuron_idx.buckets[i] = INVALID_INDEX;
    }
    for (size_t i = 0; i < s->neuron_count; i++) {
        s->neuron_idx.next[i] = INVALID_INDEX;
    }
    for (size_t i = 0; i < s->neuron_count; i++) {
        neuron_index_insert(&s->neuron_idx, s->neurons[i].id, i);
    }
    
    for (size_t i = 0; i < s->synapse_idx.bucket_count; i++) {
        s->synapse_idx.buckets[i] = INVALID_INDEX32;
    }
    for (size_t i = 0; i < s->synapse_count; i++) {
        s->synapse_idx.next[i] = INVALID_INDEX32;
    }
    for (size_t i = 0; i < s->synapse_count; i++) {
        synapse_t *syn = &s->synapses[i];
        size_t bucket = (((uint64_t)syn->source << 32) | syn->target) % s->synapse_idx.bucket_count;
        s->synapse_idx.next[i] = s->synapse_idx.buckets[bucket];
        s->synapse_idx.buckets[bucket] = (uint32_t)i;
    }
}

neuron_t *substrate_find_neuron(substrate_t *s, engram_id_t id) {
    size_t idx = neuron_index_find(&s->neuron_idx, s->neurons, id);
    return idx != INVALID_INDEX ? &s->neurons[idx] : NULL;
}

synapse_t *substrate_find_synapse(substrate_t *s, engram_id_t src, engram_id_t dst) {
    uint32_t idx = synapse_index_find(&s->synapse_idx, s->synapses, src, dst);
    return idx != INVALID_INDEX32 ? &s->synapses[idx] : NULL;
}

void substrate_for_each_synapse(substrate_t *s, engram_id_t src, void (*fn)(synapse_t*, void*), void *ctx) {
    for (size_t i = 0; i < s->synapse_count; i++) {
        if (s->synapses[i].source == src) {
            fn(&s->synapses[i], ctx);
        }
    }
}

void substrate_decay(substrate_t *s, float rate) {
    s->tick++;
    for (size_t i = 0; i < s->neuron_count; i++) {
        neuron_t *n = &s->neurons[i];
        n->activation *= (1.0f - rate);
        uint64_t age = s->tick - n->last_access;
        if (age > 0) {
            float time_decay = 1.0f / (1.0f + (float)age * rate * 0.01f);
            n->importance *= time_decay;
        }
    }
    for (size_t i = 0; i < s->synapse_count; i++) {
        synapse_t *syn = &s->synapses[i];
        uint64_t age = s->tick - syn->last_activation;
        if (age > 100) {
            syn->weight *= (1.0f - rate * 0.1f);
        }
    }
}

void substrate_prune(substrate_t *s, float threshold) {
    size_t write = 0;
    for (size_t read = 0; read < s->synapse_count; read++) {
        if (s->synapses[read].weight >= threshold) {
            if (write != read) {
                s->synapses[write] = s->synapses[read];
            }
            write++;
        }
    }
    s->synapse_count = write;
    
    for (size_t i = 0; i < s->synapse_idx.bucket_count; i++) {
        s->synapse_idx.buckets[i] = INVALID_INDEX32;
    }
    for (size_t i = 0; i < s->synapse_count; i++) {
        synapse_index_insert(&s->synapse_idx, s->synapses[i].source, s->synapses[i].target, (uint32_t)i);
    }
    
    write = 0;
    for (size_t read = 0; read < s->neuron_count; read++) {
        if (s->neurons[read].importance >= threshold * 0.1f || 
            s->neurons[read].access_count > 0) {
            if (write != read) {
                s->neurons[write] = s->neurons[read];
            }
            write++;
        }
    }
    s->neuron_count = write;
    
    for (size_t i = 0; i < s->neuron_idx.bucket_count; i++) {
        s->neuron_idx.buckets[i] = INVALID_INDEX;
    }
    for (size_t i = 0; i < s->neuron_count; i++) {
        neuron_index_insert(&s->neuron_idx, s->neurons[i].id, i);
    }
}
