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
                                     trace->original_data, trace->original_size,
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

    cluster_propagate_all(eng, eng->brainstem.tick_count);
}

int cue_queue_init(engram_t *eng) {
    uint32_t capacity = 256;

    eng->cue_queue.cues = engram_alloc(eng, capacity * sizeof(pending_cue_t));
    if (!eng->cue_queue.cues) {
        return -1;
    }

    eng->cue_queue.capacity = capacity;
    eng->cue_queue.head = 0;
    eng->cue_queue.tail = 0;

    if (engram_mutex_init(&eng->cue_queue.mutex) != 0) {
        engram_free(eng, eng->cue_queue.cues);
        return -1;
    }

    return 0;
}

void cue_queue_destroy(engram_t *eng) {
    engram_mutex_lock(&eng->cue_queue.mutex);

    while (eng->cue_queue.head != eng->cue_queue.tail) {
        pending_cue_t *c = &eng->cue_queue.cues[eng->cue_queue.head];
        if (c->data) {
            engram_free(eng, c->data);
        }
        eng->cue_queue.head = (eng->cue_queue.head + 1) % eng->cue_queue.capacity;
    }

    if (eng->cue_queue.cues) {
        engram_free(eng, eng->cue_queue.cues);
        eng->cue_queue.cues = NULL;
    }

    engram_mutex_unlock(&eng->cue_queue.mutex);
    engram_mutex_destroy(&eng->cue_queue.mutex);
}

int cue_queue_push(engram_t *eng, const engram_cue_t *cue) {
    engram_mutex_lock(&eng->cue_queue.mutex);

    uint32_t next_tail = (eng->cue_queue.tail + 1) % eng->cue_queue.capacity;
    if (next_tail == eng->cue_queue.head) {
        engram_mutex_unlock(&eng->cue_queue.mutex);
        return -1;
    }

    pending_cue_t *c = &eng->cue_queue.cues[eng->cue_queue.tail];
    c->data = engram_alloc(eng, cue->size);
    if (!c->data) {
        engram_mutex_unlock(&eng->cue_queue.mutex);
        return -1;
    }

    for (size_t i = 0; i < cue->size; i++) {
        ((uint8_t *)c->data)[i] = ((const uint8_t *)cue->data)[i];
    }
    c->size = cue->size;
    c->intensity = cue->intensity;
    c->salience = cue->salience;
    c->modality = cue->modality;
    c->flags = cue->flags;

    eng->cue_queue.tail = next_tail;

    engram_mutex_unlock(&eng->cue_queue.mutex);
    return 0;
}

int cue_queue_process(engram_t *eng, uint64_t tick) {
    engram_mutex_lock(&eng->cue_queue.mutex);

    if (eng->cue_queue.head == eng->cue_queue.tail) {
        engram_mutex_unlock(&eng->cue_queue.mutex);
        return 0;
    }

    pending_cue_t c = eng->cue_queue.cues[eng->cue_queue.head];
    eng->cue_queue.head = (eng->cue_queue.head + 1) % eng->cue_queue.capacity;

    engram_mutex_unlock(&eng->cue_queue.mutex);

    engram_cue_t cue = {
        .data = c.data,
        .size = c.size,
        .intensity = c.intensity,
        .salience = c.salience,
        .modality = c.modality,
        .flags = c.flags
    };

    if (!thalamus_gate_cue(eng, &cue)) {
        engram_free(eng, c.data);
        return 0;
    }

    uint32_t neuron_ids[256];
    uint32_t count = 0;
    encoding_cue_to_sdr(eng, &cue, neuron_ids, &count, 256);

    for (uint32_t i = 0; i < count; i++) {
        neuron_stimulate(eng, neuron_ids[i], cue.intensity);
    }

    hippocampus_store_trace(eng, &cue, neuron_ids, count, tick);

    for (uint32_t i = 0; i < count; i++) {
        working_memory_add(eng, neuron_ids[i], cue.intensity);
    }

    engram_free(eng, c.data);
    return 1;
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

void working_memory_add(engram_t *eng, uint32_t neuron_id, float activation) {
    uint32_t idx = eng->working_memory.head;
    eng->working_memory.neuron_ids[idx] = neuron_id;
    eng->working_memory.activations[idx] = activation;
    eng->working_memory.head = (eng->working_memory.head + 1) % eng->working_memory.capacity;

    if (eng->working_memory.count < eng->working_memory.capacity) {
        eng->working_memory.count++;
    }
}

void working_memory_decay(engram_t *eng) {
    for (uint32_t i = 0; i < eng->working_memory.count; i++) {
        eng->working_memory.activations[i] *= 0.95f;
    }
}
