#include "../internal.h"
#include "../core.h"
#include "../system.h"
#include "../platform.h"

#define GOVERNOR_SAMPLE_INTERVAL_TICKS 100
#define GOVERNOR_THROTTLE_STEP 0.1f
#define GOVERNOR_MIN_THROTTLE 0.1f
#define GOVERNOR_MAX_THROTTLE 1.0f

int governor_init(engram_t *eng) {
    eng->governor.limits = eng->config.resource_limits;
    eng->governor.current_cpu_percent = 0.0f;
    eng->governor.current_memory_bytes = 0;
    eng->governor.throttle_factor = 1.0f;

    uint32_t target_tps = eng->governor.limits.max_ticks_per_second;
    if (target_tps == 0) {
        target_tps = 100;
    }
    eng->governor.target_tick_interval_us = 1000000 / target_tps;

    return 0;
}

void governor_destroy(engram_t *eng) {
    (void)eng;
}

void governor_update(engram_t *eng) {
    if (eng->brainstem.tick_count % GOVERNOR_SAMPLE_INTERVAL_TICKS != 0) {
        return;
    }

    eng->governor.current_cpu_percent = engram_cpu_usage_percent();
    eng->governor.current_memory_bytes = eng->total_memory_bytes;

    float max_cpu = eng->governor.limits.max_cpu_percent;
    if (max_cpu < 1.0f) {
        max_cpu = 1.0f;
    }

    if (eng->governor.current_cpu_percent > max_cpu) {
        eng->governor.throttle_factor -= GOVERNOR_THROTTLE_STEP;
        if (eng->governor.throttle_factor < GOVERNOR_MIN_THROTTLE) {
            eng->governor.throttle_factor = GOVERNOR_MIN_THROTTLE;
        }
    } else if (eng->governor.current_cpu_percent < max_cpu * 0.5f) {
        eng->governor.throttle_factor += GOVERNOR_THROTTLE_STEP * 0.5f;
        if (eng->governor.throttle_factor > GOVERNOR_MAX_THROTTLE) {
            eng->governor.throttle_factor = GOVERNOR_MAX_THROTTLE;
        }
    }

    float throttled_interval = (float)eng->governor.target_tick_interval_us / eng->governor.throttle_factor;
    eng->governor.target_tick_interval_us = (uint32_t)throttled_interval;

    uint32_t min_interval = 1000000 / eng->governor.limits.max_ticks_per_second;
    uint32_t max_interval = 1000000 / eng->governor.limits.min_ticks_per_second;

    if (eng->governor.target_tick_interval_us < min_interval) {
        eng->governor.target_tick_interval_us = min_interval;
    }
    if (eng->governor.target_tick_interval_us > max_interval) {
        eng->governor.target_tick_interval_us = max_interval;
    }

    if (eng->governor.current_memory_bytes > eng->governor.limits.max_memory_bytes * 9 / 10) {
        synapse_prune_weak(eng, 0.05f);
        pathway_prune_weak(eng, 0.2f);
    }
}

int governor_permits_tick(engram_t *eng) {
    if (eng->governor.current_cpu_percent > eng->governor.limits.max_cpu_percent * 1.5f) {
        return 0;
    }

    if (eng->governor.current_memory_bytes > eng->governor.limits.max_memory_bytes) {
        return 0;
    }

    return 1;
}

void governor_set_limits(engram_t *eng, const engram_resource_limits_t *limits) {
    eng->governor.limits = *limits;

    if (limits->max_ticks_per_second > 0) {
        eng->governor.target_tick_interval_us = 1000000 / limits->max_ticks_per_second;
    }
}

void governor_get_usage(engram_t *eng, engram_resource_usage_t *usage) {
    usage->cpu_percent = eng->governor.current_cpu_percent;
    usage->memory_bytes = eng->governor.current_memory_bytes;
    usage->ticks_per_second = (uint32_t)eng->brainstem.current_tick_rate;
    usage->total_ticks = eng->brainstem.tick_count;
}
