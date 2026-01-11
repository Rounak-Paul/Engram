#include "../platform.h"
#include <time.h>
#include <unistd.h>

#ifdef ENGRAM_PLATFORM_MACOS
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/thread_info.h>

static mach_timebase_info_data_t timebase_info;
static int timebase_initialized = 0;

uint64_t engram_time_now_ns(void) {
    if (!timebase_initialized) {
        mach_timebase_info(&timebase_info);
        timebase_initialized = 1;
    }
    uint64_t ticks = mach_absolute_time();
    return ticks * timebase_info.numer / timebase_info.denom;
}

float engram_cpu_usage_percent(void) {
    thread_basic_info_data_t info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;

    kern_return_t kr = thread_info(mach_thread_self(), THREAD_BASIC_INFO, (thread_info_t)&info, &count);
    if (kr != KERN_SUCCESS) {
        return 0.0f;
    }

    return (float)info.cpu_usage / (float)TH_USAGE_SCALE * 100.0f;
}

#else

uint64_t engram_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

float engram_cpu_usage_percent(void) {
    static uint64_t last_time = 0;
    static clock_t last_clock = 0;

    uint64_t now = engram_time_now_ns();
    clock_t now_clock = clock();

    if (last_time == 0) {
        last_time = now;
        last_clock = now_clock;
        return 0.0f;
    }

    uint64_t elapsed_ns = now - last_time;
    clock_t elapsed_clock = now_clock - last_clock;

    last_time = now;
    last_clock = now_clock;

    if (elapsed_ns == 0) {
        return 0.0f;
    }

    float cpu_time_sec = (float)elapsed_clock / (float)CLOCKS_PER_SEC;
    float wall_time_sec = (float)elapsed_ns / 1000000000.0f;

    return (cpu_time_sec / wall_time_sec) * 100.0f;
}

#endif

void engram_sleep_us(uint32_t microseconds) {
    usleep(microseconds);
}
