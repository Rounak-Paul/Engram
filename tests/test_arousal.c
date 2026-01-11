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
    printf("Running arousal tests...\n");

    engram_config_t config = engram_config_minimal();
    config.auto_arousal = 0;
    engram_t *eng = engram_create(&config);
    TEST_ASSERT(eng != NULL, "engram_create should succeed");

    engram_arousal_t state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_WAKE, "initial state should be wake");

    int result = engram_set_arousal(eng, ENGRAM_AROUSAL_DROWSY);
    TEST_ASSERT(result == 0, "set arousal to drowsy should succeed");
    state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_DROWSY, "state should be drowsy");

    result = engram_set_arousal(eng, ENGRAM_AROUSAL_SLEEP);
    TEST_ASSERT(result == 0, "set arousal to sleep should succeed");
    state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_SLEEP, "state should be sleep");

    result = engram_set_arousal(eng, ENGRAM_AROUSAL_REM);
    TEST_ASSERT(result == 0, "set arousal to REM should succeed");
    state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_REM, "state should be REM");

    result = engram_set_arousal(eng, ENGRAM_AROUSAL_WAKE);
    TEST_ASSERT(result == 0, "set arousal to wake should succeed");
    state = engram_get_arousal(eng);
    TEST_ASSERT(state == ENGRAM_AROUSAL_WAKE, "state should be wake");

    result = engram_set_arousal_auto(eng, 1);
    TEST_ASSERT(result == 0, "enable auto arousal should succeed");

    usleep(100000);

    engram_destroy(eng);

    printf("PASS: All arousal tests passed\n");
    return 0;
}
