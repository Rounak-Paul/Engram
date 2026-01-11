#ifndef ENGRAM_ALLOCATOR_H
#define ENGRAM_ALLOCATOR_H

#include <stddef.h>

typedef void *(*engram_alloc_fn)(size_t size, void *ctx);
typedef void *(*engram_realloc_fn)(void *ptr, size_t size, void *ctx);
typedef void (*engram_free_fn)(void *ptr, void *ctx);

typedef struct engram_allocator {
    engram_alloc_fn alloc;
    engram_realloc_fn realloc;
    engram_free_fn free;
    void *ctx;
} engram_allocator_t;

engram_allocator_t engram_allocator_default(void);

#endif
