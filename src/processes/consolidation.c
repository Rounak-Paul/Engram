#include "../internal.h"
#include "../core.h"
#include "../regions.h"
#include "../processes.h"

void consolidation_step(engram_t *eng, uint64_t tick) {
    (void)tick;

    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        engram_pathway_t *p = pathway_get(eng, i);
        if (!p) {
            continue;
        }

        if (consolidation_check_pathway(eng, i)) {
            neocortex_consolidate_pathway(eng, i);
        }
    }
}

int consolidation_check_pathway(engram_t *eng, uint32_t pathway_id) {
    engram_pathway_t *p = pathway_get(eng, pathway_id);
    if (!p) {
        return 0;
    }

    if (p->activation_count < 3) {
        return 0;
    }

    if (p->strength < eng->config.consolidation_threshold) {
        return 0;
    }

    for (uint32_t i = 0; i < eng->neocortex.count; i++) {
        if (eng->neocortex.consolidated_pathway_ids[i] == pathway_id) {
            return 0;
        }
    }

    return 1;
}

void consolidation_merge_similar(engram_t *eng) {
    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        engram_pathway_t *p1 = pathway_get(eng, i);
        if (!p1) {
            continue;
        }

        for (uint32_t j = i + 1; j < eng->pathways.capacity; j++) {
            engram_pathway_t *p2 = pathway_get(eng, j);
            if (!p2) {
                continue;
            }

            uint32_t matches = 0;
            for (uint32_t k = 0; k < p1->neuron_count; k++) {
                for (uint32_t l = 0; l < p2->neuron_count; l++) {
                    if (p1->neuron_ids[k] == p2->neuron_ids[l]) {
                        matches++;
                    }
                }
            }

            uint32_t max_count = p1->neuron_count > p2->neuron_count ? p1->neuron_count : p2->neuron_count;
            float overlap = (float)matches / (float)max_count;

            if (overlap > 0.7f) {
                if (p1->strength > p2->strength) {
                    p1->strength += p2->strength * 0.5f;
                    p1->activation_count += p2->activation_count;
                    pathway_destroy(eng, j);
                } else {
                    p2->strength += p1->strength * 0.5f;
                    p2->activation_count += p1->activation_count;
                    pathway_destroy(eng, i);
                    break;
                }
            }
        }
    }
}
