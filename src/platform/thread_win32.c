#include "../platform.h"

#ifdef ENGRAM_PLATFORM_WINDOWS

#include <windows.h>

typedef struct thread_start_data {
    engram_thread_fn fn;
    void *arg;
} thread_start_data_t;

static DWORD WINAPI thread_start_routine(LPVOID param) {
    thread_start_data_t *data = (thread_start_data_t *)param;
    engram_thread_fn fn = data->fn;
    void *arg = data->arg;
    HeapFree(GetProcessHeap(), 0, data);
    fn(arg);
    return 0;
}

int engram_thread_create(engram_thread_t *thread, engram_thread_fn fn, void *arg) {
    thread_start_data_t *data = HeapAlloc(GetProcessHeap(), 0, sizeof(thread_start_data_t));
    if (!data) {
        return -1;
    }
    data->fn = fn;
    data->arg = arg;

    thread->handle = CreateThread(NULL, 0, thread_start_routine, data, 0, NULL);
    if (!thread->handle) {
        HeapFree(GetProcessHeap(), 0, data);
        return -1;
    }
    return 0;
}

int engram_thread_join(engram_thread_t *thread) {
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
    return 0;
}

void engram_thread_yield(void) {
    SwitchToThread();
}

int engram_mutex_init(engram_mutex_t *mutex) {
    InitializeCriticalSection(&mutex->cs);
    return 0;
}

void engram_mutex_destroy(engram_mutex_t *mutex) {
    DeleteCriticalSection(&mutex->cs);
}

void engram_mutex_lock(engram_mutex_t *mutex) {
    EnterCriticalSection(&mutex->cs);
}

void engram_mutex_unlock(engram_mutex_t *mutex) {
    LeaveCriticalSection(&mutex->cs);
}

int engram_mutex_trylock(engram_mutex_t *mutex) {
    return TryEnterCriticalSection(&mutex->cs) ? 0 : -1;
}

int engram_cond_init(engram_cond_t *cond) {
    InitializeConditionVariable(&cond->cv);
    return 0;
}

void engram_cond_destroy(engram_cond_t *cond) {
    (void)cond;
}

void engram_cond_wait(engram_cond_t *cond, engram_mutex_t *mutex) {
    SleepConditionVariableCS(&cond->cv, &mutex->cs, INFINITE);
}

int engram_cond_timedwait(engram_cond_t *cond, engram_mutex_t *mutex, uint32_t timeout_ms) {
    return SleepConditionVariableCS(&cond->cv, &mutex->cs, timeout_ms) ? 0 : -1;
}

void engram_cond_signal(engram_cond_t *cond) {
    WakeConditionVariable(&cond->cv);
}

void engram_cond_broadcast(engram_cond_t *cond) {
    WakeAllConditionVariable(&cond->cv);
}

#endif
