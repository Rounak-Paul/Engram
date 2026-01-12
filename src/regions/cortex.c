#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

size_t cortex_query(cortex_t *c, substrate_t *s, const engram_vec_t query,
                    engram_id_t *results, float *scores, size_t max_results, float threshold) {
    size_t result_count = 0;
    
    for (size_t i = 0; i < c->index_size && i < c->cache_size; i++) {
        c->similarity_cache[i] = -1.0f;
    }
    
    for (size_t i = 0; i < s->neuron_count; i++) {
        neuron_t *n = &s->neurons[i];
        float sim = vec_dot(query, n->embedding);
        
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
