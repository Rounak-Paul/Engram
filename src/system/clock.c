#include "../internal.h"
#include "../system.h"

uint64_t clock_get_tick(engram_t *eng) {
    return eng->brainstem.tick_count;
}

float clock_get_rate(engram_t *eng) {
    return eng->brainstem.current_tick_rate;
}

uint64_t clock_elapsed_ticks(engram_t *eng, uint64_t from_tick) {
    return eng->brainstem.tick_count - from_tick;
}

float clock_ticks_to_seconds(engram_t *eng, uint64_t ticks) {
    if (eng->brainstem.current_tick_rate < 0.001f) {
        return 0.0f;
    }
    return (float)ticks / eng->brainstem.current_tick_rate;
}
