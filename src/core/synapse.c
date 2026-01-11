#include "internal.h"
#include "core.h"
#include <string.h>

static inline uint64_t synapse_hash_key(uint32_t pre_id, uint32_t post_id) {
    return ((uint64_t)pre_id << 32) | post_id;
}

static inline uint32_t synapse_hash_bucket(uint64_t key) {
    return (uint32_t)((key ^ (key >> 16)) & (ENGRAM_SYNAPSE_HASH_SIZE - 1));
}

int synapse_pool_init(engram_t *eng, uint32_t initial_capacity) {
    eng->synapses.synapses = engram_alloc(eng, initial_capacity * sizeof(engram_synapse_t));
    if (!eng->synapses.synapses) {
        return -1;
    }
    memset(eng->synapses.synapses, 0, initial_capacity * sizeof(engram_synapse_t));

    eng->synapses.free_list = engram_alloc(eng, initial_capacity * sizeof(uint32_t));
    if (!eng->synapses.free_list) {
        engram_free(eng, eng->synapses.synapses);
        return -1;
    }

    eng->synapses.hash_table = engram_alloc(eng, ENGRAM_SYNAPSE_HASH_SIZE * sizeof(uint32_t));
    if (!eng->synapses.hash_table) {
        engram_free(eng, eng->synapses.free_list);
        engram_free(eng, eng->synapses.synapses);
        return -1;
    }
    for (uint32_t i = 0; i < ENGRAM_SYNAPSE_HASH_SIZE; i++) {
        eng->synapses.hash_table[i] = ENGRAM_HASH_NONE;
    }

    eng->synapses.entry_pool_capacity = initial_capacity;
    eng->synapses.entry_pool = engram_alloc(eng, initial_capacity * sizeof(synapse_hash_entry_t));
    if (!eng->synapses.entry_pool) {
        engram_free(eng, eng->synapses.hash_table);
        engram_free(eng, eng->synapses.free_list);
        engram_free(eng, eng->synapses.synapses);
        return -1;
    }
    eng->synapses.entry_pool_count = 0;

    for (uint32_t i = 0; i < initial_capacity; i++) {
        eng->synapses.free_list[i] = initial_capacity - 1 - i;
    }

    eng->synapses.capacity = initial_capacity;
    eng->synapses.count = 0;
    eng->synapses.free_count = initial_capacity;

    return 0;
}

void synapse_pool_destroy(engram_t *eng) {
    if (eng->synapses.synapses) {
        engram_free(eng, eng->synapses.synapses);
        eng->synapses.synapses = NULL;
    }
    if (eng->synapses.free_list) {
        engram_free(eng, eng->synapses.free_list);
        eng->synapses.free_list = NULL;
    }
    if (eng->synapses.hash_table) {
        engram_free(eng, eng->synapses.hash_table);
        eng->synapses.hash_table = NULL;
    }
    if (eng->synapses.entry_pool) {
        engram_free(eng, eng->synapses.entry_pool);
        eng->synapses.entry_pool = NULL;
    }
    eng->synapses.capacity = 0;
    eng->synapses.count = 0;
    eng->synapses.free_count = 0;
    eng->synapses.entry_pool_count = 0;
    eng->synapses.entry_pool_capacity = 0;
}

static int synapse_pool_grow(engram_t *eng) {
    uint32_t new_capacity = eng->synapses.capacity + ENGRAM_SYNAPSE_POOL_BLOCK;

    engram_synapse_t *new_synapses = engram_realloc(eng, eng->synapses.synapses,
                                                     new_capacity * sizeof(engram_synapse_t));
    if (!new_synapses) {
        return -1;
    }
    eng->synapses.synapses = new_synapses;

    uint32_t *new_free_list = engram_realloc(eng, eng->synapses.free_list,
                                              new_capacity * sizeof(uint32_t));
    if (!new_free_list) {
        return -1;
    }
    eng->synapses.free_list = new_free_list;

    for (uint32_t i = eng->synapses.capacity; i < new_capacity; i++) {
        eng->synapses.free_list[eng->synapses.free_count++] = i;
        memset(&eng->synapses.synapses[i], 0, sizeof(engram_synapse_t));
    }

    eng->synapses.capacity = new_capacity;
    return 0;
}

static uint32_t synapse_hash_alloc(engram_t *eng) {
    if (eng->synapses.entry_pool_count >= eng->synapses.entry_pool_capacity) {
        uint32_t new_cap = eng->synapses.entry_pool_capacity * 2;
        synapse_hash_entry_t *new_pool = engram_realloc(eng, eng->synapses.entry_pool,
                                                         new_cap * sizeof(synapse_hash_entry_t));
        if (!new_pool) return ENGRAM_HASH_NONE;
        eng->synapses.entry_pool = new_pool;
        eng->synapses.entry_pool_capacity = new_cap;
    }
    return eng->synapses.entry_pool_count++;
}

