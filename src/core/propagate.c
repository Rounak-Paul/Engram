#include "internal.h"
#include <string.h>
#include <math.h>

typedef struct {
    substrate_t *s;
    float activation;
    float decay;
    size_t depth;
} propagate_ctx_t;

static void propagate_synapse(synapse_t *syn, void *ctx) {
    propagate_ctx_t *p = ctx;
    if (syn->weight > 0.01f) {
        float propagated = p->activation * syn->weight * p->decay;
        syn->last_activation = p->s->tick;
        propagate_activation(p->s, syn->target, propagated, p->decay, p->depth - 1);
    }
}

void propagate_activation(substrate_t *s, engram_id_t source, float activation, float decay, size_t depth) {
    if (depth == 0 || activation < 0.01f) return;
    
    neuron_t *src = substrate_find_neuron(s, source);
    if (!src) return;
    
    src->activation += activation;
    if (src->activation > 1.0f) src->activation = 1.0f;
    src->last_access = s->tick;
    src->access_count++;
    src->importance += activation * 0.1f;
    
    propagate_ctx_t ctx = { .s = s, .activation = activation, .decay = decay, .depth = depth };
    substrate_for_each_synapse(s, source, propagate_synapse, &ctx);
}

void propagate_learning(substrate_t *s, engram_id_t *active, size_t count, float rate) {
    for (size_t i = 0; i < count; i++) {
        for (size_t j = i + 1; j < count; j++) {
            engram_id_t a = active[i];
            engram_id_t b = active[j];
            
            substrate_add_synapse(s, a, b, rate);
            substrate_add_synapse(s, b, a, rate);
        }
    }
}
