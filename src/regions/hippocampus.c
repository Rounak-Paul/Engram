#include "internal.h"
#include <stdlib.h>
#include <string.h>

hippocampus_t hippocampus_create(size_t capacity) {
    hippocampus_t h = {0};
    h.recent_activations = calloc(capacity, sizeof(engram_id_t));
    h.recent_capacity = capacity;
    h.recent_count = 0;
    h.consolidation_threshold = 0.5f;
    return h;
}

void hippocampus_destroy(hippocampus_t *h) {
    free(h->recent_activations);
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

void hippocampus_consolidate(hippocampus_t *h, substrate_t *s, float threshold) {
    for (size_t i = 0; i < h->recent_count; i++) {
        neuron_t *n = substrate_find_neuron(s, h->recent_activations[i]);
        if (n && n->importance > threshold) {
            n->importance *= 1.1f;
            if (n->importance > 10.0f) n->importance = 10.0f;
        }
    }
}