uint32_t synapse_create(engram_t *eng, uint32_t pre_id, uint32_t post_id, float weight) {
    if (eng->synapses.free_count == 0) {
        if (synapse_pool_grow(eng) != 0) {
            return UINT32_MAX;
        }
    }

    uint32_t idx = eng->synapses.free_list[--eng->synapses.free_count];
    engram_synapse_t *s = &eng->synapses.synapses[idx];

    s->pre_neuron_id = pre_id;
    s->post_neuron_id = post_id;
    s->weight = weight;
    s->eligibility_trace = 0.0f;
    s->last_active_tick = 0;
    s->potentiation_count = 0;
    s->flags = ENGRAM_SYNAPSE_FLAG_NONE;

    uint64_t key = synapse_hash_key(pre_id, post_id);
    uint32_t bucket = synapse_hash_bucket(key);
    uint32_t entry_idx = synapse_hash_alloc(eng);
    if (entry_idx != ENGRAM_HASH_NONE) {
        synapse_hash_entry_t *entry = &eng->synapses.entry_pool[entry_idx];
        entry->key = key;
        entry->synapse_idx = idx;
        entry->next = eng->synapses.hash_table[bucket];
        eng->synapses.hash_table[bucket] = entry_idx;
    }

    engram_neuron_t *pre = neuron_get(eng, pre_id);
    if (pre) {
        if (pre->outgoing_count >= pre->outgoing_capacity) {
            uint16_t new_cap = pre->outgoing_capacity == 0 ? 8 : pre->outgoing_capacity * 2;
            uint32_t *new_list = engram_realloc(eng, pre->outgoing_synapses, new_cap * sizeof(uint32_t));
            if (new_list) {
                pre->outgoing_synapses = new_list;
                pre->outgoing_capacity = new_cap;
            }
        }
        if (pre->outgoing_count < pre->outgoing_capacity) {
            pre->outgoing_synapses[pre->outgoing_count++] = idx;
        }
    }

    eng->synapses.count++;
    return idx;
}

void synapse_destroy(engram_t *eng, uint32_t idx) {
    if (idx >= eng->synapses.capacity) {
        return;
    }

    engram_synapse_t *s = &eng->synapses.synapses[idx];

    uint64_t key = synapse_hash_key(s->pre_neuron_id, s->post_neuron_id);
    uint32_t bucket = synapse_hash_bucket(key);
    uint32_t *prev_idx = &eng->synapses.hash_table[bucket];
    uint32_t entry_idx = *prev_idx;
    while (entry_idx != ENGRAM_HASH_NONE) {
        synapse_hash_entry_t *entry = &eng->synapses.entry_pool[entry_idx];
        if (entry->synapse_idx == idx) {
            *prev_idx = entry->next;
            break;
        }
        prev_idx = &entry->next;
        entry_idx = entry->next;
    }

    engram_neuron_t *pre = neuron_get(eng, s->pre_neuron_id);
    if (pre && pre->outgoing_synapses) {
        for (uint16_t i = 0; i < pre->outgoing_count; i++) {
            if (pre->outgoing_synapses[i] == idx) {
                pre->outgoing_synapses[i] = pre->outgoing_synapses[--pre->outgoing_count];
                break;
            }
        }
    }

    memset(s, 0, sizeof(engram_synapse_t));
    eng->synapses.free_list[eng->synapses.free_count++] = idx;
    eng->synapses.count--;
}

engram_synapse_t *synapse_get(engram_t *eng, uint32_t idx) {
    if (idx >= eng->synapses.capacity) {
        return NULL;
    }
    return &eng->synapses.synapses[idx];
}

void synapse_update_eligibility(engram_t *eng, uint32_t idx, float dt) {
    engram_synapse_t *s = synapse_get(eng, idx);
    if (!s) {
        return;
    }

    s->eligibility_trace *= (1.0f - 0.1f * dt);
    if (s->eligibility_trace < 0.001f) {
        s->eligibility_trace = 0.0f;
    }
}

int synapse_prune_weak(engram_t *eng, float threshold) {
    int pruned = 0;
    for (uint32_t i = 0; i < eng->synapses.capacity; i++) {
        engram_synapse_t *s = &eng->synapses.synapses[i];
        if (s->weight > 0.0f && s->weight < threshold) {
            if (!(s->flags & ENGRAM_SYNAPSE_FLAG_LOCKED)) {
                synapse_destroy(eng, i);
                pruned++;
            }
        }
    }
    return pruned;
}

uint32_t synapse_find(engram_t *eng, uint32_t pre_id, uint32_t post_id) {
    uint64_t key = synapse_hash_key(pre_id, post_id);
    uint32_t bucket = synapse_hash_bucket(key);
    uint32_t entry_idx = eng->synapses.hash_table[bucket];
    while (entry_idx != ENGRAM_HASH_NONE) {
        synapse_hash_entry_t *entry = &eng->synapses.entry_pool[entry_idx];
        if (entry->key == key) {
            engram_synapse_t *s = &eng->synapses.synapses[entry->synapse_idx];
            if (s->weight > 0.0f) {
                return entry->synapse_idx;
            }
        }
        entry_idx = entry->next;
    }
    return UINT32_MAX;
}
