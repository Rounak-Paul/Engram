#include "internal.h"
#include "core.h"
#include <string.h>

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
    eng->synapses.capacity = 0;
    eng->synapses.count = 0;
    eng->synapses.free_count = 0;
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
    for (uint32_t i = 0; i < eng->synapses.capacity; i++) {
        engram_synapse_t *s = &eng->synapses.synapses[i];
        if (s->pre_neuron_id == pre_id && s->post_neuron_id == post_id && s->weight > 0.0f) {
            return i;
        }
    }
    return UINT32_MAX;
}
