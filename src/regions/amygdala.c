#include "../internal.h"
#include "../regions.h"
#include <string.h>

static uint64_t hash_data(const void *data, size_t size);

int amygdala_init(engram_t *eng) {
    eng->amygdala.salience_threshold = 0.5f;
    eng->amygdala.novelty_decay = 0.001f;

    uint32_t capacity = 4096;
    eng->amygdala.seen_hashes = engram_alloc(eng, capacity * sizeof(uint64_t));
    if (!eng->amygdala.seen_hashes) {
        return -1;
    }
    memset(eng->amygdala.seen_hashes, 0, capacity * sizeof(uint64_t));

    eng->amygdala.seen_capacity = capacity;
    eng->amygdala.seen_count = 0;

    return 0;
}

void amygdala_destroy(engram_t *eng) {
    if (eng->amygdala.seen_hashes) {
        engram_free(eng, eng->amygdala.seen_hashes);
        eng->amygdala.seen_hashes = NULL;
    }
    eng->amygdala.seen_capacity = 0;
    eng->amygdala.seen_count = 0;
}

float amygdala_compute_salience(engram_t *eng, const void *data, size_t size) {
    uint64_t hash = hash_data(data, size);
    int novel = amygdala_is_novel(eng, hash);

    float salience = 0.5f;

    if (novel) {
        salience += 0.3f;
    }

    if (size > 100) {
        salience += 0.1f;
    }

    if (salience > 1.0f) {
        salience = 1.0f;
    }

    return salience;
}

int amygdala_is_novel(engram_t *eng, uint64_t hash) {
    for (uint32_t i = 0; i < eng->amygdala.seen_count; i++) {
        if (eng->amygdala.seen_hashes[i] == hash) {
            return 0;
        }
    }
    return 1;
}

void amygdala_mark_seen(engram_t *eng, uint64_t hash) {
    if (eng->amygdala.seen_count >= eng->amygdala.seen_capacity) {
        for (uint32_t i = 0; i < eng->amygdala.seen_capacity - 1; i++) {
            eng->amygdala.seen_hashes[i] = eng->amygdala.seen_hashes[i + 1];
        }
        eng->amygdala.seen_count = eng->amygdala.seen_capacity - 1;
    }

    eng->amygdala.seen_hashes[eng->amygdala.seen_count++] = hash;
}

static uint64_t hash_data(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 14695981039346656037ULL;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}

float amygdala_get_emotional_weight(engram_t *eng, float base_salience) {
    (void)eng;
    return base_salience * 1.5f;
}
