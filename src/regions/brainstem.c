#include "../internal.h"
#include "../core.h"
#include "../regions.h"
#include "../processes.h"
#include "../system.h"

static void brainstem_tick_wake(engram_t *eng, uint64_t tick);
static void brainstem_tick_drowsy(engram_t *eng, uint64_t tick);
static void brainstem_tick_sleep(engram_t *eng, uint64_t tick);
static void brainstem_tick_rem(engram_t *eng, uint64_t tick);

int brainstem_init(engram_t *eng) {
    atomic_store(&eng->brainstem.running, false);
    atomic_store(&eng->brainstem.arousal_state, ENGRAM_AROUSAL_WAKE);
    atomic_store(&eng->brainstem.auto_arousal, eng->config.auto_arousal);
    eng->brainstem.tick_count = 0;
    eng->brainstem.last_tick_time_ns = 0;
    eng->brainstem.current_tick_rate = 0.0f;
    eng->brainstem.ticks_since_arousal_change = 0;
    return 0;
}

void brainstem_destroy(engram_t *eng) {
    brainstem_stop(eng);
}

void brainstem_start(engram_t *eng) {
    if (atomic_load(&eng->brainstem.running)) {
        return;
    }
    atomic_store(&eng->brainstem.running, true);
    engram_thread_create(&eng->brainstem.thread, brainstem_thread_fn, eng);
}

void brainstem_stop(engram_t *eng) {
    if (!atomic_load(&eng->brainstem.running)) {
        return;
    }
    atomic_store(&eng->brainstem.running, false);
    engram_thread_join(&eng->brainstem.thread);
}

void *brainstem_thread_fn(void *arg) {
    engram_t *eng = (engram_t *)arg;
    uint64_t last_time = engram_time_now_ns();
    uint32_t tick_count_window = 0;
    uint64_t window_start = last_time;

    while (atomic_load(&eng->brainstem.running)) {
        uint64_t now = engram_time_now_ns();
        uint64_t tick = eng->brainstem.tick_count;

        if (!governor_permits_tick(eng)) {
            engram_sleep_us(eng->governor.target_tick_interval_us * 2);
            continue;
        }

        engram_mutex_lock(&eng->state_mutex);

        engram_arousal_t state = atomic_load(&eng->brainstem.arousal_state);
        switch (state) {
            case ENGRAM_AROUSAL_WAKE:
                brainstem_tick_wake(eng, tick);
                break;
            case ENGRAM_AROUSAL_DROWSY:
                brainstem_tick_drowsy(eng, tick);
                break;
            case ENGRAM_AROUSAL_SLEEP:
                brainstem_tick_sleep(eng, tick);
                break;
            case ENGRAM_AROUSAL_REM:
                brainstem_tick_rem(eng, tick);
                break;
        }

        eng->brainstem.tick_count++;
        eng->brainstem.ticks_since_arousal_change++;
        eng->brainstem.last_tick_time_ns = now;

        engram_mutex_unlock(&eng->state_mutex);

        tick_count_window++;
        uint64_t elapsed = now - window_start;
        if (elapsed >= 1000000000ULL) {
            eng->brainstem.current_tick_rate = (float)tick_count_window * 1000000000.0f / (float)elapsed;
            tick_count_window = 0;
            window_start = now;
        }

        if (atomic_load(&eng->brainstem.auto_arousal)) {
            arousal_auto_update(eng);
        }

        governor_update(eng);

        uint64_t work_time = engram_time_now_ns() - now;
        uint64_t target_ns = (uint64_t)eng->governor.target_tick_interval_us * 1000ULL;
        if (work_time < target_ns) {
            engram_sleep_us((uint32_t)((target_ns - work_time) / 1000ULL));
        }
    }

    return NULL;
}

static void brainstem_tick_wake(engram_t *eng, uint64_t tick) {
    cue_queue_process(eng, tick);

    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        engram_neuron_t *n = &eng->neurons.neurons[i];
        if (n->activation >= n->threshold) {
            neuron_fire(eng, i, tick);
        }
    }

    cluster_propagate_all(eng, tick);
    plasticity_apply(eng, tick);

    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        neuron_update(eng, i, 1.0f);
    }
}

static void brainstem_tick_drowsy(engram_t *eng, uint64_t tick) {
    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        neuron_update(eng, i, 2.0f);
    }

    if (eng->brainstem.ticks_since_arousal_change % 10 == 0) {
        consolidation_step(eng, tick);
    }
}

static void brainstem_tick_sleep(engram_t *eng, uint64_t tick) {
    replay_step(eng, tick);
    consolidation_step(eng, tick);
    decay_step(eng, tick);

    if (eng->brainstem.ticks_since_arousal_change % 100 == 0) {
        synapse_prune_weak(eng, 0.01f);
        pathway_prune_weak(eng, 0.1f);
    }
}

static void brainstem_tick_rem(engram_t *eng, uint64_t tick) {
    replay_step(eng, tick);

    if (eng->brainstem.ticks_since_arousal_change % 5 == 0) {
        uint32_t random_cluster = (uint32_t)(tick % eng->clusters.count);
        for (uint32_t i = 0; i < 3; i++) {
            uint32_t nid = eng->clusters.clusters[random_cluster].neuron_start + (i % eng->clusters.clusters[random_cluster].neuron_count);
            neuron_stimulate(eng, nid, 0.3f);
        }
    }

    cluster_propagate_all(eng, tick);
    plasticity_apply(eng, tick);
}
