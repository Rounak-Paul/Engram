#include "../internal.h"
#include "../core.h"
#include "../regions.h"
#include "../processes.h"
#include <string.h>

int hippocampus_init(engram_t *eng) {
    uint32_t capacity = eng->config.hippocampus_capacity;

    eng->hippocampus.traces = engram_alloc(eng, capacity * sizeof(memory_trace_t));
    if (!eng->hippocampus.traces) {
        return -1;
    }
    memset(eng->hippocampus.traces, 0, capacity * sizeof(memory_trace_t));

    eng->hippocampus.capacity = capacity;
    eng->hippocampus.count = 0;
    eng->hippocampus.head = 0;

    return 0;
}

void hippocampus_destroy(engram_t *eng) {
    if (!eng->hippocampus.traces) {
        return;
    }

    for (uint32_t i = 0; i < eng->hippocampus.capacity; i++) {
        memory_trace_t *trace = &eng->hippocampus.traces[i];
        if (trace->neuron_ids) {
            engram_free(eng, trace->neuron_ids);
        }
        if (trace->original_data) {
            engram_free(eng, trace->original_data);
        }
    }

    engram_free(eng, eng->hippocampus.traces);
    eng->hippocampus.traces = NULL;
    eng->hippocampus.capacity = 0;
    eng->hippocampus.count = 0;
}

int hippocampus_store_trace(engram_t *eng, const engram_cue_t *cue, uint32_t *neuron_ids, uint32_t count, uint64_t tick) {
    uint64_t content_hash = encoding_hash(cue->data, cue->size);
    
    for (uint32_t i = 0; i < eng->hippocampus.count; i++) {
        if (eng->hippocampus.traces[i].content_hash == content_hash) {
            eng->hippocampus.traces[i].strength += cue->salience;
            if (eng->hippocampus.traces[i].strength > 10.0f) {
                eng->hippocampus.traces[i].strength = 10.0f;
            }
            return 0;
        }
    }

    uint32_t idx;
    if (eng->hippocampus.count >= eng->hippocampus.capacity) {
        uint32_t oldest_idx = 0;
        float min_strength = eng->hippocampus.traces[0].strength;
        
        for (uint32_t i = 1; i < eng->hippocampus.capacity; i++) {
            if (eng->hippocampus.traces[i].strength < min_strength) {
                min_strength = eng->hippocampus.traces[i].strength;
                oldest_idx = i;
            }
        }
        
        memory_trace_t *old = &eng->hippocampus.traces[oldest_idx];
        if (old->neuron_ids) {
            engram_free(eng, old->neuron_ids);
        }
        if (old->original_data) {
            engram_free(eng, old->original_data);
        }
        memset(old, 0, sizeof(memory_trace_t));
        idx = oldest_idx;
    } else {
        idx = eng->hippocampus.count;
    }

    memory_trace_t *trace = &eng->hippocampus.traces[idx];

    trace->neuron_ids = engram_alloc(eng, count * sizeof(uint32_t));
    if (!trace->neuron_ids) {
        return -1;
    }
    memcpy(trace->neuron_ids, neuron_ids, count * sizeof(uint32_t));
    trace->neuron_count = count;

    trace->original_data = engram_alloc(eng, cue->size);
    if (!trace->original_data) {
        engram_free(eng, trace->neuron_ids);
        trace->neuron_ids = NULL;
        return -1;
    }
    memcpy(trace->original_data, cue->data, cue->size);
    trace->original_size = cue->size;
    trace->modality = cue->modality;

    trace->content_hash = content_hash;
    trace->strength = cue->salience;
    trace->created_tick = (uint32_t)tick;
    trace->replay_count = 0;

    if (eng->hippocampus.count < eng->hippocampus.capacity) {
        eng->hippocampus.count++;
    }

    eng->hippocampus.head = idx;
    return 0;
}

memory_trace_t *hippocampus_find_by_hash(engram_t *eng, uint64_t content_hash) {
    for (uint32_t i = 0; i < eng->hippocampus.count; i++) {
        if (eng->hippocampus.traces[i].content_hash == content_hash) {
            return &eng->hippocampus.traces[i];
        }
    }
    return NULL;
}

memory_trace_t *hippocampus_find_by_pattern(engram_t *eng, uint32_t *neuron_ids, uint32_t count, float threshold) {
    memory_trace_t *best_match = NULL;
    float best_overlap = threshold;

    for (uint32_t i = 0; i < eng->hippocampus.count; i++) {
        memory_trace_t *trace = &eng->hippocampus.traces[i];
        if (!trace->neuron_ids || trace->neuron_count == 0) {
            continue;
        }

        uint32_t matches = 0;
        for (uint32_t j = 0; j < count; j++) {
            for (uint32_t k = 0; k < trace->neuron_count; k++) {
                if (neuron_ids[j] == trace->neuron_ids[k]) {
                    matches++;
                    break;
                }
            }
        }

        uint32_t max_count = count > trace->neuron_count ? count : trace->neuron_count;
        float overlap = (float)matches / (float)max_count;

        if (overlap > best_overlap) {
            best_overlap = overlap;
            best_match = trace;
        }
    }

    return best_match;
}

int hippocampus_get_replay_batch(engram_t *eng, memory_trace_t ***out_traces, uint32_t *out_count, uint32_t batch_size) {
    if (eng->hippocampus.count == 0) {
        *out_traces = NULL;
        *out_count = 0;
        return 0;
    }

    uint32_t actual_count = batch_size;
    if (actual_count > eng->hippocampus.count) {
        actual_count = eng->hippocampus.count;
    }

    memory_trace_t **trace_ptrs = engram_alloc(eng, actual_count * sizeof(memory_trace_t *));
    if (!trace_ptrs) {
        *out_traces = NULL;
        *out_count = 0;
        return -1;
    }

    uint32_t *indices = engram_alloc(eng, eng->hippocampus.count * sizeof(uint32_t));
    float *scores = engram_alloc(eng, eng->hippocampus.count * sizeof(float));
    if (!indices || !scores) {
        if (indices) engram_free(eng, indices);
        if (scores) engram_free(eng, scores);
        engram_free(eng, trace_ptrs);
        *out_traces = NULL;
        *out_count = 0;
        return -1;
    }

    for (uint32_t i = 0; i < eng->hippocampus.count; i++) {
        indices[i] = i;
        memory_trace_t *t = &eng->hippocampus.traces[i];
        float tick_diff = (float)(eng->brainstem.tick_count - t->created_tick);
        float recency = 1.0f / (1.0f + tick_diff / 1000.0f);
        float novelty = 1.0f / (1.0f + (float)t->replay_count);
        scores[i] = t->strength * (0.5f * recency + 0.5f * novelty);
    }

    for (uint32_t i = 0; i < actual_count; i++) {
        uint32_t max_idx = i;
        for (uint32_t j = i + 1; j < eng->hippocampus.count; j++) {
            if (scores[indices[j]] > scores[indices[max_idx]]) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            uint32_t tmp = indices[i];
            indices[i] = indices[max_idx];
            indices[max_idx] = tmp;
        }
        trace_ptrs[i] = &eng->hippocampus.traces[indices[i]];
    }

    engram_free(eng, indices);
    engram_free(eng, scores);
    *out_traces = trace_ptrs;
    *out_count = actual_count;
    return 0;
}

void hippocampus_mark_replayed(engram_t *eng, memory_trace_t *trace) {
    (void)eng;
    trace->replay_count++;
    trace->strength *= 1.1f;
    if (trace->strength > 10.0f) {
        trace->strength = 10.0f;
    }
}
