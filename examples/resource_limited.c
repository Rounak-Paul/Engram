#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    printf("Engram Resource-Limited Example\n");
    printf("================================\n\n");

    engram_config_t config = engram_config_minimal();

    config.resource_limits.max_cpu_percent = 1.0f;
    config.resource_limits.max_memory_bytes = 512 * 1024;
    config.resource_limits.max_ticks_per_second = 20;
    config.resource_limits.min_ticks_per_second = 1;

    printf("Configuration:\n");
    printf("  Neurons: %u\n", config.neuron_count);
    printf("  Max CPU: %.1f%%\n", config.resource_limits.max_cpu_percent);
    printf("  Max Memory: %zu KB\n", config.resource_limits.max_memory_bytes / 1024);
    printf("  Max TPS: %u\n", config.resource_limits.max_ticks_per_second);
    printf("\n");

    engram_t *brain = engram_create(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }

    printf("Brain created. Monitoring resource usage...\n\n");

    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 10; i++) {
            char data[32];
            snprintf(data, sizeof(data), "event_%d_%d", round, i);

            engram_cue_t cue = {
                .data = data,
                .size = strlen(data),
                .intensity = 1.0f,
                .salience = 0.5f,
                .modality = ENGRAM_MODALITY_TEXT,
                .flags = 0
            };
            engram_stimulate(brain, &cue);
        }

        usleep(500000);

        engram_resource_usage_t usage;
        engram_get_resource_usage(brain, &usage);

        float pressure = engram_memory_pressure(brain);

        printf("Round %d: CPU=%.2f%%, Memory=%zu bytes, TPS=%u, Pressure=%.2f\n",
               round + 1,
               usage.cpu_percent,
               usage.memory_bytes,
               usage.ticks_per_second,
               pressure);
    }

    printf("\nDynamically adjusting limits...\n");
    engram_resource_limits_t new_limits = {
        .max_cpu_percent = 2.0f,
        .max_memory_bytes = 1024 * 1024,
        .max_ticks_per_second = 50,
        .min_ticks_per_second = 5
    };
    engram_set_resource_limits(brain, &new_limits);
    printf("New limits: CPU=%.1f%%, Memory=%zu KB, TPS=%u\n\n",
           new_limits.max_cpu_percent,
           new_limits.max_memory_bytes / 1024,
           new_limits.max_ticks_per_second);

    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 20; i++) {
            char data[32];
            snprintf(data, sizeof(data), "burst_%d_%d", round, i);

            engram_cue_t cue = {
                .data = data,
                .size = strlen(data),
                .intensity = 1.0f,
                .salience = 0.8f,
                .modality = ENGRAM_MODALITY_TEXT,
                .flags = 0
            };
            engram_stimulate(brain, &cue);
        }

        usleep(300000);

        engram_resource_usage_t usage;
        engram_get_resource_usage(brain, &usage);

        printf("Burst %d: CPU=%.2f%%, Memory=%zu bytes, TPS=%u\n",
               round + 1,
               usage.cpu_percent,
               usage.memory_bytes,
               usage.ticks_per_second);
    }

    engram_stats_t stats;
    engram_stats(brain, &stats);
    printf("\nFinal statistics:\n");
    printf("  Total ticks: %llu\n", stats.tick_count);
    printf("  Pathways: %u\n", stats.pathway_count);
    printf("  Synapses: %u\n", stats.synapse_count);
    printf("  Memory: %.2f KB\n", stats.memory_usage_bytes / 1024.0f);

    engram_destroy(brain);
    printf("\nDone.\n");

    return 0;
}
