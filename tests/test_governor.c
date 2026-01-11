#include <engram/engram.h>
#include <stdio.h>
#include <unistd.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

int main(void) {
    printf("Running governor tests...\n");

    engram_config_t config = engram_config_minimal();
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    engram_resource_limits_t limits = {
        .max_cpu_percent = 2.0f,
        .max_memory_bytes = 2 * 1024 * 1024,
        .max_ticks_per_second = 50,
        .min_ticks_per_second = 1
    };

    int result = engram_set_resource_limits(eng, &limits);
    TEST_ASSERT(result == 0, "set resource limits should succeed");

    usleep(200000);

    engram_resource_usage_t usage;
    engram_get_resource_usage(eng, &usage);

    printf("Resource usage: CPU=%.2f%%, Memory=%zu bytes, TPS=%u, Total ticks=%llu\n",
           usage.cpu_percent, usage.memory_bytes, usage.ticks_per_second, usage.total_ticks);

    TEST_ASSERT(usage.total_ticks > 0, "should have ticked");

    float pressure = engram_memory_pressure(eng);
    TEST_ASSERT(pressure >= 0.0f && pressure <= 10.0f, "memory pressure should be reasonable");

    uint64_t ticks = engram_tick_count(eng);
    TEST_ASSERT(ticks > 0, "tick count should be positive");

    float rate = engram_current_tick_rate(eng);
    printf("Current tick rate: %.2f ticks/sec\n", rate);

    engram_destroy(eng);

    printf("PASS: All governor tests passed\n");
    return 0;
}
