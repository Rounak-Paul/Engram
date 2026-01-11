#ifndef ENGRAM_PLATFORM_H
#define ENGRAM_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

typedef struct engram_thread engram_thread_t;
typedef struct engram_mutex engram_mutex_t;
typedef struct engram_cond engram_cond_t;

typedef void *(*engram_thread_fn)(void *);

engram_thread_t *engram_thread_create(engram_thread_fn fn, void *arg);
void engram_thread_join(engram_thread_t *t);
void engram_thread_destroy(engram_thread_t *t);

engram_mutex_t *engram_mutex_create(void);
void engram_mutex_destroy(engram_mutex_t *m);
void engram_mutex_lock(engram_mutex_t *m);
void engram_mutex_unlock(engram_mutex_t *m);

engram_cond_t *engram_cond_create(void);
void engram_cond_destroy(engram_cond_t *c);
void engram_cond_wait(engram_cond_t *c, engram_mutex_t *m);
void engram_cond_signal(engram_cond_t *c);
void engram_cond_broadcast(engram_cond_t *c);

uint64_t engram_time_ms(void);
void engram_sleep_ms(uint32_t ms);

uint32_t engram_cpu_count(void);

#endif
