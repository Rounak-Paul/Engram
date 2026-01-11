#include "../platform.h"
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>

#ifdef ENGRAM_PLATFORM_MACOS
#include <mach/mach_time.h>
#include <mach/mach.h>
#include <mach/thread_info.h>
#endif

int engram_thread_create(engram_thread_t *thread, engram_thread_fn fn, void *arg) {
    return pthread_create(&thread->handle, NULL, fn, arg);
}

int engram_thread_join(engram_thread_t *thread) {
    return pthread_join(thread->handle, NULL);
}

void engram_thread_yield(void) {
    sched_yield();
}

int engram_mutex_init(engram_mutex_t *mutex) {
    return pthread_mutex_init(&mutex->handle, NULL);
}

void engram_mutex_destroy(engram_mutex_t *mutex) {
    pthread_mutex_destroy(&mutex->handle);
}

void engram_mutex_lock(engram_mutex_t *mutex) {
    pthread_mutex_lock(&mutex->handle);
}

void engram_mutex_unlock(engram_mutex_t *mutex) {
    pthread_mutex_unlock(&mutex->handle);
}

int engram_mutex_trylock(engram_mutex_t *mutex) {
    return pthread_mutex_trylock(&mutex->handle);
}

int engram_cond_init(engram_cond_t *cond) {
    return pthread_cond_init(&cond->handle, NULL);
}

void engram_cond_destroy(engram_cond_t *cond) {
    pthread_cond_destroy(&cond->handle);
}

void engram_cond_wait(engram_cond_t *cond, engram_mutex_t *mutex) {
    pthread_cond_wait(&cond->handle, &mutex->handle);
}

int engram_cond_timedwait(engram_cond_t *cond, engram_mutex_t *mutex, uint32_t timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return pthread_cond_timedwait(&cond->handle, &mutex->handle, &ts);
}

void engram_cond_signal(engram_cond_t *cond) {
    pthread_cond_signal(&cond->handle);
}

void engram_cond_broadcast(engram_cond_t *cond) {
    pthread_cond_broadcast(&cond->handle);
}
