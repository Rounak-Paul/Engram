#include "internal.h"
#include "core.h"
#include "regions.h"
#include "processes.h"
#include "system.h"
#include <stdlib.h>
#include <string.h>

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

    if (neuron_pool_init(eng, config->neuron_count) != 0) {
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
    if (encoding_cue_to_sdr(eng, cue, neuron_ids, &count, 256) != 0) {
        return -1;
    }

    int retries = 50;
    while (engram_mutex_trylock(&eng->state_mutex) != 0 && retries > 0) {
        engram_sleep_us(500);
        retries--;
    }
    if (retries == 0) {
        return -1;
    }

    uint64_t tick = eng->brainstem.tick_count;

    for (uint32_t i = 0; i < count; i++) {
        neuron_stimulate(eng, neuron_ids[i], cue->intensity);
    }

    neuron_process_active(eng, tick, 128);

    uint32_t synapse_limit = (count < 16) ? count : 16;
    for (uint32_t i = 0; i < synapse_limit; i++) {
        for (uint32_t j = i + 1; j < synapse_limit; j++) {
            uint32_t existing = synapse_find(eng, neuron_ids[i], neuron_ids[j]);
            if (existing != UINT32_MAX) {
                engram_synapse_t *s = synapse_get(eng, existing);
                if (s) {
                    s->weight += 0.1f * cue->intensity;
                    if (s->weight > 2.0f) {
                        s->weight = 2.0f;
                    }
                    s->last_active_tick = (uint32_t)tick;
                }
            } else {
                synapse_create(eng, neuron_ids[i], neuron_ids[j], 0.2f * cue->intensity);
            }
        }
    }

    uint64_t content_hash = encoding_hash(cue->data, cue->size);
    uint32_t existing_pathway = pathway_find_by_hash(eng, content_hash);
    if (existing_pathway != UINT32_MAX) {
        pathway_activate(eng, existing_pathway, tick);
    } else {
        existing_pathway = pathway_find_matching(eng, neuron_ids, count, 0.6f);
        if (existing_pathway != UINT32_MAX) {
            pathway_activate(eng, existing_pathway, tick);
        } else if (count >= 3) {
            pathway_create_with_data(eng, neuron_ids, count, tick,
                                     cue->data, cue->size, cue->modality, content_hash);
        }
    }

    hippocampus_store_trace(eng, cue, neuron_ids, count, tick);

    engram_mutex_unlock(&eng->state_mutex);
    return 0;
}

int engram_recall(engram_t *eng, const engram_cue_t *cue, engram_recall_t *result) {
    if (!eng || !cue || !result) {
        return -1;
    }

    memset(result, 0, sizeof(engram_recall_t));

    uint64_t cue_hash = encoding_hash(cue->data, cue->size);

    uint32_t neuron_ids[256];
    uint32_t count = 0;

    if (encoding_cue_to_sdr(eng, cue, neuron_ids, &count, 256) != 0) {
        return -1;
    }

    int retries = 100;
    while (engram_mutex_trylock(&eng->state_mutex) != 0 && retries > 0) {
        engram_sleep_us(1000);
        retries--;
    }
    if (retries == 0) {
        return -1;
    }

    uint32_t pathway_id = pathway_find_by_hash(eng, cue_hash);

    if (pathway_id == UINT32_MAX) {
        pathway_id = neocortex_find_pathway(eng, neuron_ids, count);
    }

    if (pathway_id == UINT32_MAX) {
        pathway_id = pathway_find_matching(eng, neuron_ids, count, 0.1f);
    }

    if (pathway_id == UINT32_MAX) {
        memory_trace_t *trace = hippocampus_find_by_pattern(eng, neuron_ids, count, 0.1f);
        if (trace && trace->original_data) {
            if (trace->original_size > eng->recall_buffer.data_capacity) {
                void *new_buf = engram_realloc(eng, eng->recall_buffer.data, trace->original_size);
                if (new_buf) {
                    eng->recall_buffer.data = new_buf;
                    eng->recall_buffer.data_capacity = trace->original_size;
                }
            }
            if (eng->recall_buffer.data && trace->original_size <= eng->recall_buffer.data_capacity) {
                memcpy(eng->recall_buffer.data, trace->original_data, trace->original_size);
                result->data = eng->recall_buffer.data;
                result->data_size = trace->original_size;
            }
            result->confidence = trace->strength / 10.0f;
            if (result->confidence > 1.0f) result->confidence = 1.0f;
            result->familiarity = (float)trace->replay_count / 10.0f;
            if (result->familiarity > 1.0f) result->familiarity = 1.0f;
            result->modality = trace->modality;
            result->pathway_id = UINT32_MAX;
            result->age_ticks = (uint32_t)(eng->brainstem.tick_count - trace->created_tick);
            engram_mutex_unlock(&eng->state_mutex);
            return 0;
        }

        engram_mutex_unlock(&eng->state_mutex);
        result->confidence = 0.0f;
        return 1;
    }

    engram_pathway_t *p = pathway_get(eng, pathway_id);
    if (!p) {
        engram_mutex_unlock(&eng->state_mutex);
        return -1;
    }

    pathway_activate(eng, pathway_id, eng->brainstem.tick_count);

    if (p->original_data && p->original_size > 0) {
        if (p->original_size > eng->recall_buffer.data_capacity) {
            void *new_buf = engram_realloc(eng, eng->recall_buffer.data, p->original_size);
            if (new_buf) {
                eng->recall_buffer.data = new_buf;
                eng->recall_buffer.data_capacity = p->original_size;
            }
        }
        if (eng->recall_buffer.data && p->original_size <= eng->recall_buffer.data_capacity) {
            memcpy(eng->recall_buffer.data, p->original_data, p->original_size);
            result->data = eng->recall_buffer.data;
            result->data_size = p->original_size;
        }
    }

    size_t pattern_bytes = p->neuron_count * sizeof(float);
    if (pattern_bytes > eng->recall_buffer.pattern_capacity) {
        float *new_buf = engram_realloc(eng, eng->recall_buffer.pattern, pattern_bytes);
        if (new_buf) {
            eng->recall_buffer.pattern = new_buf;
            eng->recall_buffer.pattern_capacity = pattern_bytes;
        }
    }
    if (eng->recall_buffer.pattern && pattern_bytes <= eng->recall_buffer.pattern_capacity) {
        for (uint32_t i = 0; i < p->neuron_count; i++) {
            engram_neuron_t *n = neuron_get(eng, p->neuron_ids[i]);
            eng->recall_buffer.pattern[i] = n ? n->activation : 0.0f;
        }
        result->pattern = eng->recall_buffer.pattern;
        result->pattern_size = p->neuron_count;
    }

    result->confidence = p->strength / 10.0f;
    if (result->confidence > 1.0f) {
        result->confidence = 1.0f;
    }
    result->familiarity = (float)p->activation_count / 100.0f;
    if (result->familiarity > 1.0f) {
        result->familiarity = 1.0f;
    }
    result->pathway_id = pathway_id;
    result->modality = p->modality;
    result->age_ticks = (uint32_t)(eng->brainstem.tick_count - p->created_tick);

    engram_mutex_unlock(&eng->state_mutex);
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

    engram_mutex_lock(&eng->state_mutex);

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

    engram_mutex_unlock(&eng->state_mutex);
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

    engram_mutex_lock(&eng->state_mutex);

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

    engram_mutex_unlock(&eng->state_mutex);
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
