#include "../internal.h"
#include "../regions.h"
#include <string.h>

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
