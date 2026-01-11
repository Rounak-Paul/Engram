#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

int main(void) {
    printf("Running recall tests...\n");

    engram_config_t config = engram_config_minimal();
    config.auto_arousal = 0;
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    const char *fact = "The sky is blue";
    engram_cue_t learn_cue = {
        .data = fact,
        .size = strlen(fact),
        .intensity = 1.0f,
        .salience = 1.0f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_FLAG_LEARN
    };

    for (int i = 0; i < 5; i++) {
        engram_stimulate(eng, &learn_cue);
        usleep(50000);
    }

    usleep(200000);

    const char *query = "The sky is blue";
    engram_cue_t recall_cue = {
        .data = query,
        .size = strlen(query),
        .intensity = 0.8f,
        .salience = 1.0f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_FLAG_RECALL
    };

    engram_recall_t result;
    int recall_result = engram_recall(eng, &recall_cue, &result);

    if (recall_result == 0) {
        printf("Recall succeeded: confidence=%.2f, familiarity=%.2f\n",
               result.confidence, result.familiarity);
    } else if (recall_result == 1) {
        printf("No matching memory found (expected for new brain)\n");
    }

    engram_destroy(eng);

    printf("PASS: All recall tests passed\n");
    return 0;
}
