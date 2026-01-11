#include "internal.h"
#include "core.h"
#include <string.h>

static void active_queue_push(engram_t *eng, uint32_t neuron_id);

int neuron_pool_init(engram_t *eng, uint32_t capacity) {
    eng->neurons.neurons = engram_alloc(eng, capacity * sizeof(engram_neuron_t));
    if (!eng->neurons.neurons) {
        return -1;
    }
    memset(eng->neurons.neurons, 0, capacity * sizeof(engram_neuron_t));
    eng->neurons.capacity = capacity;
    eng->neurons.count = 0;

    for (uint32_t i = 0; i < capacity; i++) {
        eng->neurons.neurons[i].id = i;
        eng->neurons.neurons[i].threshold = eng->config.initial_threshold;
        eng->neurons.neurons[i].activation = 0.0f;
        eng->neurons.neurons[i].fatigue = 0.0f;
        eng->neurons.neurons[i].last_fired_tick = 0;
        eng->neurons.neurons[i].fire_count = 0;
        eng->neurons.neurons[i].outgoing_synapses = NULL;
        eng->neurons.neurons[i].outgoing_count = 0;
        eng->neurons.neurons[i].outgoing_capacity = 0;
        eng->neurons.neurons[i].cluster_id = i / eng->config.cluster_size;
        eng->neurons.neurons[i].refractory_remaining = 0;
        eng->neurons.neurons[i].flags = ENGRAM_NEURON_FLAG_NONE;
    }
    eng->neurons.count = capacity;

    uint32_t queue_cap = 4096;
    eng->active_queue.neuron_ids = engram_alloc(eng, queue_cap * sizeof(uint32_t));
    if (!eng->active_queue.neuron_ids) {
        engram_free(eng, eng->neurons.neurons);
        return -1;
    }
    eng->active_queue.capacity = queue_cap;
    eng->active_queue.head = 0;
    eng->active_queue.tail = 0;
    eng->active_queue.count = 0;

    return 0;
}

void neuron_pool_destroy(engram_t *eng) {
    if (eng->neurons.neurons) {
        for (uint32_t i = 0; i < eng->neurons.count; i++) {
            if (eng->neurons.neurons[i].outgoing_synapses) {
                engram_free(eng, eng->neurons.neurons[i].outgoing_synapses);
            }
        }
        engram_free(eng, eng->neurons.neurons);
        eng->neurons.neurons = NULL;
    }
    if (eng->active_queue.neuron_ids) {
        engram_free(eng, eng->active_queue.neuron_ids);
        eng->active_queue.neuron_ids = NULL;
    }
    eng->neurons.capacity = 0;
    eng->neurons.count = 0;
}

engram_neuron_t *neuron_get(engram_t *eng, uint32_t id) {
    if (id >= eng->neurons.count) {
        return NULL;
    }
    return &eng->neurons.neurons[id];
}

static void active_queue_push(engram_t *eng, uint32_t neuron_id) {
    if (eng->active_queue.count >= eng->active_queue.capacity) {
        return;
    }
    eng->active_queue.neuron_ids[eng->active_queue.tail] = neuron_id;
    eng->active_queue.tail = (eng->active_queue.tail + 1) % eng->active_queue.capacity;
    eng->active_queue.count++;
}

static int active_queue_pop(engram_t *eng, uint32_t *out_id) {
    if (eng->active_queue.count == 0) {
        return -1;
    }
    *out_id = eng->active_queue.neuron_ids[eng->active_queue.head];
    eng->active_queue.head = (eng->active_queue.head + 1) % eng->active_queue.capacity;
    eng->active_queue.count--;
    return 0;
}

int neuron_fire(engram_t *eng, uint32_t id, uint64_t tick) {
    engram_neuron_t *n = neuron_get(eng, id);
    if (!n) {
        return -1;
    }

    if (n->refractory_remaining > 0) {
        return 1;
    }

    if (n->activation < n->threshold) {
        return 1;
    }

    n->flags |= ENGRAM_NEURON_FLAG_ACTIVE;
    n->last_fired_tick = (uint32_t)tick;
    n->fire_count++;
    n->fatigue += eng->config.fatigue_rate;
    n->refractory_remaining = 2;

    if (n->fatigue > 1.0f) {
        n->fatigue = 1.0f;
    }

    for (uint16_t i = 0; i < n->outgoing_count; i++) {
        uint32_t syn_idx = n->outgoing_synapses[i];
        engram_synapse_t *s = synapse_get(eng, syn_idx);
        if (!s || s->weight < 0.001f) continue;
        
        engram_neuron_t *post = neuron_get(eng, s->post_neuron_id);
        if (post) {
            float signal = n->activation * s->weight;
            post->activation += signal * (1.0f - post->fatigue * 0.5f);
            if (post->activation > 1.0f) post->activation = 1.0f;
            
            if (post->activation >= post->threshold && post->refractory_remaining == 0) {
                active_queue_push(eng, s->post_neuron_id);
            }
            
            s->last_active_tick = (uint32_t)tick;
            s->eligibility_trace = 1.0f;
            
            if (post->flags & ENGRAM_NEURON_FLAG_ACTIVE) {
                s->weight += eng->config.learning_rate * s->eligibility_trace;
                if (s->weight > 1.0f) s->weight = 1.0f;
                s->potentiation_count++;
            }
        }
    }

    n->activation *= 0.5f;

    return 0;
}

void neuron_update(engram_t *eng, uint32_t id, float dt) {
    engram_neuron_t *n = neuron_get(eng, id);
    if (!n) {
        return;
    }

    if (n->refractory_remaining > 0) {
        n->refractory_remaining--;
        if (n->refractory_remaining == 0) {
            n->flags &= ~ENGRAM_NEURON_FLAG_REFRACTORY;
        }
    }

    if (!(n->flags & ENGRAM_NEURON_FLAG_ACTIVE)) {
        n->activation *= (1.0f - eng->config.decay_rate * dt);
        if (n->activation < 0.001f) {
            n->activation = 0.0f;
        }
    }

    n->fatigue *= (1.0f - eng->config.recovery_rate * dt);
    if (n->fatigue < 0.0f) {
        n->fatigue = 0.0f;
    }

    n->flags &= ~ENGRAM_NEURON_FLAG_ACTIVE;
}

void neuron_stimulate(engram_t *eng, uint32_t id, float input) {
    engram_neuron_t *n = neuron_get(eng, id);
    if (!n) {
        return;
    }

    float effective_input = input * (1.0f - n->fatigue * 0.5f);
    n->activation += effective_input;

    if (n->activation > 1.0f) {
        n->activation = 1.0f;
    }
    if (n->activation < 0.0f) {
        n->activation = 0.0f;
    }

    if (n->activation >= n->threshold && n->refractory_remaining == 0) {
        active_queue_push(eng, id);
    }
}

void neuron_process_active(engram_t *eng, uint64_t tick, uint32_t max_iterations) {
    uint32_t iterations = 0;
    uint32_t neuron_id;
    
    while (iterations < max_iterations && active_queue_pop(eng, &neuron_id) == 0) {
        neuron_fire(eng, neuron_id, tick);
        iterations++;
    }
}

uint32_t neuron_count_active(engram_t *eng) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        if (eng->neurons.neurons[i].activation > 0.1f) {
            count++;
        }
    }
    return count;
}
