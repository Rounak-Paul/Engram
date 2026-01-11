#include "../internal.h"
#include "../core.h"
#include "../regions.h"
#include "../processes.h"

void replay_step(engram_t *eng, uint64_t tick) {
    memory_trace_t **traces = NULL;
    uint32_t count = 0;

    if (hippocampus_get_replay_batch(eng, &traces, &count, eng->config.replay_batch_size) != 0) {
        return;
    }

    if (count == 0 || !traces) {
        return;
    }

    for (uint32_t t = 0; t < count; t++) {
        memory_trace_t *trace = traces[t];
        if (!trace || !trace->neuron_ids) {
            continue;
        }

        replay_activate_trace(eng, trace->neuron_ids, trace->neuron_count, 0.5f);

        for (uint32_t i = 0; i < trace->neuron_count - 1; i++) {
            for (uint32_t j = i + 1; j < trace->neuron_count; j++) {
                uint32_t syn_idx = synapse_find(eng, trace->neuron_ids[i], trace->neuron_ids[j]);
                if (syn_idx != UINT32_MAX) {
                    engram_synapse_t *s = synapse_get(eng, syn_idx);
                    if (s) {
                        s->weight += 0.02f;
                        s->last_active_tick = (uint32_t)tick;
                        if (s->weight > 2.0f) {
                            s->weight = 2.0f;
                        }
                    }
                } else {
                    uint32_t new_syn = synapse_create(eng, trace->neuron_ids[i], trace->neuron_ids[j], 0.3f);
                    (void)new_syn;
                }
            }
        }

        uint32_t existing = pathway_find_by_hash(eng, trace->content_hash);
        if (existing == UINT32_MAX) {
            existing = pathway_find_matching(eng, trace->neuron_ids, trace->neuron_count, 0.5f);
        }

        if (existing != UINT32_MAX) {
            pathway_activate(eng, existing, tick);
        } else if (trace->neuron_count >= 3) {
            pathway_create_with_data(eng, trace->neuron_ids, trace->neuron_count, tick,
                                     trace->modality, trace->content_hash);
        }

        hippocampus_mark_replayed(eng, trace);
    }

    engram_free(eng, traces);
}

void replay_activate_trace(engram_t *eng, uint32_t *neuron_ids, uint32_t count, float intensity) {
    for (uint32_t i = 0; i < count; i++) {
        neuron_stimulate(eng, neuron_ids[i], intensity);
    }
    neuron_process_active(eng, eng->brainstem.tick_count, 64);
}

int working_memory_init(engram_t *eng) {
    uint32_t capacity = eng->config.working_memory_slots;

    eng->working_memory.neuron_ids = engram_alloc(eng, capacity * sizeof(uint32_t));
    if (!eng->working_memory.neuron_ids) {
        return -1;
    }

    eng->working_memory.activations = engram_alloc(eng, capacity * sizeof(float));
    if (!eng->working_memory.activations) {
        engram_free(eng, eng->working_memory.neuron_ids);
        return -1;
    }

    eng->working_memory.capacity = capacity;
    eng->working_memory.head = 0;
    eng->working_memory.count = 0;

    return 0;
}

void working_memory_destroy(engram_t *eng) {
    if (eng->working_memory.neuron_ids) {
        engram_free(eng, eng->working_memory.neuron_ids);
    }
    if (eng->working_memory.activations) {
        engram_free(eng, eng->working_memory.activations);
    }
}
