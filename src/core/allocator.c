#include "internal.h"
#include <stdlib.h>

static void *default_alloc(size_t size, void *ctx) {
    (void)ctx;
    return malloc(size);
}

static void *default_realloc(void *ptr, size_t size, void *ctx) {
    (void)ctx;
    return realloc(ptr, size);
}

static void default_free(void *ptr, void *ctx) {
    (void)ctx;
    free(ptr);
}

engram_allocator_t engram_allocator_default(void) {
    engram_allocator_t alloc = {
        .alloc = default_alloc,
        .realloc = default_realloc,
        .free = default_free,
        .ctx = NULL
    };
    return alloc;
}

void *engram_alloc(engram_t *eng, size_t size) {
    void *ptr = eng->allocator.alloc(size, eng->allocator.ctx);
    if (ptr) {
        eng->total_memory_bytes += size;
    }
    return ptr;
}

void *engram_realloc(engram_t *eng, void *ptr, size_t size) {
    return eng->allocator.realloc(ptr, size, eng->allocator.ctx);
}

void engram_free(engram_t *eng, void *ptr) {
    if (ptr) {
        eng->allocator.free(ptr, eng->allocator.ctx);
    }
}
