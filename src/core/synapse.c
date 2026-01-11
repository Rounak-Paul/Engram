#include "internal.h"
#include <stdlib.h>
#include <math.h>

void engram_synapse_create(engram_substrate_t *s, uint32_t pre, uint32_t post, float weight) {
    if (s->synapse_count >= s->synapse_capacity) return;
    if (pre >= s->neuron_count || post >= s->neuron_count) return;
    if (pre == post) return;
    
    for (uint32_t i = 0; i < s->synapse_count; i++) {
        if (s->synapses[i].pre == pre && s->synapses[i].post == post) {
            atomic_store(&s->synapses[i].weight, weight);
            return;
        }
    }
    
    engram_synapse_t *syn = &s->synapses[s->synapse_count++];
    syn->pre = pre;
    syn->post = post;
    atomic_store(&syn->weight, weight);
    syn->eligibility = 1.0f;
    syn->last_active = 0;
    syn->potentiation = 0;
    syn->flags = 0;
}

void engram_synapse_strengthen(engram_synapse_t *syn, float amount) {
    float w = atomic_load(&syn->weight);
    w += amount * syn->eligibility;
    if (w > 1.0f) w = 1.0f;
    atomic_store(&syn->weight, w);
    syn->potentiation++;
}

void engram_synapse_weaken(engram_synapse_t *syn, float amount) {
    float w = atomic_load(&syn->weight);
    w -= amount;
    if (w < 0.0f) w = 0.0f;
    atomic_store(&syn->weight, w);
}

void engram_synapse_prune(engram_substrate_t *s, float threshold) {
    uint32_t write = 0;
    for (uint32_t read = 0; read < s->synapse_count; read++) {
        float w = atomic_load(&s->synapses[read].weight);
        if (w >= threshold) {
            if (write != read) {
                s->synapses[write] = s->synapses[read];
            }
            write++;
        }
    }
    s->synapse_count = write;
}
