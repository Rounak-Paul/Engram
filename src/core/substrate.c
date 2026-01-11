#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void engram_substrate_init(engram_substrate_t *s, uint32_t neuron_count, uint32_t synapse_capacity) {
    memset(s, 0, sizeof(*s));
    
    s->neuron_count = neuron_count;
    s->neurons = calloc(neuron_count, sizeof(engram_neuron_t));
    
    for (uint32_t i = 0; i < neuron_count; i++) {
        s->neurons[i].id = i;
        s->neurons[i].activation = 0.0f;
        s->neurons[i].threshold = 0.5f;
        s->neurons[i].fatigue = 0.0f;
        s->neurons[i].cluster_id = (uint16_t)(i / ENGRAM_CLUSTER_SIZE);
    }
    
    s->synapse_capacity = synapse_capacity;
    s->synapse_count = 0;
    s->synapses = calloc(synapse_capacity, sizeof(engram_synapse_t));
    
    s->cluster_count = (neuron_count + ENGRAM_CLUSTER_SIZE - 1) / ENGRAM_CLUSTER_SIZE;
    s->clusters = calloc(s->cluster_count, sizeof(engram_cluster_t));
    
    for (uint32_t i = 0; i < s->cluster_count; i++) {
        s->clusters[i].id = i;
        uint32_t start = i * ENGRAM_CLUSTER_SIZE;
        uint32_t end = start + ENGRAM_CLUSTER_SIZE;
        if (end > neuron_count) end = neuron_count;
        s->clusters[i].neurons = &s->neurons[start];
        s->clusters[i].neuron_count = end - start;
        s->clusters[i].lateral_inhibition = 0.1f;
    }
    
    s->pathway_capacity = ENGRAM_MAX_PATHWAYS;
    s->pathway_count = 0;
    s->pathways = calloc(s->pathway_capacity, sizeof(engram_pathway_t));
    
    s->lock = engram_mutex_create();
}

void engram_substrate_free(engram_substrate_t *s) {
    if (!s) return;
    
    for (uint32_t i = 0; i < s->pathway_count; i++) {
        free(s->pathways[i].neuron_ids);
    }
    free(s->pathways);
    free(s->clusters);
    free(s->synapses);
    free(s->neurons);
    engram_mutex_destroy(s->lock);
    memset(s, 0, sizeof(*s));
}
