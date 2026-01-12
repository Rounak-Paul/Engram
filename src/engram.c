#include "engram/engram.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DEFAULT_MAX_NEURONS 100000
#define DEFAULT_MAX_SYNAPSES 1000000
#define DEFAULT_DECAY_RATE 0.001f
#define DEFAULT_ACTIVATION_THRESHOLD 0.95f
#define DEFAULT_LEARNING_RATE 0.1f
#define DEFAULT_NOISE_THRESHOLD 0.01f

engram_config_t engram_config_default(void) {
    return (engram_config_t){
        .enable_gpu = true,
        .max_neurons = DEFAULT_MAX_NEURONS,
        .max_synapses = DEFAULT_MAX_SYNAPSES,
        .decay_rate = DEFAULT_DECAY_RATE,
        .activation_threshold = DEFAULT_ACTIVATION_THRESHOLD,
        .learning_rate = DEFAULT_LEARNING_RATE,
        .noise_threshold = DEFAULT_NOISE_THRESHOLD
    };
}

engram_t *engram_create(const engram_config_t *config) {
    engram_t *e = calloc(1, sizeof(engram_t));
    if (!e) return NULL;
    
    if (config) {
        e->config = *config;
    } else {
        e->config = engram_config_default();
    }
    
    e->substrate = substrate_create(e->config.max_neurons, e->config.max_synapses);
    if (!e->substrate.neurons) {
        free(e);
        return NULL;
    }
    
    e->wernicke = wernicke_create();
    if (!e->wernicke.char_hashes) {
        substrate_destroy(&e->substrate);
        free(e);
        return NULL;
    }
    
    e->hippocampus = hippocampus_create(ENGRAM_MAX_ACTIVATIONS * 10);
    e->cortex = cortex_create(e->config.max_neurons);
    
#ifdef ENGRAM_VULKAN_ENABLED
    if (e->config.enable_gpu) {
        e->vulkan = vulkan_create();
        e->gpu_available = e->vulkan.initialized;
    }
#endif
    
    return e;
}

void engram_destroy(engram_t *e) {
    if (!e) return;
    
#ifdef ENGRAM_VULKAN_ENABLED
    vulkan_destroy(&e->vulkan);
#endif
    
    cortex_destroy(&e->cortex);
    hippocampus_destroy(&e->hippocampus);
    wernicke_destroy(&e->wernicke);
    substrate_destroy(&e->substrate);
    
    free(e);
}

static float compute_noise_score(const engram_vec_t v) {
    float variance = 0.0f;
    float mean = 0.0f;
    
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        mean += v[i];
    }
    mean /= ENGRAM_VECTOR_DIM;
    
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        float diff = v[i] - mean;
        variance += diff * diff;
    }
    variance /= ENGRAM_VECTOR_DIM;
    
    return sqrtf(variance);
}

static neuron_t *find_or_create_neuron(engram_t *e, const engram_vec_t embedding, float threshold) {
    float best_sim = -1.0f;
    neuron_t *best_match = NULL;
    
    for (size_t i = 0; i < e->substrate.neuron_count; i++) {
        neuron_t *n = &e->substrate.neurons[i];
        float sim = vec_dot(embedding, n->embedding);
        if (sim > best_sim) {
            best_sim = sim;
            best_match = n;
        }
    }
    
    if (best_match && best_sim >= threshold) {
        return best_match;
    }
    
    neuron_t *n = substrate_alloc_neuron(&e->substrate);
    if (!n) return NULL;
    
    memcpy(n->embedding, embedding, sizeof(engram_vec_t));
    cortex_store(&e->cortex, n->id);
    
    if (best_match && best_sim >= threshold * 0.7f) {
        synapse_t *syn = substrate_alloc_synapse(&e->substrate);
        if (syn) {
            syn->source = n->id;
            syn->target = best_match->id;
            syn->weight = best_sim;
            syn->last_activation = e->substrate.tick;
        }
        syn = substrate_alloc_synapse(&e->substrate);
        if (syn) {
            syn->source = best_match->id;
            syn->target = n->id;
            syn->weight = best_sim * 0.5f;
            syn->last_activation = e->substrate.tick;
        }
    }
    
    return n;
}

engram_response_t engram_cue(engram_t *e, const char *input) {
    engram_response_t response = {0};
    
    if (!e || !input || !*input) {
        return response;
    }
    
    substrate_decay(&e->substrate, e->config.decay_rate);
    
    engram_vec_t query_embedding;
    wernicke_encode(&e->wernicke, input, query_embedding);
    
    float noise = compute_noise_score(query_embedding);
    if (noise < e->config.noise_threshold) {
        return response;
    }
    
    neuron_t *input_neuron = find_or_create_neuron(e, query_embedding, 
                                                    e->config.activation_threshold);
    if (!input_neuron) {
        return response;
    }
    
    propagate_activation(&e->substrate, input_neuron->id, 1.0f, 0.7f, 5);
    
    engram_id_t *result_ids = malloc(ENGRAM_MAX_ACTIVATIONS * sizeof(engram_id_t));
    float *result_scores = malloc(ENGRAM_MAX_ACTIVATIONS * sizeof(float));
    
    if (!result_ids || !result_scores) {
        free(result_ids);
        free(result_scores);
        return response;
    }
    
    size_t count = cortex_query(&e->cortex, &e->substrate, query_embedding,
                                 result_ids, result_scores, ENGRAM_MAX_ACTIVATIONS,
                                 0.3f);
    
    engram_id_t active_neurons[ENGRAM_MAX_ACTIVATIONS];
    size_t active_count = 0;
    
    active_neurons[active_count++] = input_neuron->id;
    
    for (size_t i = 0; i < count && active_count < ENGRAM_MAX_ACTIVATIONS; i++) {
        active_neurons[active_count++] = result_ids[i];
        hippocampus_record(&e->hippocampus, result_ids[i]);
    }
    
    propagate_learning(&e->substrate, active_neurons, active_count, e->config.learning_rate);
    
    hippocampus_consolidate(&e->hippocampus, &e->substrate, e->config.activation_threshold);
    
    if (e->substrate.synapse_count > e->config.max_synapses * 0.9) {
        substrate_prune(&e->substrate, e->config.noise_threshold);
    }
    
#ifdef ENGRAM_VULKAN_ENABLED
    if (e->gpu_available) {
        vulkan_sync_neurons(&e->vulkan, &e->substrate);
    }
#endif
    
    response.ids = result_ids;
    response.relevance = result_scores;
    response.count = count;
    
    return response;
}

void engram_response_free(engram_response_t *response) {
    if (!response) return;
    free(response->ids);
    free(response->relevance);
    response->ids = NULL;
    response->relevance = NULL;
    response->count = 0;
}

bool engram_gpu_available(engram_t *e) {
    return e ? e->gpu_available : false;
}

size_t engram_neuron_count(engram_t *e) {
    return e ? e->substrate.neuron_count : 0;
}

size_t engram_synapse_count(engram_t *e) {
    return e ? e->substrate.synapse_count : 0;
}
