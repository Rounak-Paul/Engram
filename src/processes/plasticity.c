#include "../internal.h"
#include "../core.h"
#include "../processes.h"

#define STDP_A_PLUS  0.01f
#define STDP_A_MINUS 0.012f
#define STDP_TAU_PLUS 20.0f
#define STDP_TAU_MINUS 20.0f

static float exp_approx(float x);

void plasticity_apply(engram_t *eng, uint64_t tick) {
    (void)tick;
    for (uint32_t i = 0; i < eng->synapses.capacity; i++) {
        engram_synapse_t *s = &eng->synapses.synapses[i];
        if (s->weight < 0.001f) {
            continue;
        }

        if (s->flags & ENGRAM_SYNAPSE_FLAG_LOCKED) {
            continue;
        }

        engram_neuron_t *pre = neuron_get(eng, s->pre_neuron_id);
        engram_neuron_t *post = neuron_get(eng, s->post_neuron_id);
        if (!pre || !post) {
            continue;
        }

        int pre_fired = (pre->flags & ENGRAM_NEURON_FLAG_ACTIVE) != 0;
        int post_fired = (post->flags & ENGRAM_NEURON_FLAG_ACTIVE) != 0;

        if (pre_fired || post_fired) {
            int32_t timing_diff = (int32_t)post->last_fired_tick - (int32_t)pre->last_fired_tick;
            plasticity_stdp(eng, i, timing_diff, s->eligibility_trace);
        }

        synapse_update_eligibility(eng, i, 1.0f);
    }
}

void plasticity_stdp(engram_t *eng, uint32_t synapse_idx, int32_t timing_diff, float eligibility) {
    engram_synapse_t *s = synapse_get(eng, synapse_idx);
    if (!s) {
        return;
    }

    float dw = 0.0f;
    float base_eligibility = eligibility > 0.1f ? eligibility : 0.1f;

    if (timing_diff > 0 && timing_diff < 100) {
        dw = STDP_A_PLUS * exp_approx(-(float)timing_diff / STDP_TAU_PLUS);
    } else if (timing_diff < 0 && timing_diff > -100) {
        dw = -STDP_A_MINUS * exp_approx((float)timing_diff / STDP_TAU_MINUS);
    }

    dw *= base_eligibility * eng->config.learning_rate;
    s->weight += dw;

    if (s->weight < 0.0f) {
        s->weight = 0.0f;
    }
    if (s->weight > 2.0f) {
        s->weight = 2.0f;
    }

    if (dw > 0) {
        s->potentiation_count++;
    }
}

static float exp_approx(float x) {
    if (x < -10.0f) {
        return 0.0f;
    }
    if (x > 10.0f) {
        return 22026.0f;
    }

    float result = 1.0f + x;
    float term = x;
    for (int i = 2; i < 8; i++) {
        term *= x / (float)i;
        result += term;
    }
    return result;
}

void plasticity_create_connection(engram_t *eng, uint32_t pre_id, uint32_t post_id, uint64_t tick) {
    uint32_t existing = synapse_find(eng, pre_id, post_id);
    if (existing != UINT32_MAX) {
        engram_synapse_t *s = synapse_get(eng, existing);
        if (s) {
            s->weight += 0.1f;
            if (s->weight > 2.0f) {
                s->weight = 2.0f;
            }
            s->last_active_tick = (uint32_t)tick;
        }
        return;
    }

    synapse_create(eng, pre_id, post_id, 0.2f);
}
