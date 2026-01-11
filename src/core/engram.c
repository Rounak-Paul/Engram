#include "internal.h"
#include <stdlib.h>
#include <string.h>

engram_config_t engram_config_default(void) {
    return (engram_config_t){
        .neuron_count = 65536,
        .synapse_pool_size = 1048576,
        .learning_rate = 0.01f,
        .decay_rate = 0.0001f,
        .inhibition_threshold = 0.5f,
        .activation_threshold = 0.3f,
        .hippocampus_tick_ms = 50,
        .consolidation_tick_ms = 1000,
        .use_vulkan = 1
    };
}

engram_t *engram_create(const engram_config_t *config) {
    engram_t *eng = calloc(1, sizeof(engram_t));
    if (!eng) return NULL;
    
    if (config) {
        eng->config = *config;
    } else {
        eng->config = engram_config_default();
    }
    
    engram_random_seed(engram_time_ms());
    
    engram_substrate_init(&eng->substrate, eng->config.neuron_count, eng->config.synapse_pool_size);
    
    engram_wernicke_init(&eng->wernicke);
    
    eng->cue_lock = engram_mutex_create();
    eng->response_capacity = 4096;
    eng->response_buffer = malloc(eng->response_capacity);
    
    atomic_store(&eng->alive, 1);
    atomic_store(&eng->tick, 0);
    
    engram_hippocampus_start(eng);
    engram_cortex_start(eng);
    
    return eng;
}

void engram_destroy(engram_t *eng) {
    if (!eng) return;
    
    atomic_store(&eng->alive, 0);
    
    engram_hippocampus_stop(eng);
    engram_cortex_stop(eng);
    
    engram_wernicke_free(&eng->wernicke);
    engram_substrate_free(&eng->substrate);
    
    engram_mutex_destroy(eng->cue_lock);
    free(eng->response_buffer);
    free(eng);
}

engram_response_t engram_cue(engram_t *eng, const char *input, size_t len) {
    engram_response_t resp = {0};
    
    if (!eng || !input || len == 0) return resp;
    
    engram_mutex_lock(eng->cue_lock);
    
    uint16_t *token_ids = NULL;
    uint32_t token_count = 0;
    engram_wernicke_tokenize(&eng->wernicke, input, len, &token_ids, &token_count);
    
    engram_pattern_t combined = {0};
    for (uint32_t i = 0; i < token_count; i++) {
        engram_pattern_t token_pattern;
        engram_wernicke_embed(&eng->wernicke, token_ids[i], &token_pattern);
        engram_pattern_add(&combined, &token_pattern, 1.0f / (float)token_count);
    }
    engram_pattern_normalize(&combined);
    
    engram_mutex_lock(eng->substrate.lock);
    engram_learn(eng, &combined);
    engram_mutex_unlock(eng->substrate.lock);
    
    engram_hippocampus_encode(eng, &combined);
    
    engram_pattern_t result;
    engram_mutex_lock(eng->substrate.lock);
    engram_recall(eng, &combined, &result);
    engram_mutex_unlock(eng->substrate.lock);
    
    float max_sim = 0.0f;
    uint16_t *best_tokens = NULL;
    uint32_t best_count = 0;
    
    engram_mutex_lock(eng->wernicke.vocab.lock);
    
    uint32_t response_capacity = 32;
    best_tokens = malloc(response_capacity * sizeof(uint16_t));
    
    for (uint32_t i = 0; i < eng->wernicke.vocab.count && best_count < response_capacity; i++) {
        engram_token_t *t = &eng->wernicke.vocab.tokens[i];
        float sim = engram_pattern_similarity(&result, &t->pattern);
        
        if (sim > 0.2f) {
            int already_in = 0;
            for (uint32_t k = 0; k < token_count; k++) {
                if (token_ids[k] == t->id) {
                    already_in = 1;
                    break;
                }
            }
            
            if (!already_in || sim > 0.5f) {
                best_tokens[best_count++] = t->id;
                if (sim > max_sim) max_sim = sim;
            }
        }
    }
    
    engram_mutex_unlock(eng->wernicke.vocab.lock);
    
    if (best_count > 0) {
        engram_wernicke_detokenize(&eng->wernicke, best_tokens, best_count,
                                    eng->response_buffer, eng->response_capacity);
        resp.text = strdup(eng->response_buffer);
        resp.text_len = strlen(resp.text);
        resp.confidence = max_sim;
        resp.novelty = 1.0f - max_sim;
    } else {
        resp.text = strdup(input);
        resp.text_len = len;
        resp.confidence = 0.0f;
        resp.novelty = 1.0f;
    }
    
    free(best_tokens);
    free(token_ids);
    engram_mutex_unlock(eng->cue_lock);
    
    return resp;
}

void engram_response_free(engram_response_t *resp) {
    if (resp && resp->text) {
        free(resp->text);
        resp->text = NULL;
        resp->text_len = 0;
    }
}

uint64_t engram_neuron_count(engram_t *eng) {
    return eng ? eng->substrate.neuron_count : 0;
}

uint64_t engram_synapse_count(engram_t *eng) {
    return eng ? eng->substrate.synapse_count : 0;
}

uint64_t engram_pathway_count(engram_t *eng) {
    return eng ? eng->substrate.pathway_count : 0;
}
