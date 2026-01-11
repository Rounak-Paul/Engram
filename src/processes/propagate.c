#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

void engram_propagate(engram_t *eng, const engram_pattern_t *input, engram_pattern_t *output) {
    engram_substrate_t *s = &eng->substrate;
    
    memset(output, 0, sizeof(*output));
    
    for (uint32_t i = 0; i < input->active_count; i++) {
        uint32_t neuron_id = input->active_indices[i] % s->neuron_count;
        float activation = input->values[input->active_indices[i]];
        atomic_store(&s->neurons[neuron_id].activation, activation);
    }
    
    for (uint32_t iter = 0; iter < 3; iter++) {
        for (uint32_t i = 0; i < s->synapse_count; i++) {
            engram_synapse_t *syn = &s->synapses[i];
            float pre_act = atomic_load(&s->neurons[syn->pre].activation);
            float weight = atomic_load(&syn->weight);
            
            if (pre_act > s->neurons[syn->pre].threshold) {
                float post_act = atomic_load(&s->neurons[syn->post].activation);
                post_act += pre_act * weight;
                if (post_act > 1.0f) post_act = 1.0f;
                atomic_store(&s->neurons[syn->post].activation, post_act);
            }
        }
        
        for (uint32_t c = 0; c < s->cluster_count; c++) {
            engram_cluster_t *cluster = &s->clusters[c];
            float max_act = 0.0f;
            
            for (uint32_t n = 0; n < cluster->neuron_count; n++) {
                float act = atomic_load(&cluster->neurons[n].activation);
                if (act > max_act) max_act = act;
            }
            
            if (max_act > 0.0f) {
                for (uint32_t n = 0; n < cluster->neuron_count; n++) {
                    float act = atomic_load(&cluster->neurons[n].activation);
                    act -= (max_act - act) * cluster->lateral_inhibition;
                    if (act < 0.0f) act = 0.0f;
                    atomic_store(&cluster->neurons[n].activation, act);
                }
            }
        }
    }
    
    output->active_count = 0;
    float sum = 0.0f;
    
    for (uint32_t i = 0; i < s->neuron_count && i < ENGRAM_PATTERN_DIM; i++) {
        float act = atomic_load(&s->neurons[i].activation);
        output->values[i] = act;
        sum += act * act;
        
        if (act > eng->config.activation_threshold && output->active_count < 64) {
            output->active_indices[output->active_count++] = i;
        }
    }
    
    output->magnitude = sqrtf(sum);
}

void engram_learn(engram_t *eng, const engram_pattern_t *pattern) {
    engram_substrate_t *s = &eng->substrate;
    float lr = eng->config.learning_rate;
    
    for (uint32_t i = 0; i + 1 < pattern->active_count; i++) {
        uint32_t pre = pattern->active_indices[i] % s->neuron_count;
        uint32_t post = pattern->active_indices[i + 1] % s->neuron_count;
        
        int found = 0;
        for (uint32_t j = 0; j < s->synapse_count; j++) {
            if (s->synapses[j].pre == pre && s->synapses[j].post == post) {
                engram_synapse_strengthen(&s->synapses[j], lr);
                found = 1;
                break;
            }
        }
        
        if (!found) {
            engram_synapse_create(s, pre, post, lr);
        }
    }
    
    if (pattern->active_count > 2) {
        uint32_t *neurons = malloc(pattern->active_count * sizeof(uint32_t));
        for (uint32_t i = 0; i < pattern->active_count; i++) {
            neurons[i] = pattern->active_indices[i] % s->neuron_count;
        }
        engram_pathway_activate(s, neurons, pattern->active_count, 0.5f);
        free(neurons);
    }
}

void engram_recall(engram_t *eng, const engram_pattern_t *cue, engram_pattern_t *result) {
    engram_substrate_t *s = &eng->substrate;
    
    float best_match = 0.0f;
    engram_pathway_t *best_pathway = NULL;
    
    for (uint32_t i = 0; i < s->pathway_count; i++) {
        engram_pathway_t *p = &s->pathways[i];
        
        uint32_t overlap = 0;
        for (uint32_t j = 0; j < cue->active_count; j++) {
            uint32_t cue_neuron = cue->active_indices[j] % s->neuron_count;
            for (uint32_t k = 0; k < p->neuron_count; k++) {
                if (p->neuron_ids[k] == cue_neuron) {
                    overlap++;
                    break;
                }
            }
        }
        
        float match = (float)overlap / (float)(cue->active_count + 1) * p->strength;
        if (match > best_match) {
            best_match = match;
            best_pathway = p;
        }
    }
    
    memset(result, 0, sizeof(*result));
    
    if (best_pathway) {
        best_pathway->access_count++;
        engram_pathway_strengthen(best_pathway, 0.05f);
        
        for (uint32_t i = 0; i < best_pathway->neuron_count && i < 64; i++) {
            uint32_t nid = best_pathway->neuron_ids[i];
            if (nid < ENGRAM_PATTERN_DIM) {
                result->values[nid] = best_pathway->strength;
                result->active_indices[result->active_count++] = nid;
            }
        }
    }
    
    engram_propagate(eng, cue, result);
    engram_pattern_normalize(result);
}

void engram_decay_tick(engram_t *eng) {
    engram_substrate_t *s = &eng->substrate;
    float decay = eng->config.decay_rate;
    
    for (uint32_t i = 0; i < s->synapse_count; i++) {
        engram_synapse_weaken(&s->synapses[i], decay);
    }
    
    for (uint32_t i = 0; i < s->pathway_count; i++) {
        engram_pathway_decay(&s->pathways[i], decay);
    }
    
    for (uint32_t i = 0; i < s->neuron_count; i++) {
        float act = atomic_load(&s->neurons[i].activation);
        act *= (1.0f - decay * 2.0f);
        atomic_store(&s->neurons[i].activation, act);
    }
}

void engram_consolidate(engram_t *eng) {
    engram_substrate_t *s = &eng->substrate;
    
    for (uint32_t i = 0; i < s->pathway_count; i++) {
        engram_pathway_t *p = &s->pathways[i];
        
        if (p->access_count > 5) {
            for (uint32_t j = 0; j + 1 < p->neuron_count; j++) {
                for (uint32_t k = 0; k < s->synapse_count; k++) {
                    if (s->synapses[k].pre == p->neuron_ids[j] &&
                        s->synapses[k].post == p->neuron_ids[j + 1]) {
                        float w = atomic_load(&s->synapses[k].weight);
                        w = w * 0.9f + p->strength * 0.1f;
                        if (w > 1.0f) w = 1.0f;
                        atomic_store(&s->synapses[k].weight, w);
                    }
                }
            }
        }
    }
}
