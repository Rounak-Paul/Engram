#ifndef ENGRAM_CONFIG_H
#define ENGRAM_CONFIG_H

#include <stdint.h>
#include <stddef.h>
#include "allocator.h"

typedef struct engram_resource_limits {
    float max_cpu_percent;
    size_t max_memory_bytes;
    uint32_t max_ticks_per_second;
    uint32_t min_ticks_per_second;
} engram_resource_limits_t;

typedef struct engram_config {
    uint32_t neuron_count;
    uint32_t cluster_size;

    float initial_threshold;
    float decay_rate;
    float learning_rate;
    float fatigue_rate;
    float recovery_rate;

    uint32_t hippocampus_capacity;
    uint32_t working_memory_slots;

    float consolidation_threshold;
    uint32_t replay_batch_size;

    engram_resource_limits_t resource_limits;

    uint32_t arousal_cycle_ticks;
    int auto_arousal;

    engram_allocator_t *allocator;
} engram_config_t;

engram_config_t engram_config_default(void);
engram_config_t engram_config_minimal(void);
engram_config_t engram_config_standard(void);
engram_config_t engram_config_performance(void);

#endif
