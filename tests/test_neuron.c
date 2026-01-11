#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

int main(void) {
    printf("Running neuron tests...\n");

    engram_config_t config = engram_config_minimal();
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    engram_stats_t stats;
    engram_stats(eng, &stats);
    TEST_ASSERT(stats.neuron_count == config.neuron_count, "neuron count should match config");

    const char *cue_data = "test neuron";
    engram_cue_t cue = {
        .data = cue_data,
        .size = 11,
        .intensity = 1.0f,
        .salience = 0.8f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = 0
    };

    int result = engram_stimulate(eng, &cue);
    TEST_ASSERT(result == 0, "engram_stimulate should succeed");

    usleep(100000);

    engram_stats(eng, &stats);
    TEST_ASSERT(stats.tick_count > 0, "ticks should be running");

    engram_destroy(eng);

    printf("PASS: All neuron tests passed\n");
    return 0;
}
