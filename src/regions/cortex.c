#include "internal.h"
#include <stdlib.h>
#include <string.h>

cortex_t cortex_create(size_t capacity) {
    cortex_t c = {0};
    c.index = calloc(capacity, sizeof(engram_id_t));
    c.index_size = 0;
    c.similarity_cache = calloc(capacity, sizeof(float));
    c.cache_size = capacity;
    return c;
}

void cortex_destroy(cortex_t *c) {
    free(c->index);
    free(c->similarity_cache);
    memset(c, 0, sizeof(*c));
}

void cortex_store(cortex_t *c, engram_id_t id) {
    for (size_t i = 0; i < c->index_size; i++) {
        if (c->index[i] == id) return;
    }
    
    if (c->index_size < c->cache_size) {
        c->index[c->index_size++] = id;
    }
}

size_t cortex_query(cortex_t *c, substrate_t *s, const float *query,
                    engram_id_t *results, float *scores, size_t max_results, float threshold) {
    size_t result_count = 0;
    
    for (size_t i = 0; i < c->index_size && i < c->cache_size; i++) {
        c->similarity_cache[i] = -1.0f;
    }
    
    for (size_t i = 0; i < s->neuron_count; i++) {
        neuron_t *n = &s->neurons[i];
        float sim = vec_dot(query, n->embedding, s->vector_dim);
        
        sim *= (0.5f + 0.5f * n->importance / (1.0f + n->importance));
        
        if (sim >= threshold) {
            size_t insert_pos = result_count;
            for (size_t j = 0; j < result_count; j++) {
                if (sim > scores[j]) {
                    insert_pos = j;
                    break;
                }
            }
            
            if (insert_pos < max_results) {
                size_t move_count = result_count - insert_pos;
                if (result_count >= max_results) move_count--;
                
                if (move_count > 0) {
                    memmove(&results[insert_pos + 1], &results[insert_pos], 
                            move_count * sizeof(engram_id_t));
                    memmove(&scores[insert_pos + 1], &scores[insert_pos],
                            move_count * sizeof(float));
                }
                
                results[insert_pos] = n->id;
                scores[insert_pos] = sim;
                
                if (result_count < max_results) result_count++;
            }
        }
    }
    
    return result_count;
}

typedef struct {
    substrate_t *s;
    float modulation;
    engram_id_t *recruited;
    size_t *count;
    size_t cap;
} complete_ctx_t;

static void recruit_synapse(synapse_t *syn, void *ctx) {
    complete_ctx_t *c = ctx;
    if (syn->sign <= 0 || syn->weight < 0.3f) return;
    if (*c->count >= c->cap) return;

    neuron_t *n = substrate_find_neuron(c->s, syn->target);
    if (!n) return;

    float drive = syn->weight * c->modulation;
    n->potential += drive;
    if (n->potential >= n->threshold && c->s->tick >= n->refractory_until) {
        for (size_t i = 0; i < *c->count; i++) {
            if (c->recruited[i] == syn->target) return;
        }
        c->recruited[(*c->count)++] = syn->target;
    }
}

void cortex_complete(substrate_t *s, const engram_id_t *seeds, size_t seed_count,
                     float modulation, size_t max_recruit) {
    if (modulation <= 0.0f || max_recruit == 0) return;

    engram_id_t recruited[ENGRAM_MAX_ACTIVATIONS];
    size_t count = 0;
    size_t cap = max_recruit < ENGRAM_MAX_ACTIVATIONS ? max_recruit : ENGRAM_MAX_ACTIVATIONS;

    complete_ctx_t ctx = { .s = s, .modulation = modulation,
                           .recruited = recruited, .count = &count, .cap = cap };

    for (size_t i = 0; i < seed_count; i++) {
        substrate_for_each_synapse(s, seeds[i], recruit_synapse, &ctx);
    }

    for (size_t i = 0; i < count; i++) {
        neuron_t *n = substrate_find_neuron(s, recruited[i]);
        if (n) {
            n->activation = n->potential > 1.0f ? 1.0f : n->potential;
            n->last_fire_tick = s->tick;
            n->importance += 0.05f;
        }
    }
}
