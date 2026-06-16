#include "internal.h"
#include <string.h>
#include <math.h>

typedef struct {
    substrate_t *s;
    float input;
    float decay;
    size_t depth;
    engram_id_t *fired;
    size_t *fired_count;
    size_t fired_cap;
} propagate_ctx_t;

static void integrate_and_fire(propagate_ctx_t *p, engram_id_t target, float charge);

static void propagate_synapse(synapse_t *syn, void *ctx) {
    propagate_ctx_t *p = ctx;
    if (syn->weight <= 0.01f) return;

    float charge = p->input * syn->weight * p->decay * (float)syn->sign;
    syn->last_activation = p->s->tick;

    propagate_ctx_t child = *p;
    child.input = fabsf(charge);
    child.depth = p->depth - 1;
    integrate_and_fire(&child, syn->target, charge);
}

static void integrate_and_fire(propagate_ctx_t *p, engram_id_t target, float charge) {
    if (p->depth == 0 || p->input < 0.01f) return;

    neuron_t *n = substrate_find_neuron(p->s, target);
    if (!n) return;

    if (p->s->tick < n->refractory_until) return;

    n->potential += charge;
    if (n->potential < 0.0f) n->potential = 0.0f;

    n->last_access = p->s->tick;
    n->access_count++;

    if (n->potential < n->threshold) return;

    n->activation = n->potential;
    if (n->activation > 1.0f) n->activation = 1.0f;
    n->potential = 0.0f;
    n->refractory_until = p->s->tick + ENGRAM_DEFAULT_REFRACTORY;
    n->last_fire_tick = p->s->tick;
    n->importance += n->activation * 0.1f;

    if (*p->fired_count < p->fired_cap) {
        p->fired[(*p->fired_count)++] = target;
    }

    propagate_ctx_t fan = *p;
    fan.input = n->activation;
    fan.depth = p->depth - 1;
    substrate_for_each_synapse(p->s, target, propagate_synapse, &fan);
}

size_t propagate_activation(substrate_t *s, engram_id_t source, float input, float decay,
                            size_t depth, engram_id_t *fired, size_t fired_cap) {
    size_t fired_count = 0;

    neuron_t *src = substrate_find_neuron(s, source);
    if (!src) return 0;

    propagate_ctx_t ctx = {
        .s = s, .input = input, .decay = decay, .depth = depth,
        .fired = fired, .fired_count = &fired_count, .fired_cap = fired_cap
    };

    src->potential = src->threshold;
    integrate_and_fire(&ctx, source, input);

    return fired_count;
}

void propagate_stdp(substrate_t *s, const engram_id_t *fired, size_t count, float rate) {
    float gate = s->dopamine;
    if (gate < 0.05f) gate = 0.05f;

    for (size_t i = 0; i < count; i++) {
        neuron_t *a = substrate_find_neuron(s, fired[i]);
        if (!a) continue;

        for (size_t j = 0; j < count; j++) {
            if (i == j) continue;
            neuron_t *b = substrate_find_neuron(s, fired[j]);
            if (!b) continue;

            int64_t dt = (int64_t)b->last_fire_tick - (int64_t)a->last_fire_tick;
            if (dt == 0) dt = (i < j) ? 1 : -1;

            float window = (float)ENGRAM_DEFAULT_STDP_WINDOW;
            float magnitude = expf(-fabsf((float)dt) / window);

            if (dt > 0) {
                synapse_t *syn = substrate_add_synapse(s, fired[i], fired[j], 0.0f, 1);
                if (syn) {
                    float delta = rate * gate * magnitude * syn->plasticity;
                    syn->weight += delta * (1.0f - syn->weight);
                    if (syn->weight > 1.0f) syn->weight = 1.0f;
                    syn->last_activation = s->tick;
                }
            } else {
                synapse_t *syn = substrate_find_synapse(s, fired[i], fired[j]);
                if (syn && syn->sign > 0) {
                    float delta = rate * gate * magnitude * syn->plasticity * 0.5f;
                    syn->weight -= delta;
                    if (syn->weight < 0.0f) syn->weight = 0.0f;
                }
            }
        }
    }
}

void substrate_modulate(substrate_t *s, float novelty) {
    if (novelty < 0.0f) novelty = 0.0f;
    if (novelty > 1.0f) novelty = 1.0f;

    s->dopamine += (novelty - s->dopamine) * 0.5f;
    s->acetylcholine += ((1.0f - novelty) - s->acetylcholine) * 0.2f;
}
