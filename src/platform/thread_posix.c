#include "engram/platform.h"
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

struct engram_thread {
    pthread_t handle;
};

struct engram_mutex {
    pthread_mutex_t handle;
};

struct engram_cond {
    pthread_cond_t handle;
};

engram_thread_t *engram_thread_create(engram_thread_fn fn, void *arg) {
    engram_thread_t *t = malloc(sizeof(engram_thread_t));
    if (!t) return NULL;
    if (pthread_create(&t->handle, NULL, fn, arg) != 0) {
        free(t);
        return NULL;
    }
    return t;
}

void engram_thread_join(engram_thread_t *t) {
    if (t) pthread_join(t->handle, NULL);
}

void engram_thread_destroy(engram_thread_t *t) {
    free(t);
}

engram_mutex_t *engram_mutex_create(void) {
    engram_mutex_t *m = malloc(sizeof(engram_mutex_t));
    if (!m) return NULL;
    pthread_mutex_init(&m->handle, NULL);
    return m;
}

void engram_mutex_destroy(engram_mutex_t *m) {
    if (m) {
        pthread_mutex_destroy(&m->handle);
        free(m);
    }
}

void engram_mutex_lock(engram_mutex_t *m) {
    pthread_mutex_lock(&m->handle);
}

void engram_mutex_unlock(engram_mutex_t *m) {
    pthread_mutex_unlock(&m->handle);
}

engram_cond_t *engram_cond_create(void) {
    engram_cond_t *c = malloc(sizeof(engram_cond_t));
    if (!c) return NULL;
    pthread_cond_init(&c->handle, NULL);
    return c;
}

void engram_cond_destroy(engram_cond_t *c) {
    if (c) {
        pthread_cond_destroy(&c->handle);
        free(c);
    }
}

void engram_cond_wait(engram_cond_t *c, engram_mutex_t *m) {
    pthread_cond_wait(&c->handle, &m->handle);
}

void engram_cond_signal(engram_cond_t *c) {
    pthread_cond_signal(&c->handle);
}

void engram_cond_broadcast(engram_cond_t *c) {
    pthread_cond_broadcast(&c->handle);
}

uint64_t engram_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

void engram_sleep_ms(uint32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

uint32_t engram_cpu_count(void) {
#ifdef __APPLE__
    int count;
    size_t size = sizeof(count);
    if (sysctlbyname("hw.ncpu", &count, &size, NULL, 0) == 0) {
        return (uint32_t)count;
    }
    return 1;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (uint32_t)n : 1;
#endif
}
