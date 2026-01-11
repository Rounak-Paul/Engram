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
    printf("Running consolidation tests...\n");

    engram_config_t config = engram_config_minimal();
    config.auto_arousal = 0;
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    const char *facts[] = {
        "Memory one",
        "Memory two",
        "Memory three"
    };

    for (int i = 0; i < 3; i++) {
        engram_cue_t cue = {
            .data = facts[i],
            .size = strlen(facts[i]),
            .intensity = 1.0f,
            .salience = 0.9f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = 0
        };
        engram_stimulate(eng, &cue);
        usleep(50000);
    }

    usleep(100000);

    int result = engram_set_arousal(eng, ENGRAM_AROUSAL_SLEEP);
    TEST_ASSERT(result == 0, "set arousal to sleep should succeed");

    engram_arousal_t state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_SLEEP, "arousal should be sleep");

    usleep(500000);

    engram_set_arousal(eng, ENGRAM_AROUSAL_WAKE);
    state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_WAKE, "arousal should be wake");

    engram_destroy(eng);

    printf("PASS: All consolidation tests passed\n");
    return 0;
}
