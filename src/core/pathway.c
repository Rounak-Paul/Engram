#include "internal.h"
#include <stdlib.h>
#include <string.h>

void engram_pathway_create(engram_substrate_t *s, uint32_t *neurons, uint32_t count, float strength) {
    if (s->pathway_count >= s->pathway_capacity) return;
    if (count == 0) return;
    
    engram_pathway_t *p = &s->pathways[s->pathway_count];
    p->id = s->pathway_count;
    p->neuron_ids = malloc(count * sizeof(uint32_t));
    memcpy(p->neuron_ids, neurons, count * sizeof(uint32_t));
    p->neuron_count = count;
    p->strength = strength;
    p->access_count = 1;
    p->last_access = 0;
    p->created_tick = 0;
    
    s->pathway_count++;
    
    for (uint32_t i = 0; i + 1 < count; i++) {
        engram_synapse_create(s, neurons[i], neurons[i + 1], strength);
    }
}

void engram_pathway_strengthen(engram_pathway_t *p, float amount) {
    p->strength += amount;
    if (p->strength > 1.0f) p->strength = 1.0f;
    p->access_count++;
}

void engram_pathway_decay(engram_pathway_t *p, float rate) {
    p->strength *= (1.0f - rate);
}

static engram_pathway_t *find_pathway_by_pattern(engram_substrate_t *s, uint32_t *neurons, uint32_t count) {
    for (uint32_t i = 0; i < s->pathway_count; i++) {
        engram_pathway_t *p = &s->pathways[i];
        if (p->neuron_count != count) continue;
        
        int match = 1;
        for (uint32_t j = 0; j < count && match; j++) {
            if (p->neuron_ids[j] != neurons[j]) match = 0;
        }
        if (match) return p;
    }
    return NULL;
}

void engram_pathway_activate(engram_substrate_t *s, uint32_t *neurons, uint32_t count, float strength) {
    engram_pathway_t *existing = find_pathway_by_pattern(s, neurons, count);
    if (existing) {
        engram_pathway_strengthen(existing, strength * 0.1f);
    } else {
        engram_pathway_create(s, neurons, count, strength);
    }
}
