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

        if (!governor_permits_tick(eng)) {
            engram_sleep_us(5000);
            continue;
        }

        engram_sleep_us(100);

        if (engram_mutex_trylock(&eng->state_mutex) != 0) {
            engram_sleep_us(1000);
            continue;
        }

        uint64_t tick = eng->brainstem.tick_count;
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

        engram_sleep_us(1000);
    }

    return NULL;
}

static void brainstem_tick_wake(engram_t *eng, uint64_t tick) {
    neuron_process_active(eng, tick, 32);
    
    plasticity_apply(eng, tick);
    
    if (tick % 50 == 0) {
        decay_step(eng, tick);
    }
    
    if (tick % 500 == 0) {
        synapse_prune_weak(eng, 0.005f);
    }
}

static void brainstem_tick_drowsy(engram_t *eng, uint64_t tick) {
    neuron_process_active(eng, tick, 64);
    
    plasticity_apply(eng, tick);

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

    neuron_process_active(eng, tick, 128);
}
