#include "internal.h"
#include "core.h"
#include "regions.h"
#include "processes.h"
#include "system.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static engram_allocator_t default_allocator;

engram_config_t engram_config_default(void) {
    engram_config_t config = {
        .neuron_count = 10000,
        .cluster_size = 100,
        .initial_threshold = 0.5f,
        .decay_rate = 0.001f,
        .learning_rate = 0.01f,
        .fatigue_rate = 0.05f,
        .recovery_rate = 0.02f,
        .hippocampus_capacity = 1000,
        .working_memory_slots = 64,
        .consolidation_threshold = 1.5f,
        .replay_batch_size = 32,
        .resource_limits = {
            .max_cpu_percent = 5.0f,
            .max_memory_bytes = 50 * 1024 * 1024,
            .max_ticks_per_second = 100,
            .min_ticks_per_second = 1
        },
        .arousal_cycle_ticks = 1000,
        .auto_arousal = 1,
        .allocator = NULL
    };
    return config;
}

engram_config_t engram_config_minimal(void) {
    engram_config_t config = engram_config_default();
    config.neuron_count = 1000;
    config.cluster_size = 50;
    config.hippocampus_capacity = 100;
    config.working_memory_slots = 16;
    config.resource_limits.max_cpu_percent = 1.0f;
    config.resource_limits.max_memory_bytes = 1 * 1024 * 1024;
    config.resource_limits.max_ticks_per_second = 10;
    return config;
}

engram_config_t engram_config_standard(void) {
    return engram_config_default();
}

engram_config_t engram_config_performance(void) {
    engram_config_t config = engram_config_default();
    config.neuron_count = 1000000;
    config.cluster_size = 256;
    config.hippocampus_capacity = 10000;
    config.working_memory_slots = 256;
    config.resource_limits.max_cpu_percent = 25.0f;
    config.resource_limits.max_memory_bytes = 500 * 1024 * 1024;
    config.resource_limits.max_ticks_per_second = 1000;
    config.resource_limits.min_ticks_per_second = 10;
    return config;
}

