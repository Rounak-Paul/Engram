#include "../internal.h"
#include "../core.h"
#include "../processes.h"

void decay_step(engram_t *eng, uint64_t tick) {
    (void)tick;
    decay_synapses(eng, 1.0f);
    decay_activations(eng, 1.0f);
}

void decay_synapses(engram_t *eng, float dt) {
    float decay_amount = eng->config.decay_rate * dt;

    for (uint32_t i = 0; i < eng->synapses.capacity; i++) {
        engram_synapse_t *s = &eng->synapses.synapses[i];

        if (s->weight < 0.001f) {
            continue;
        }

        if (s->flags & ENGRAM_SYNAPSE_FLAG_LOCKED) {
            continue;
        }

        s->weight *= (1.0f - decay_amount);

        if (s->weight < 0.001f) {
            s->weight = 0.0f;
        }
    }
}

void decay_activations(engram_t *eng, float dt) {
    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        neuron_update(eng, i, dt);
    }
}
