#include "internal.h"
#include <stdlib.h>
#include <math.h>

static void *cortex_thread(void *arg) {
    engram_t *eng = arg;
    engram_cortex_t *c = &eng->cortex;
    
    while (atomic_load(&c->running)) {
        engram_sleep_ms(c->tick_ms);
        
        if (!atomic_load(&eng->alive)) break;
        
        engram_mutex_lock(eng->substrate.lock);
        
        engram_decay_tick(eng);
        
        engram_consolidate(eng);
        
        engram_synapse_prune(&eng->substrate, 0.01f);
        
        engram_mutex_unlock(eng->substrate.lock);
    }
    
    return NULL;
}

void engram_cortex_start(engram_t *eng) {
    engram_cortex_t *c = &eng->cortex;
    
    c->lock = engram_mutex_create();
    c->cond = engram_cond_create();
    c->tick_ms = eng->config.consolidation_tick_ms;
    
    atomic_store(&c->running, 1);
    c->thread = engram_thread_create(cortex_thread, eng);
}

void engram_cortex_stop(engram_t *eng) {
    engram_cortex_t *c = &eng->cortex;
    
    atomic_store(&c->running, 0);
    engram_cond_signal(c->cond);
    
    engram_thread_join(c->thread);
    engram_thread_destroy(c->thread);
    
    engram_cond_destroy(c->cond);
    engram_mutex_destroy(c->lock);
}
