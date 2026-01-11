#include "../internal.h"
#include "../core.h"
#include "../system.h"

#define AROUSAL_WAKE_MIN_TICKS    100
#define AROUSAL_DROWSY_TICKS      500
#define AROUSAL_SLEEP_TICKS       2000
#define AROUSAL_REM_TICKS         500
#define AROUSAL_ACTIVITY_THRESHOLD 0.1f

void arousal_auto_update(engram_t *eng) {
    if (!atomic_load(&eng->brainstem.auto_arousal)) {
        return;
    }

    engram_arousal_t current = atomic_load(&eng->brainstem.arousal_state);
    uint32_t ticks = eng->brainstem.ticks_since_arousal_change;
    uint32_t cycle = eng->config.arousal_cycle_ticks;

    float activity = (float)neuron_count_active(eng) / (float)eng->neurons.count;

    switch (current) {
        case ENGRAM_AROUSAL_WAKE:
            if (ticks > cycle && activity < AROUSAL_ACTIVITY_THRESHOLD) {
                arousal_set(eng, ENGRAM_AROUSAL_DROWSY);
            }
            break;

        case ENGRAM_AROUSAL_DROWSY:
            if (activity > AROUSAL_ACTIVITY_THRESHOLD * 2) {
                arousal_set(eng, ENGRAM_AROUSAL_WAKE);
            } else if (ticks > AROUSAL_DROWSY_TICKS) {
                arousal_set(eng, ENGRAM_AROUSAL_SLEEP);
            }
            break;

        case ENGRAM_AROUSAL_SLEEP:
            if (ticks > AROUSAL_SLEEP_TICKS) {
                arousal_set(eng, ENGRAM_AROUSAL_REM);
            }
            break;

        case ENGRAM_AROUSAL_REM:
            if (ticks > AROUSAL_REM_TICKS) {
                arousal_set(eng, ENGRAM_AROUSAL_WAKE);
            }
            break;
    }
}

int arousal_set(engram_t *eng, engram_arousal_t state) {
    atomic_store(&eng->brainstem.arousal_state, state);
    eng->brainstem.ticks_since_arousal_change = 0;
    return 0;
}

engram_arousal_t arousal_get(engram_t *eng) {
    return atomic_load(&eng->brainstem.arousal_state);
}
