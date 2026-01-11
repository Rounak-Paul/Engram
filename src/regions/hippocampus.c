#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void *hippocampus_thread(void *arg) {
    engram_t *eng = arg;
    engram_hippocampus_t *h = &eng->hippocampus;
    
    while (atomic_load(&h->running)) {
        engram_sleep_ms(h->tick_ms);
        
        if (!atomic_load(&eng->alive)) break;
        
        engram_mutex_lock(h->lock);
        
        for (uint32_t i = 0; i < h->working_count; i++) {
            engram_pattern_t *p = &h->working_memory[i];
            engram_learn(eng, p);
        }
        
        if (h->working_count > 0) {
            for (uint32_t i = 0; i + 1 < h->working_count; i++) {
                float sim = engram_pattern_similarity(&h->working_memory[i], 
                                                       &h->working_memory[i + 1]);
                if (sim > 0.3f) {
                    uint32_t neurons[8];
                    uint32_t count = 0;
                    for (uint32_t j = 0; j < h->working_memory[i].active_count && count < 4; j++) {
                        neurons[count++] = h->working_memory[i].active_indices[j] % eng->substrate.neuron_count;
                    }
                    for (uint32_t j = 0; j < h->working_memory[i + 1].active_count && count < 8; j++) {
                        neurons[count++] = h->working_memory[i + 1].active_indices[j] % eng->substrate.neuron_count;
                    }
                    if (count > 1) {
                        engram_mutex_lock(eng->substrate.lock);
                        engram_pathway_create(&eng->substrate, neurons, count, sim);
                        engram_mutex_unlock(eng->substrate.lock);
                    }
                }
            }
        }
        
        engram_mutex_unlock(h->lock);
        
        atomic_fetch_add(&eng->tick, 1);
    }
    
    return NULL;
}

void engram_hippocampus_start(engram_t *eng) {
    engram_hippocampus_t *h = &eng->hippocampus;
    
    h->lock = engram_mutex_create();
    h->cond = engram_cond_create();
    h->working_capacity = 64;
    h->working_memory = calloc(h->working_capacity, sizeof(engram_pattern_t));
    h->working_count = 0;
    h->tick_ms = eng->config.hippocampus_tick_ms;
    
    atomic_store(&h->running, 1);
    h->thread = engram_thread_create(hippocampus_thread, eng);
}

void engram_hippocampus_stop(engram_t *eng) {
    engram_hippocampus_t *h = &eng->hippocampus;
    
    atomic_store(&h->running, 0);
    engram_cond_signal(h->cond);
    
    engram_thread_join(h->thread);
    engram_thread_destroy(h->thread);
    
    engram_cond_destroy(h->cond);
    engram_mutex_destroy(h->lock);
    free(h->working_memory);
}

void engram_hippocampus_encode(engram_t *eng, const engram_pattern_t *pattern) {
    engram_hippocampus_t *h = &eng->hippocampus;
    
    engram_mutex_lock(h->lock);
    
    if (h->working_count >= h->working_capacity) {
        memmove(&h->working_memory[0], &h->working_memory[1], 
                (h->working_capacity - 1) * sizeof(engram_pattern_t));
        h->working_count = h->working_capacity - 1;
    }
    
    memcpy(&h->working_memory[h->working_count], pattern, sizeof(engram_pattern_t));
    h->working_count++;
    
    engram_mutex_unlock(h->lock);
}
