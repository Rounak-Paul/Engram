#include "internal.h"
#include <string.h>
#include <math.h>

void propagate_activation(substrate_t *s, engram_id_t source, float activation, float decay, size_t depth) {
    if (depth == 0 || activation < 0.01f) return;
    
    neuron_t *src = substrate_find_neuron(s, source);
    if (!src) return;
    
    src->activation += activation;
    if (src->activation > 1.0f) src->activation = 1.0f;
    src->last_access = s->tick;
    src->access_count++;
    src->importance += activation * 0.1f;
    
    for (size_t i = 0; i < s->synapse_count; i++) {
        synapse_t *syn = &s->synapses[i];
        if (syn->source == source && syn->weight > 0.01f) {
            float propagated = activation * syn->weight * decay;
            syn->last_activation = s->tick;
            propagate_activation(s, syn->target, propagated, decay, depth - 1);
        }
    }
}

void propagate_learning(substrate_t *s, engram_id_t *active, size_t count, float rate) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            engram_id_t a = active[i];
            engram_id_t b = active[j];
            
            synapse_t *existing = NULL;
            for (size_t k = 0; k < s->synapse_count; k++) {
                if ((s->synapses[k].source == a && s->synapses[k].target == b) ||
                    (s->synapses[k].source == b && s->synapses[k].target == a)) {
                    existing = &s->synapses[k];
                    break;
                }
            }
            
            if (existing) {
                existing->weight += rate * (1.0f - existing->weight);
                existing->last_activation = s->tick;
            } else {
                synapse_t *syn = substrate_alloc_synapse(s);
                if (syn) {
                    syn->source = a;
                    syn->target = b;
                    syn->weight = rate;
                    syn->last_activation = s->tick;
                }
                syn = substrate_alloc_synapse(s);
                if (syn) {
                    syn->source = b;
                    syn->target = a;
                    syn->weight = rate;
                    syn->last_activation = s->tick;
                }
            }
        }
    }
}