engram_t *engram_create(const engram_config_t *config) {
    if (!config) {
        return NULL;
    }

    default_allocator = engram_allocator_default();
    engram_allocator_t *alloc = config->allocator ? config->allocator : &default_allocator;

    engram_t *eng = alloc->alloc(sizeof(engram_t), alloc->ctx);
    if (!eng) {
        return NULL;
    }
    memset(eng, 0, sizeof(engram_t));

    eng->config = *config;
    eng->allocator = *alloc;
    eng->total_memory_bytes = sizeof(engram_t);

    if (engram_mutex_init(&eng->state_mutex) != 0) {
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (engram_mutex_init(&eng->structure_mutex) != 0) {
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    for (int i = 0; i < ENGRAM_SHARD_COUNT; i++) {
        if (engram_rwlock_init(&eng->shard_locks[i]) != 0) {
            for (int j = 0; j < i; j++) {
                engram_rwlock_destroy(&eng->shard_locks[j]);
            }
            engram_mutex_destroy(&eng->structure_mutex);
            engram_mutex_destroy(&eng->state_mutex);
            alloc->free(eng, alloc->ctx);
            return NULL;
        }
    }

    if (neuron_pool_init(eng, config->neuron_count) != 0) {
        for (int i = 0; i < ENGRAM_SHARD_COUNT; i++) {
            engram_rwlock_destroy(&eng->shard_locks[i]);
        }
        engram_mutex_destroy(&eng->structure_mutex);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (synapse_pool_init(eng, ENGRAM_SYNAPSE_POOL_BLOCK) != 0) {
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (cluster_pool_init(eng) != 0) {
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (pathway_pool_init(eng) != 0) {
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (brainstem_init(eng) != 0) {
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (thalamus_init(eng) != 0) {
        brainstem_destroy(eng);
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (hippocampus_init(eng) != 0) {
        thalamus_destroy(eng);
        brainstem_destroy(eng);
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (amygdala_init(eng) != 0) {
        hippocampus_destroy(eng);
        thalamus_destroy(eng);
        brainstem_destroy(eng);
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (neocortex_init(eng) != 0) {
        amygdala_destroy(eng);
        hippocampus_destroy(eng);
        thalamus_destroy(eng);
        brainstem_destroy(eng);
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (working_memory_init(eng) != 0) {
        neocortex_destroy(eng);
        amygdala_destroy(eng);
        hippocampus_destroy(eng);
        thalamus_destroy(eng);
        brainstem_destroy(eng);
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    if (governor_init(eng) != 0) {
        working_memory_destroy(eng);
        neocortex_destroy(eng);
        amygdala_destroy(eng);
        hippocampus_destroy(eng);
        thalamus_destroy(eng);
        brainstem_destroy(eng);
        pathway_pool_destroy(eng);
        cluster_pool_destroy(eng);
        synapse_pool_destroy(eng);
        neuron_pool_destroy(eng);
        engram_mutex_destroy(&eng->state_mutex);
        alloc->free(eng, alloc->ctx);
        return NULL;
    }

    eng->recall_buffer.data = NULL;
    eng->recall_buffer.data_capacity = 0;
    eng->recall_buffer.pattern = NULL;
    eng->recall_buffer.pattern_capacity = 0;
    
    eng->word_memory.entries = NULL;
    eng->word_memory.capacity = 0;
    eng->word_memory.count = 0;
    eng->word_memory.hash_table = engram_alloc(eng, ENGRAM_WORD_HASH_SIZE * sizeof(uint32_t));
    if (eng->word_memory.hash_table) {
        for (uint32_t i = 0; i < ENGRAM_WORD_HASH_SIZE; i++) {
            eng->word_memory.hash_table[i] = ENGRAM_HASH_NONE;
        }
    }
    eng->word_memory.entry_pool = engram_alloc(eng, 4096 * sizeof(word_hash_entry_t));
    eng->word_memory.entry_pool_count = 0;
    eng->word_memory.entry_pool_capacity = eng->word_memory.entry_pool ? 4096 : 0;

    encoding_init();
    brainstem_start(eng);

    return eng;
}

void engram_destroy(engram_t *eng) {
    if (!eng) {
        return;
    }

    brainstem_stop(eng);

    if (eng->recall_buffer.data) {
        engram_free(eng, eng->recall_buffer.data);
    }
    if (eng->recall_buffer.pattern) {
        engram_free(eng, eng->recall_buffer.pattern);
    }
    if (eng->word_memory.entries) {
        engram_free(eng, eng->word_memory.entries);
    }
    if (eng->word_memory.hash_table) {
        engram_free(eng, eng->word_memory.hash_table);
    }
    if (eng->word_memory.entry_pool) {
        engram_free(eng, eng->word_memory.entry_pool);
    }

    governor_destroy(eng);
    working_memory_destroy(eng);
    neocortex_destroy(eng);
    amygdala_destroy(eng);
    hippocampus_destroy(eng);
    thalamus_destroy(eng);
    brainstem_destroy(eng);
    pathway_pool_destroy(eng);
    cluster_pool_destroy(eng);
    synapse_pool_destroy(eng);
    neuron_pool_destroy(eng);

    for (int i = 0; i < ENGRAM_SHARD_COUNT; i++) {
        engram_rwlock_destroy(&eng->shard_locks[i]);
    }
    engram_mutex_destroy(&eng->structure_mutex);
    engram_mutex_destroy(&eng->state_mutex);

    eng->allocator.free(eng, eng->allocator.ctx);
}

int engram_stimulate(engram_t *eng, const engram_cue_t *cue) {
    if (!eng || !cue) {
        return -1;
    }

    if (!thalamus_gate_cue(eng, cue)) {
        return 0;
    }

    uint32_t neuron_ids[256];
    uint32_t count = 0;
    
    if (cue->modality == ENGRAM_MODALITY_TEXT && cue->data && cue->size > 0) {
        if (encoding_text_to_primary_neurons(eng, (const char *)cue->data, cue->size, neuron_ids, &count, 256) != 0) {
            return -1;
        }
    } else {
        if (encoding_cue_to_sdr(eng, cue, neuron_ids, &count, 256) != 0) {
            return -1;
        }
    }

    uint8_t shards_needed[ENGRAM_SHARD_COUNT] = {0};
    for (uint32_t i = 0; i < count; i++) {
        shards_needed[engram_shard_for_neuron(neuron_ids[i])] = 1;
    }

    for (int s = 0; s < ENGRAM_SHARD_COUNT; s++) {
        if (shards_needed[s]) {
            engram_rwlock_wrlock(&eng->shard_locks[s]);
        }
    }

    uint64_t tick = eng->brainstem.tick_count;

    if (cue->modality == ENGRAM_MODALITY_TEXT && cue->data && cue->size > 0) {
        engram_mutex_lock(&eng->structure_mutex);
        word_memory_learn(eng, (const char *)cue->data, cue->size, (uint32_t)tick);
        engram_mutex_unlock(&eng->structure_mutex);
    }

    for (uint32_t i = 0; i < count; i++) {
        neuron_stimulate(eng, neuron_ids[i], cue->intensity);
    }

    engram_mutex_lock(&eng->structure_mutex);

    uint32_t synapse_limit = (count < 32) ? count : 32;
    for (uint32_t i = 0; i < synapse_limit; i++) {
        for (uint32_t j = i + 1; j < synapse_limit; j++) {
            uint32_t existing = synapse_find(eng, neuron_ids[i], neuron_ids[j]);
            if (existing != UINT32_MAX) {
                engram_synapse_t *s = synapse_get(eng, existing);
                if (s) {
                    s->weight += 0.1f * cue->intensity;
                    if (s->weight > 2.0f) s->weight = 2.0f;
                    s->last_active_tick = (uint32_t)tick;
                }
            } else {
                synapse_create(eng, neuron_ids[i], neuron_ids[j], 0.2f * cue->intensity);
            }
            
            uint32_t reverse = synapse_find(eng, neuron_ids[j], neuron_ids[i]);
            if (reverse != UINT32_MAX) {
                engram_synapse_t *s = synapse_get(eng, reverse);
                if (s) {
                    s->weight += 0.1f * cue->intensity;
                    if (s->weight > 2.0f) s->weight = 2.0f;
                    s->last_active_tick = (uint32_t)tick;
                }
            } else {
                synapse_create(eng, neuron_ids[j], neuron_ids[i], 0.2f * cue->intensity);
            }
        }
    }

    if (count >= 2) {
        uint64_t content_hash = cue->data ? encoding_hash(cue->data, cue->size) : 0;
        pathway_create_with_data(eng, neuron_ids, count, tick, cue->modality, content_hash);
    }

    engram_mutex_unlock(&eng->structure_mutex);

    for (int s = ENGRAM_SHARD_COUNT - 1; s >= 0; s--) {
        if (shards_needed[s]) {
            engram_rwlock_unlock(&eng->shard_locks[s]);
        }
    }

    return 0;
}

int engram_recall(engram_t *eng, const engram_cue_t *cue, engram_recall_t *result) {
    if (!eng || !cue || !result) {
        return -1;
    }

    memset(result, 0, sizeof(engram_recall_t));

    uint32_t cue_neurons[256];
    uint32_t cue_count = 0;

    if (cue->modality == ENGRAM_MODALITY_TEXT && cue->data && cue->size > 0) {
        if (encoding_text_to_primary_neurons(eng, (const char *)cue->data, cue->size, cue_neurons, &cue_count, 256) != 0) {
            return -1;
        }
    } else {
        if (encoding_cue_to_sdr(eng, cue, cue_neurons, &cue_count, 256) != 0) {
            return -1;
        }
    }

    if (cue_count == 0) {
        return 1;
    }

    for (int s = 0; s < ENGRAM_SHARD_COUNT; s++) {
        engram_rwlock_rdlock(&eng->shard_locks[s]);
    }

    float cue_weights[256];
    float max_cue_weight = 0.0f;
    for (uint32_t i = 0; i < cue_count; i++) {
        neuron_stimulate(eng, cue_neurons[i], 1.0f);
        engram_neuron_t *n = neuron_get(eng, cue_neurons[i]);
        if (n && n->outgoing_count > 0) {
            cue_weights[i] = 1.0f / (float)n->outgoing_count;
            if (cue_weights[i] > max_cue_weight) {
                max_cue_weight = cue_weights[i];
            }
        } else {
            cue_weights[i] = 0.0f;
        }
    }
    
    if (max_cue_weight > 0.0f) {
        for (uint32_t i = 0; i < cue_count; i++) {
            cue_weights[i] = cue_weights[i] / max_cue_weight;
            if (cue_weights[i] < 0.5f) {
                cue_weights[i] = 0.0f;
            }
        }
    }

    uint32_t activated[256];
    float activations[256];
    uint32_t cue_hits[256];
    uint32_t activated_count = 0;

    for (uint32_t i = 0; i < cue_count && activated_count < 256; i++) {
        engram_neuron_t *n = neuron_get(eng, cue_neurons[i]);
        if (!n) continue;
        float cue_weight = cue_weights[i];
        
        for (uint16_t j = 0; j < n->outgoing_count && activated_count < 256; j++) {
            uint32_t syn_idx = n->outgoing_synapses[j];
            engram_synapse_t *s = synapse_get(eng, syn_idx);
            if (!s || s->weight < 0.15f) continue;
            
            s->weight += 0.01f;
            if (s->weight > 2.0f) s->weight = 2.0f;
            
            uint32_t post_id = s->post_neuron_id;
            
            int is_cue = 0;
            for (uint32_t k = 0; k < cue_count; k++) {
                if (cue_neurons[k] == post_id) {
                    is_cue = 1;
                    break;
                }
            }
            if (is_cue) continue;
            
            int found = 0;
            for (uint32_t k = 0; k < activated_count; k++) {
                if (activated[k] == post_id) {
                    activations[k] += s->weight * cue_weight;
                    cue_hits[k]++;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                activated[activated_count] = post_id;
                activations[activated_count] = s->weight * cue_weight;
                cue_hits[activated_count] = 1;
                activated_count++;
            }
        }
    }
    
    float decay = 0.4f;
    float threshold = 0.01f;
    uint32_t spread_start = 0;
    uint32_t spread_end = activated_count;
    
    while (spread_start < spread_end && activated_count < 200) {
        uint32_t new_end = activated_count;
        
        for (uint32_t i = spread_start; i < spread_end && activated_count < 200; i++) {
            float act = activations[i] * decay;
            if (act < threshold) continue;
            
            engram_neuron_t *n = neuron_get(eng, activated[i]);
            if (!n) continue;
            
            float specificity = 1.0f / (1.0f + (float)n->outgoing_count * 0.1f);
            float spread_weight = act * specificity;
            
            if (spread_weight < threshold) continue;
            
            for (uint16_t j = 0; j < n->outgoing_count && activated_count < 200; j++) {
                uint32_t syn_idx = n->outgoing_synapses[j];
                engram_synapse_t *s = synapse_get(eng, syn_idx);
                if (!s || s->weight < 0.15f) continue;
                
                uint32_t post_id = s->post_neuron_id;
                
                int is_cue = 0;
                for (uint32_t k = 0; k < cue_count; k++) {
                    if (cue_neurons[k] == post_id) {
                        is_cue = 1;
                        break;
                    }
                }
                if (is_cue) continue;
                
                float contribution = s->weight * spread_weight;
                
                int found = 0;
                for (uint32_t k = 0; k < activated_count; k++) {
                    if (activated[k] == post_id) {
                        activations[k] += contribution;
                        cue_hits[k]++;
                        found = 1;
                        break;
                    }
                }
                if (!found && contribution > threshold) {
                    activated[activated_count] = post_id;
                    activations[activated_count] = contribution;
                    cue_hits[activated_count] = 1;
                    activated_count++;
                }
            }
        }
        
        spread_start = spread_end;
        spread_end = new_end;
        decay *= 0.6f;
        
        if (spread_start >= spread_end) break;
    }

    for (uint32_t i = 0; i < activated_count; i++) {
        if (cue_hits[i] >= 2) {
            activations[i] *= (float)cue_hits[i];
        }
    }
    
    uint32_t min_outgoing = UINT32_MAX;
    for (uint32_t i = 0; i < activated_count; i++) {
        engram_neuron_t *an = neuron_get(eng, activated[i]);
        if (an && an->outgoing_count > 0 && an->outgoing_count < min_outgoing) {
            min_outgoing = an->outgoing_count;
        }
    }
    
    if (min_outgoing < UINT32_MAX) {
        uint32_t max_allowed = min_outgoing * 3;
        for (uint32_t i = 0; i < activated_count; i++) {
            engram_neuron_t *an = neuron_get(eng, activated[i]);
            if (an && an->outgoing_count > max_allowed) {
                activations[i] = 0.0f;
            }
        }
    }

    for (uint32_t i = 0; i < activated_count - 1; i++) {
        for (uint32_t j = i + 1; j < activated_count; j++) {
            if (activations[j] > activations[i]) {
                uint32_t tmp_id = activated[i];
                float tmp_act = activations[i];
                activated[i] = activated[j];
                activations[i] = activations[j];
                activated[j] = tmp_id;
                activations[j] = tmp_act;
            }
        }
    }

    uint32_t filtered[48];
    float filtered_act[48];
    uint32_t filtered_count = 0;
    
    float max_activation = 0.0f;
    for (uint32_t i = 0; i < activated_count; i++) {
        if (activations[i] > max_activation) {
            max_activation = activations[i];
        }
    }
    
    float min_activation = max_activation * 0.1f;
    
    for (uint32_t i = 0; i < activated_count && filtered_count < 48; i++) {
        if (activations[i] < min_activation) break;
        filtered[filtered_count] = activated[i];
        filtered_act[filtered_count] = activations[i];
        filtered_count++;
    }
    
    if (1024 > eng->recall_buffer.data_capacity) {
        void *new_buf = engram_realloc(eng, eng->recall_buffer.data, 1024);
        if (new_buf) {
            eng->recall_buffer.data = new_buf;
            eng->recall_buffer.data_capacity = 1024;
        }
    }

    if (eng->recall_buffer.data && filtered_count > 0) {
        if (word_memory_reconstruct(eng, filtered, filtered_count, 
                                     (char *)eng->recall_buffer.data, 
                                     eng->recall_buffer.data_capacity) == 0) {
            result->data = eng->recall_buffer.data;
            result->data_size = strlen((char *)eng->recall_buffer.data) + 1;
            result->confidence = filtered_act[0];
            if (result->confidence > 1.0f) result->confidence = 1.0f;
            result->modality = ENGRAM_MODALITY_TEXT;
            result->pathway_id = UINT32_MAX;
            result->age_ticks = 0;
            
            uint64_t tick = eng->brainstem.tick_count;
            uint32_t learn_limit = filtered_count < 8 ? filtered_count : 8;
            for (uint32_t i = 0; i < cue_count && i < 8; i++) {
                for (uint32_t j = 0; j < learn_limit; j++) {
                    uint32_t existing = synapse_find(eng, cue_neurons[i], filtered[j]);
                    if (existing != UINT32_MAX) {
                        engram_synapse_t *s = synapse_get(eng, existing);
                        if (s) {
                            s->weight += 0.02f * filtered_act[j];
                            if (s->weight > 2.0f) s->weight = 2.0f;
                            s->last_active_tick = (uint32_t)tick;
                        }
                    } else if (filtered_act[j] > 0.3f) {
                        synapse_create(eng, cue_neurons[i], filtered[j], 0.1f * filtered_act[j]);
                    }
                }
            }
        }
    }

    for (int s = ENGRAM_SHARD_COUNT - 1; s >= 0; s--) {
        engram_rwlock_unlock(&eng->shard_locks[s]);
    }
    
    if (result->data_size == 0) {
        result->confidence = 0.0f;
        return 1;
    }
    
    return 0;
}

int engram_associate(engram_t *eng, const engram_cue_t *cues, size_t count) {
    if (!eng || !cues || count < 2) {
        return -1;
    }

    uint32_t all_neurons[512];
    uint32_t total = 0;

    for (size_t i = 0; i < count && total < 512; i++) {
        uint32_t neuron_ids[128];
        uint32_t neuron_count = 0;

        if (encoding_cue_to_sdr(eng, &cues[i], neuron_ids, &neuron_count, 128) == 0) {
            for (uint32_t j = 0; j < neuron_count && total < 512; j++) {
                all_neurons[total++] = neuron_ids[j];
            }
        }
    }

    if (total < 2) {
        return -1;
    }

    uint8_t shards_needed[ENGRAM_SHARD_COUNT] = {0};
    for (uint32_t i = 0; i < total; i++) {
        shards_needed[engram_shard_for_neuron(all_neurons[i])] = 1;
    }

    for (int s = 0; s < ENGRAM_SHARD_COUNT; s++) {
        if (shards_needed[s]) {
            engram_rwlock_wrlock(&eng->shard_locks[s]);
        }
    }
    engram_mutex_lock(&eng->structure_mutex);

    for (uint32_t i = 0; i < total - 1; i++) {
        for (uint32_t j = i + 1; j < total; j++) {
            uint32_t existing = synapse_find(eng, all_neurons[i], all_neurons[j]);
            if (existing == UINT32_MAX) {
                synapse_create(eng, all_neurons[i], all_neurons[j], 0.3f);
            } else {
                engram_synapse_t *s = synapse_get(eng, existing);
                if (s) {
                    s->weight += 0.1f;
                    if (s->weight > 2.0f) {
                        s->weight = 2.0f;
                    }
                }
            }
        }
    }

    pathway_create(eng, all_neurons, total, eng->brainstem.tick_count);

    engram_mutex_unlock(&eng->structure_mutex);
    for (int s = ENGRAM_SHARD_COUNT - 1; s >= 0; s--) {
        if (shards_needed[s]) {
            engram_rwlock_unlock(&eng->shard_locks[s]);
        }
    }
    return 0;
}

int engram_set_arousal(engram_t *eng, engram_arousal_t state) {
    if (!eng) {
        return -1;
    }
    return arousal_set(eng, state);
}

engram_arousal_t engram_get_arousal(engram_t *eng) {
    if (!eng) {
        return ENGRAM_AROUSAL_WAKE;
    }
    return arousal_get(eng);
}

int engram_set_arousal_auto(engram_t *eng, int enable) {
    if (!eng) {
        return -1;
    }
    atomic_store(&eng->brainstem.auto_arousal, enable);
    return 0;
}

int engram_set_resource_limits(engram_t *eng, const engram_resource_limits_t *limits) {
    if (!eng || !limits) {
        return -1;
    }
    governor_set_limits(eng, limits);
    return 0;
}

void engram_get_resource_usage(engram_t *eng, engram_resource_usage_t *usage) {
    if (!eng || !usage) {
        return;
    }
    usage->cpu_percent = eng->governor.current_cpu_percent;
    usage->memory_bytes = eng->governor.current_memory_bytes;
    usage->ticks_per_second = (uint32_t)eng->brainstem.current_tick_rate;
    usage->total_ticks = eng->brainstem.tick_count;
}

int engram_save(engram_t *eng, const char *path) {
    if (!eng || !path) {
        return -1;
    }
    return persistence_save(eng, path);
}

engram_t *engram_load(const char *path, const engram_config_t *config) {
    if (!path || !config) {
        return NULL;
    }
    return persistence_load(path, config);
}

int engram_checkpoint(engram_t *eng) {
    (void)eng;
    return 0;
}

void engram_stats(engram_t *eng, engram_stats_t *stats) {
    if (!eng || !stats) {
        return;
    }

    memset(stats, 0, sizeof(engram_stats_t));

    for (int s = 0; s < ENGRAM_SHARD_COUNT; s++) {
        engram_rwlock_rdlock(&eng->shard_locks[s]);
    }

    stats->tick_count = eng->brainstem.tick_count;
    stats->neuron_count = eng->neurons.count;
    stats->active_neuron_count = neuron_count_active(eng);
    stats->synapse_count = eng->synapses.count;
    stats->cluster_count = eng->clusters.count;
    stats->pathway_count = eng->pathways.count;

    float sum = 0.0f;
    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        sum += eng->neurons.neurons[i].activation;
    }
    stats->avg_activation = eng->neurons.count > 0 ? sum / eng->neurons.count : 0.0f;

    stats->memory_usage_bytes = (float)eng->total_memory_bytes;
    stats->cpu_usage_percent = eng->governor.current_cpu_percent;
    stats->current_tick_rate = eng->brainstem.current_tick_rate;
    stats->arousal_state = arousal_get(eng);

    for (int s = ENGRAM_SHARD_COUNT - 1; s >= 0; s--) {
        engram_rwlock_unlock(&eng->shard_locks[s]);
    }
}

size_t engram_pathway_count(engram_t *eng) {
    if (!eng) {
        return 0;
    }
    return eng->pathways.count;
}

float engram_memory_pressure(engram_t *eng) {
    if (!eng) {
        return 0.0f;
    }
    return (float)eng->total_memory_bytes / (float)eng->config.resource_limits.max_memory_bytes;
}

uint64_t engram_tick_count(engram_t *eng) {
    if (!eng) {
        return 0;
    }
    return eng->brainstem.tick_count;
}

float engram_current_tick_rate(engram_t *eng) {
    if (!eng) {
        return 0.0f;
    }
    return eng->brainstem.current_tick_rate;
}
