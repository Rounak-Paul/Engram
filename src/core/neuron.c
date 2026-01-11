#include "internal.h"
#include <string.h>

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
        eng->neurons.neurons[i].cluster_id = i / eng->config.cluster_size;
        eng->neurons.neurons[i].refractory_remaining = 0;
        eng->neurons.neurons[i].flags = ENGRAM_NEURON_FLAG_NONE;
    }
    eng->neurons.count = capacity;

    return 0;
}

void neuron_pool_destroy(engram_t *eng) {
    if (eng->neurons.neurons) {
        engram_free(eng, eng->neurons.neurons);
        eng->neurons.neurons = NULL;
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

void neuron_reset_activations(engram_t *eng) {
    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        eng->neurons.neurons[i].activation = 0.0f;
        eng->neurons.neurons[i].flags &= ~ENGRAM_NEURON_FLAG_ACTIVE;
    }
}
