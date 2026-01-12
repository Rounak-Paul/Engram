#include "internal.h"
#include <stdlib.h>
#include <string.h>

substrate_t substrate_create(size_t neuron_cap, size_t synapse_cap) {
    substrate_t s = {0};
    s.neurons = calloc(neuron_cap, sizeof(neuron_t));
    s.synapses = calloc(synapse_cap, sizeof(synapse_t));
    s.neuron_capacity = neuron_cap;
    s.synapse_capacity = synapse_cap;
    s.next_id = 1;
    s.tick = 0;
    return s;
}

void substrate_destroy(substrate_t *s) {
    free(s->neurons);
    free(s->synapses);
    memset(s, 0, sizeof(*s));
}

neuron_t *substrate_alloc_neuron(substrate_t *s) {
    if (s->neuron_count >= s->neuron_capacity) {
        size_t new_cap = s->neuron_capacity * 2;
        neuron_t *new_neurons = realloc(s->neurons, new_cap * sizeof(neuron_t));
        if (!new_neurons) return NULL;
        memset(new_neurons + s->neuron_capacity, 0, 
               (new_cap - s->neuron_capacity) * sizeof(neuron_t));
        s->neurons = new_neurons;
        s->neuron_capacity = new_cap;
    }
    neuron_t *n = &s->neurons[s->neuron_count++];
    memset(n, 0, sizeof(*n));
    n->id = s->next_id++;
    n->last_access = s->tick;
    n->importance = 1.0f;
    return n;
}

synapse_t *substrate_alloc_synapse(substrate_t *s) {
    if (s->synapse_count >= s->synapse_capacity) {
        size_t new_cap = s->synapse_capacity * 2;
        synapse_t *new_synapses = realloc(s->synapses, new_cap * sizeof(synapse_t));
        if (!new_synapses) return NULL;
        memset(new_synapses + s->synapse_capacity, 0,
               (new_cap - s->synapse_capacity) * sizeof(synapse_t));
        s->synapses = new_synapses;
        s->synapse_capacity = new_cap;
    }
    synapse_t *syn = &s->synapses[s->synapse_count++];
    memset(syn, 0, sizeof(*syn));
    return syn;
}

neuron_t *substrate_find_neuron(substrate_t *s, engram_id_t id) {
    for (size_t i = 0; i < s->neuron_count; i++) {
        if (s->neurons[i].id == id) {
            return &s->neurons[i];
        }
    }
    return NULL;
}

void substrate_decay(substrate_t *s, float rate) {
    s->tick++;
    for (size_t i = 0; i < s->neuron_count; i++) {
        neuron_t *n = &s->neurons[i];
        n->activation *= (1.0f - rate);
        uint64_t age = s->tick - n->last_access;
        if (age > 0) {
            float time_decay = 1.0f / (1.0f + (float)age * rate * 0.01f);
            n->importance *= time_decay;
        }
    }
    for (size_t i = 0; i < s->synapse_count; i++) {
        synapse_t *syn = &s->synapses[i];
        uint64_t age = s->tick - syn->last_activation;
        if (age > 100) {
            syn->weight *= (1.0f - rate * 0.1f);
        }
    }
}

void substrate_prune(substrate_t *s, float threshold) {
    size_t write = 0;
    for (size_t read = 0; read < s->synapse_count; read++) {
        if (s->synapses[read].weight >= threshold) {
            if (write != read) {
                s->synapses[write] = s->synapses[read];
            }
            write++;
        }
    }
    s->synapse_count = write;
    
    write = 0;
    for (size_t read = 0; read < s->neuron_count; read++) {
        if (s->neurons[read].importance >= threshold * 0.1f || 
            s->neurons[read].access_count > 0) {
            if (write != read) {
                s->neurons[write] = s->neurons[read];
            }
            write++;
        }
    }
    s->neuron_count = write;
}
