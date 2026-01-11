#ifndef ENGRAM_PLATFORM_H
#define ENGRAM_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
    #define ENGRAM_PLATFORM_WINDOWS
#elif defined(__APPLE__)
    #define ENGRAM_PLATFORM_MACOS
#else
    #define ENGRAM_PLATFORM_POSIX
#endif

typedef struct engram_thread engram_thread_t;
typedef struct engram_mutex engram_mutex_t;
typedef struct engram_cond engram_cond_t;

typedef void *(*engram_thread_fn)(void *arg);

int engram_thread_create(engram_thread_t *thread, engram_thread_fn fn, void *arg);
int engram_thread_join(engram_thread_t *thread);
void engram_thread_yield(void);

int engram_mutex_init(engram_mutex_t *mutex);
void engram_mutex_destroy(engram_mutex_t *mutex);
void engram_mutex_lock(engram_mutex_t *mutex);
void engram_mutex_unlock(engram_mutex_t *mutex);
int engram_mutex_trylock(engram_mutex_t *mutex);

int engram_cond_init(engram_cond_t *cond);
void engram_cond_destroy(engram_cond_t *cond);
void engram_cond_wait(engram_cond_t *cond, engram_mutex_t *mutex);
int engram_cond_timedwait(engram_cond_t *cond, engram_mutex_t *mutex, uint32_t timeout_ms);
void engram_cond_signal(engram_cond_t *cond);
void engram_cond_broadcast(engram_cond_t *cond);

uint64_t engram_time_now_ns(void);
void engram_sleep_us(uint32_t microseconds);
float engram_cpu_usage_percent(void);

#ifdef ENGRAM_PLATFORM_WINDOWS
    #include <windows.h>
    struct engram_thread {
        HANDLE handle;
    };
    struct engram_mutex {
        CRITICAL_SECTION cs;
    };
    struct engram_cond {
        CONDITION_VARIABLE cv;
    };
#else
    #include <pthread.h>
    struct engram_thread {
        pthread_t handle;
    };
    struct engram_mutex {
        pthread_mutex_t handle;
    };
    struct engram_cond {
        pthread_cond_t handle;
    };
#endif

#endif
