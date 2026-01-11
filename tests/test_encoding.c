#include <engram/engram.h>
#include <stdio.h>
#include <string.h>

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s\n", msg); \
        return 1; \
    } \
} while(0)

int main(void) {
    printf("Running encoding tests...\n");

    engram_config_t config = engram_config_minimal();
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    const char *text1 = "The quick brown fox";
    const char *text2 = "The quick brown fox";
    const char *text3 = "Something completely different";

    engram_cue_t cue1 = {
        .data = text1,
        .size = strlen(text1),
        .intensity = 1.0f,
        .salience = 0.8f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = 0
    };

    engram_cue_t cue2 = {
        .data = text2,
        .size = strlen(text2),
        .intensity = 1.0f,
        .salience = 0.8f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = 0
    };

    engram_cue_t cue3 = {
        .data = text3,
        .size = strlen(text3),
        .intensity = 1.0f,
        .salience = 0.8f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = 0
    };

    int result1 = engram_stimulate(eng, &cue1);
    int result2 = engram_stimulate(eng, &cue2);
    int result3 = engram_stimulate(eng, &cue3);

    TEST_ASSERT(result1 == 0, "stimulate cue1 should succeed");
    TEST_ASSERT(result2 == 0, "stimulate cue2 should succeed");
    TEST_ASSERT(result3 == 0, "stimulate cue3 should succeed");

    engram_destroy(eng);

    printf("PASS: All encoding tests passed\n");
    return 0;
}
