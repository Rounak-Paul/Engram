#include "internal.h"
#include "core.h"
#include <string.h>

int pathway_pool_init(engram_t *eng) {
    eng->pathways.pathways = engram_alloc(eng, ENGRAM_MAX_PATHWAYS * sizeof(engram_pathway_t));
    if (!eng->pathways.pathways) {
        return -1;
    }
    memset(eng->pathways.pathways, 0, ENGRAM_MAX_PATHWAYS * sizeof(engram_pathway_t));

    eng->pathways.free_list = engram_alloc(eng, ENGRAM_MAX_PATHWAYS * sizeof(uint32_t));
    if (!eng->pathways.free_list) {
        engram_free(eng, eng->pathways.pathways);
        return -1;
    }

    for (uint32_t i = 0; i < ENGRAM_MAX_PATHWAYS; i++) {
        eng->pathways.free_list[i] = ENGRAM_MAX_PATHWAYS - 1 - i;
    }

    eng->pathways.capacity = ENGRAM_MAX_PATHWAYS;
    eng->pathways.count = 0;
    eng->pathways.free_count = ENGRAM_MAX_PATHWAYS;

    return 0;
}

void pathway_pool_destroy(engram_t *eng) {
    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        if (eng->pathways.pathways[i].neuron_ids) {
            engram_free(eng, eng->pathways.pathways[i].neuron_ids);
        }
        if (eng->pathways.pathways[i].original_data) {
            engram_free(eng, eng->pathways.pathways[i].original_data);
        }
    }
    if (eng->pathways.pathways) {
        engram_free(eng, eng->pathways.pathways);
        eng->pathways.pathways = NULL;
    }
    if (eng->pathways.free_list) {
        engram_free(eng, eng->pathways.free_list);
        eng->pathways.free_list = NULL;
    }
    eng->pathways.capacity = 0;
    eng->pathways.count = 0;
    eng->pathways.free_count = 0;
}

uint32_t pathway_create(engram_t *eng, uint32_t *neuron_ids, uint32_t count, uint64_t tick) {
    return pathway_create_with_data(eng, neuron_ids, count, tick, NULL, 0, 0, 0);
}

uint32_t pathway_create_with_data(engram_t *eng, uint32_t *neuron_ids, uint32_t count, uint64_t tick,
                                  const void *data, size_t data_size, uint32_t modality, uint64_t content_hash) {
    if (eng->pathways.free_count == 0) {
        return UINT32_MAX;
    }

    uint32_t idx = eng->pathways.free_list[--eng->pathways.free_count];
    engram_pathway_t *p = &eng->pathways.pathways[idx];

    p->neuron_ids = engram_alloc(eng, count * sizeof(uint32_t));
    if (!p->neuron_ids) {
        eng->pathways.free_list[eng->pathways.free_count++] = idx;
        return UINT32_MAX;
    }

    memcpy(p->neuron_ids, neuron_ids, count * sizeof(uint32_t));
    p->id = idx;
    p->neuron_count = count;
    p->strength = 1.0f;
    p->activation_count = 1;
    p->last_active_tick = (uint32_t)tick;
    p->created_tick = (uint32_t)tick;
    p->content_hash = content_hash;
    p->modality = modality;

    if (data && data_size > 0) {
        p->original_data = engram_alloc(eng, data_size);
        if (p->original_data) {
            memcpy(p->original_data, data, data_size);
            p->original_size = data_size;
        }
    } else {
        p->original_data = NULL;
        p->original_size = 0;
    }

    eng->pathways.count++;
    return idx;
}

void pathway_destroy(engram_t *eng, uint32_t idx) {
    if (idx >= eng->pathways.capacity) {
        return;
    }

    engram_pathway_t *p = &eng->pathways.pathways[idx];
    if (p->neuron_ids) {
        engram_free(eng, p->neuron_ids);
    }
    if (p->original_data) {
        engram_free(eng, p->original_data);
    }

    memset(p, 0, sizeof(engram_pathway_t));
    eng->pathways.free_list[eng->pathways.free_count++] = idx;
    eng->pathways.count--;
}

engram_pathway_t *pathway_get(engram_t *eng, uint32_t idx) {
    if (idx >= eng->pathways.capacity) {
        return NULL;
    }
    if (eng->pathways.pathways[idx].neuron_count == 0) {
        return NULL;
    }
    return &eng->pathways.pathways[idx];
}

void pathway_activate(engram_t *eng, uint32_t idx, uint64_t tick) {
    engram_pathway_t *p = pathway_get(eng, idx);
    if (!p) {
        return;
    }

    p->activation_count++;
    p->last_active_tick = (uint32_t)tick;
    p->strength += 0.1f;
    if (p->strength > 10.0f) {
        p->strength = 10.0f;
    }

    for (uint32_t i = 0; i < p->neuron_count; i++) {
        neuron_stimulate(eng, p->neuron_ids[i], 0.5f * p->strength);
    }
}

uint32_t pathway_find_matching(engram_t *eng, uint32_t *neuron_ids, uint32_t count, float threshold) {
    float best_overlap = threshold;
    uint32_t best_idx = UINT32_MAX;

    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        engram_pathway_t *p = pathway_get(eng, i);
        if (!p) {
            continue;
        }

        uint32_t matches = 0;
        for (uint32_t j = 0; j < count; j++) {
            for (uint32_t k = 0; k < p->neuron_count; k++) {
                if (neuron_ids[j] == p->neuron_ids[k]) {
                    matches++;
                    break;
                }
            }
        }

        float overlap = (float)matches / (float)(count > p->neuron_count ? count : p->neuron_count);
        if (overlap > best_overlap) {
            best_overlap = overlap;
            best_idx = i;
        }
    }

    return best_idx;
}

uint32_t pathway_find_by_hash(engram_t *eng, uint64_t content_hash) {
    if (content_hash == 0) {
        return UINT32_MAX;
    }

    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        engram_pathway_t *p = pathway_get(eng, i);
        if (p && p->content_hash == content_hash) {
            return i;
        }
    }

    return UINT32_MAX;
}

void pathway_prune_weak(engram_t *eng, float min_strength) {
    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        engram_pathway_t *p = pathway_get(eng, i);
        if (p && p->strength < min_strength) {
            pathway_destroy(eng, i);
        }
    }
}
