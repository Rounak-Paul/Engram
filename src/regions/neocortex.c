#include "../internal.h"
#include "../core.h"
#include "../regions.h"
#include <string.h>

int neocortex_init(engram_t *eng) {
    uint32_t capacity = ENGRAM_MAX_PATHWAYS / 2;

    eng->neocortex.consolidated_pathway_ids = engram_alloc(eng, capacity * sizeof(uint32_t));
    if (!eng->neocortex.consolidated_pathway_ids) {
        return -1;
    }
    memset(eng->neocortex.consolidated_pathway_ids, 0xFF, capacity * sizeof(uint32_t));

    eng->neocortex.capacity = capacity;
    eng->neocortex.count = 0;

    return 0;
}

void neocortex_destroy(engram_t *eng) {
    if (eng->neocortex.consolidated_pathway_ids) {
        engram_free(eng, eng->neocortex.consolidated_pathway_ids);
        eng->neocortex.consolidated_pathway_ids = NULL;
    }
    eng->neocortex.capacity = 0;
    eng->neocortex.count = 0;
}

int neocortex_consolidate_pathway(engram_t *eng, uint32_t pathway_id) {
    engram_pathway_t *p = pathway_get(eng, pathway_id);
    if (!p) {
        return -1;
    }

    if (p->activation_count < 3) {
        return 1;
    }

    if (p->strength < eng->config.consolidation_threshold) {
        return 1;
    }

    for (uint32_t i = 0; i < eng->neocortex.count; i++) {
        if (eng->neocortex.consolidated_pathway_ids[i] == pathway_id) {
            return 0;
        }
    }

    if (eng->neocortex.count >= eng->neocortex.capacity) {
        return -1;
    }

    eng->neocortex.consolidated_pathway_ids[eng->neocortex.count++] = pathway_id;

    for (uint32_t i = 0; i < p->neuron_count - 1; i++) {
        uint32_t pre = p->neuron_ids[i];
        uint32_t post = p->neuron_ids[i + 1];

        uint32_t syn_idx = synapse_find(eng, pre, post);
        if (syn_idx != UINT32_MAX) {
            engram_synapse_t *s = synapse_get(eng, syn_idx);
            if (s) {
                s->flags |= ENGRAM_SYNAPSE_FLAG_LOCKED;
                s->weight *= 1.2f;
                if (s->weight > 2.0f) {
                    s->weight = 2.0f;
                }
            }
        } else {
            syn_idx = synapse_create(eng, pre, post, 0.5f);
            if (syn_idx != UINT32_MAX) {
                engram_synapse_t *s = synapse_get(eng, syn_idx);
                if (s) {
                    s->flags |= ENGRAM_SYNAPSE_FLAG_LOCKED;
                }
            }
        }
    }

    return 0;
}

uint32_t neocortex_find_pathway(engram_t *eng, uint32_t *neuron_ids, uint32_t count) {
    float best_overlap = 0.0f;
    uint32_t best_pathway = UINT32_MAX;

    for (uint32_t i = 0; i < eng->neocortex.count; i++) {
        uint32_t pid = eng->neocortex.consolidated_pathway_ids[i];
        engram_pathway_t *p = pathway_get(eng, pid);
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
            best_pathway = pid;
        }
    }

    if (best_overlap < 0.3f) {
        return UINT32_MAX;
    }

    return best_pathway;
}

void neocortex_strengthen_pathway(engram_t *eng, uint32_t pathway_id) {
    engram_pathway_t *p = pathway_get(eng, pathway_id);
    if (!p) {
        return;
    }

    p->strength *= 1.1f;
    if (p->strength > 10.0f) {
        p->strength = 10.0f;
    }
}
