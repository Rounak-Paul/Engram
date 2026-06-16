#include "internal.h"
#include <stdlib.h>
#include <string.h>

hippocampus_t hippocampus_create(size_t capacity, size_t trace_capacity) {
    hippocampus_t h = {0};
    h.recent_activations = calloc(capacity, sizeof(engram_id_t));
    h.recent_capacity = capacity;
    h.recent_count = 0;
    h.consolidation_threshold = 0.5f;
    h.traces = calloc(trace_capacity, sizeof(replay_trace_t));
    h.trace_capacity = trace_capacity;
    h.trace_count = 0;
    h.trace_head = 0;
    return h;
}

void hippocampus_destroy(hippocampus_t *h) {
    free(h->recent_activations);
    free(h->traces);
    memset(h, 0, sizeof(*h));
}

void hippocampus_record(hippocampus_t *h, engram_id_t id) {
    for (size_t i = 0; i < h->recent_count; i++) {
        if (h->recent_activations[i] == id) {
            for (size_t j = i; j > 0; j--) {
                h->recent_activations[j] = h->recent_activations[j - 1];
            }
            h->recent_activations[0] = id;
            return;
        }
    }

    if (h->recent_count < h->recent_capacity) {
        for (size_t i = h->recent_count; i > 0; i--) {
            h->recent_activations[i] = h->recent_activations[i - 1];
        }
        h->recent_activations[0] = id;
        h->recent_count++;
    } else {
        for (size_t i = h->recent_capacity - 1; i > 0; i--) {
            h->recent_activations[i] = h->recent_activations[i - 1];
        }
        h->recent_activations[0] = id;
    }
}

void hippocampus_imprint(hippocampus_t *h, const engram_id_t *members, size_t count, float salience) {
    if (count < 2 || h->trace_capacity == 0) return;
    if (count > ENGRAM_REPLAY_PATTERN_MAX) count = ENGRAM_REPLAY_PATTERN_MAX;

    replay_trace_t *t = &h->traces[h->trace_head];
    memcpy(t->members, members, count * sizeof(engram_id_t));
    t->member_count = (uint8_t)count;
    t->salience = salience;

    h->trace_head = (h->trace_head + 1) % h->trace_capacity;
    if (h->trace_count < h->trace_capacity) h->trace_count++;
}

void hippocampus_consolidate(hippocampus_t *h, substrate_t *s, float threshold) {
    for (size_t i = 0; i < h->recent_count; i++) {
        neuron_t *n = substrate_find_neuron(s, h->recent_activations[i]);
        if (n && n->importance > threshold) {
            n->importance *= 1.1f;
            if (n->importance > 10.0f) n->importance = 10.0f;
        }
    }
}

void hippocampus_replay(hippocampus_t *h, substrate_t *s, size_t passes, float rate) {
    if (h->trace_count == 0 || passes == 0) return;

    float saved_dopamine = s->dopamine;

    for (size_t pass = 0; pass < passes; pass++) {
        for (size_t i = 0; i < h->trace_count; i++) {
            replay_trace_t *t = &h->traces[i];
            if (t->member_count < 2) continue;

            s->dopamine = t->salience;
            for (uint8_t m = 0; m < t->member_count; m++) {
                neuron_t *n = substrate_find_neuron(s, t->members[m]);
                if (n) n->last_fire_tick = s->tick + m;
            }
            propagate_stdp(s, t->members, t->member_count, rate * 0.5f);
            s->tick++;
        }
    }

    s->dopamine = saved_dopamine;
}
