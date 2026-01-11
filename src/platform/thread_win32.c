#include "engram/platform.h"
#include <windows.h>
#include <stdlib.h>

struct engram_thread {
    HANDLE handle;
};

struct engram_mutex {
    CRITICAL_SECTION cs;
};

struct engram_cond {
    CONDITION_VARIABLE cv;
};

typedef struct {
    engram_thread_fn fn;
    void *arg;
} thread_wrapper_t;

static DWORD WINAPI thread_proc(LPVOID param) {
    thread_wrapper_t *w = param;
    engram_thread_fn fn = w->fn;
    void *arg = w->arg;
    free(w);
    fn(arg);
    return 0;
}

engram_thread_t *engram_thread_create(engram_thread_fn fn, void *arg) {
    engram_thread_t *t = malloc(sizeof(engram_thread_t));
    if (!t) return NULL;
    thread_wrapper_t *w = malloc(sizeof(thread_wrapper_t));
    if (!w) {
        free(t);
        return NULL;
    }
    w->fn = fn;
    w->arg = arg;
    t->handle = CreateThread(NULL, 0, thread_proc, w, 0, NULL);
    if (!t->handle) {
        free(w);
        free(t);
        return NULL;
    }
    return t;
}

void engram_thread_join(engram_thread_t *t) {
    if (t && t->handle) {
        WaitForSingleObject(t->handle, INFINITE);
    }
}

void engram_thread_destroy(engram_thread_t *t) {
    if (t) {
        if (t->handle) CloseHandle(t->handle);
        free(t);
    }
}

engram_mutex_t *engram_mutex_create(void) {
    engram_mutex_t *m = malloc(sizeof(engram_mutex_t));
    if (!m) return NULL;
    InitializeCriticalSection(&m->cs);
    return m;
}

void engram_mutex_destroy(engram_mutex_t *m) {
    if (m) {
        DeleteCriticalSection(&m->cs);
        free(m);
    }
}

void engram_mutex_lock(engram_mutex_t *m) {
    EnterCriticalSection(&m->cs);
}

void engram_mutex_unlock(engram_mutex_t *m) {
    LeaveCriticalSection(&m->cs);
}

engram_cond_t *engram_cond_create(void) {
    engram_cond_t *c = malloc(sizeof(engram_cond_t));
    if (!c) return NULL;
    InitializeConditionVariable(&c->cv);
    return c;
}

void engram_cond_destroy(engram_cond_t *c) {
    free(c);
}

void engram_cond_wait(engram_cond_t *c, engram_mutex_t *m) {
    SleepConditionVariableCS(&c->cv, &m->cs, INFINITE);
}

void engram_cond_signal(engram_cond_t *c) {
    WakeConditionVariable(&c->cv);
}

void engram_cond_broadcast(engram_cond_t *c) {
    WakeAllConditionVariable(&c->cv);
}

uint64_t engram_time_ms(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
}

void engram_sleep_ms(uint32_t ms) {
    Sleep(ms);
}

uint32_t engram_cpu_count(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}
