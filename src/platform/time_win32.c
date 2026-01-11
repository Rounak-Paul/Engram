#include "../platform.h"

#ifdef ENGRAM_PLATFORM_WINDOWS

#include <windows.h>

static LARGE_INTEGER frequency;
static int frequency_initialized = 0;

uint64_t engram_time_now_ns(void) {
    if (!frequency_initialized) {
        QueryPerformanceFrequency(&frequency);
        frequency_initialized = 1;
    }

    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);

    return (uint64_t)((double)counter.QuadPart / (double)frequency.QuadPart * 1000000000.0);
}

void engram_sleep_us(uint32_t microseconds) {
    DWORD ms = microseconds / 1000;
    if (ms == 0) {
        ms = 1;
    }
    Sleep(ms);
}

float engram_cpu_usage_percent(void) {
    static ULARGE_INTEGER last_cpu = {0};
    static ULARGE_INTEGER last_time = {0};

    FILETIME creation, exit, kernel, user;
    GetThreadTimes(GetCurrentThread(), &creation, &exit, &kernel, &user);

    ULARGE_INTEGER current_cpu;
    current_cpu.LowPart = user.dwLowDateTime + kernel.dwLowDateTime;
    current_cpu.HighPart = user.dwHighDateTime + kernel.dwHighDateTime;

    ULARGE_INTEGER current_time;
    GetSystemTimeAsFileTime((FILETIME *)&current_time);

    if (last_time.QuadPart == 0) {
        last_cpu = current_cpu;
        last_time = current_time;
        return 0.0f;
    }

    ULONGLONG cpu_diff = current_cpu.QuadPart - last_cpu.QuadPart;
    ULONGLONG time_diff = current_time.QuadPart - last_time.QuadPart;

    last_cpu = current_cpu;
    last_time = current_time;

    if (time_diff == 0) {
        return 0.0f;
    }

    return (float)cpu_diff / (float)time_diff * 100.0f;
}

#endif
