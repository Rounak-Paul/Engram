#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

int main(void) {
    printf("Running synapse tests...\n");

    engram_config_t config = engram_config_minimal();
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    const char *cue1 = "first concept";
    const char *cue2 = "second concept";

    engram_cue_t cues[2] = {
        {
            .data = cue1,
            .size = 13,
            .intensity = 1.0f,
            .salience = 0.9f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = 0
        },
        {
            .data = cue2,
            .size = 14,
            .intensity = 1.0f,
            .salience = 0.9f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = 0
        }
    };

    int result = engram_associate(eng, cues, 2);
    TEST_ASSERT(result == 0, "engram_associate should succeed");

    engram_stats_t stats;
    engram_stats(eng, &stats);
    TEST_ASSERT(stats.synapse_count > 0, "synapses should be created");

    engram_destroy(eng);

    printf("PASS: All synapse tests passed\n");
    return 0;
}
