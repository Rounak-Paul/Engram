#include "../internal.h"
#include "../core.h"
#include "../processes.h"
#include <string.h>
#include <ctype.h>

#define NGRAM_SIZE 3
#define SHINGLE_COUNT 64
#define MINHASH_BUCKETS 16

static uint64_t fnv1a_hash(const void *data, size_t size);
static uint64_t fnv1a_hash_seeded(const void *data, size_t size, uint64_t seed);
static void extract_ngram_hashes(const void *data, size_t size, uint64_t *out_hashes, uint32_t *out_count, uint32_t max_hashes);
static void minhash_signature(uint64_t *hashes, uint32_t hash_count, uint64_t *out_sig, uint32_t sig_size);
static void normalize_text(const char *input, size_t size, char *output, size_t *out_size);

void encoding_init(void) {
}

int encoding_cue_to_sdr(engram_t *eng, const engram_cue_t *cue, uint32_t *out_neuron_ids, uint32_t *out_count, uint32_t max_neurons) {
    if (!cue->data || cue->size == 0) {
        *out_count = 0;
        return 0;
    }

    uint32_t target_active = eng->neurons.count / 50;
    if (target_active < 20) {
        target_active = 20;
    }
    if (target_active > max_neurons) {
        target_active = max_neurons;
    }
    if (target_active > 256) {
        target_active = 256;
    }

    uint64_t ngram_hashes[512];
    uint32_t ngram_count = 0;

    if (cue->modality == ENGRAM_MODALITY_TEXT) {
        char normalized[1024];
        size_t norm_size = 0;
        size_t input_size = cue->size < 1023 ? cue->size : 1023;
        normalize_text((const char *)cue->data, input_size, normalized, &norm_size);
        extract_ngram_hashes(normalized, norm_size, ngram_hashes, &ngram_count, 512);
    } else {
        extract_ngram_hashes(cue->data, cue->size, ngram_hashes, &ngram_count, 512);
    }

    if (ngram_count == 0) {
        uint64_t base_hash = fnv1a_hash(cue->data, cue->size);
        for (uint32_t i = 0; i < target_active; i++) {
            uint64_t h = fnv1a_hash_seeded(&base_hash, sizeof(base_hash), i * 0x9E3779B97F4A7C15ULL);
            out_neuron_ids[i] = (uint32_t)(h % eng->neurons.count);
        }
        *out_count = target_active;
        return 0;
    }

    uint64_t minhash_sig[MINHASH_BUCKETS];
    minhash_signature(ngram_hashes, ngram_count, minhash_sig, MINHASH_BUCKETS);

    uint32_t neurons_per_bucket = target_active / MINHASH_BUCKETS;
    if (neurons_per_bucket < 1) {
        neurons_per_bucket = 1;
    }

    uint32_t idx = 0;
    for (uint32_t b = 0; b < MINHASH_BUCKETS && idx < target_active; b++) {
        for (uint32_t j = 0; j < neurons_per_bucket && idx < target_active; j++) {
            uint64_t h = fnv1a_hash_seeded(&minhash_sig[b], sizeof(uint64_t), j);
            uint32_t neuron_id = (uint32_t)(h % eng->neurons.count);
            
            int duplicate = 0;
            for (uint32_t k = 0; k < idx; k++) {
                if (out_neuron_ids[k] == neuron_id) {
                    duplicate = 1;
                    break;
                }
            }
            
            if (!duplicate) {
                out_neuron_ids[idx++] = neuron_id;
            } else {
                h = fnv1a_hash_seeded(&h, sizeof(uint64_t), idx);
                out_neuron_ids[idx++] = (uint32_t)(h % eng->neurons.count);
            }
        }
    }

    while (idx < target_active) {
        uint64_t h = fnv1a_hash_seeded(minhash_sig, sizeof(minhash_sig), idx);
        out_neuron_ids[idx] = (uint32_t)(h % eng->neurons.count);
        idx++;
    }

    *out_count = target_active;
    return 0;
}

uint64_t encoding_hash(const void *data, size_t size) {
    return fnv1a_hash(data, size);
}

float encoding_similarity(engram_t *eng, const engram_cue_t *cue1, const engram_cue_t *cue2) {
    uint32_t neurons1[256], neurons2[256];
    uint32_t count1 = 0, count2 = 0;

    if (encoding_cue_to_sdr(eng, cue1, neurons1, &count1, 256) != 0) {
        return 0.0f;
    }
    if (encoding_cue_to_sdr(eng, cue2, neurons2, &count2, 256) != 0) {
        return 0.0f;
    }

    if (count1 == 0 || count2 == 0) {
        return 0.0f;
    }

    uint32_t matches = 0;
    for (uint32_t i = 0; i < count1; i++) {
        for (uint32_t j = 0; j < count2; j++) {
            if (neurons1[i] == neurons2[j]) {
                matches++;
                break;
            }
        }
    }

    uint32_t union_size = count1 + count2 - matches;
    return (float)matches / (float)union_size;
}

static uint64_t fnv1a_hash(const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 14695981039346656037ULL;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }

    return hash;
}

static uint64_t fnv1a_hash_seeded(const void *data, size_t size, uint64_t seed) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = seed ^ 14695981039346656037ULL;

    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;
    }

    hash ^= hash >> 33;
    hash *= 0xC6A4A7935BD1E995ULL;
    hash ^= hash >> 47;

    return hash;
}

static void normalize_text(const char *input, size_t size, char *output, size_t *out_size) {
    size_t j = 0;
    int prev_space = 1;

    for (size_t i = 0; i < size && j < 1023; i++) {
        char c = input[i];
        
        if (c >= 'A' && c <= 'Z') {
            c = c + ('a' - 'A');
        }

        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            output[j++] = c;
            prev_space = 0;
        } else if (!prev_space && j > 0) {
            output[j++] = ' ';
            prev_space = 1;
        }
    }

    if (j > 0 && output[j - 1] == ' ') {
        j--;
    }

    output[j] = '\0';
    *out_size = j;
}

static void extract_ngram_hashes(const void *data, size_t size, uint64_t *out_hashes, uint32_t *out_count, uint32_t max_hashes) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t count = 0;

    size_t word_start = 0;
    for (size_t i = 0; i <= size && count < max_hashes; i++) {
        if (i == size || bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\n') {
            if (i > word_start) {
                size_t word_len = i - word_start;
                
                for (int rep = 0; rep < 4 && count < max_hashes; rep++) {
                    uint64_t h = fnv1a_hash(&bytes[word_start], word_len);
                    h ^= (uint64_t)rep * 0x9E3779B97F4A7C15ULL;
                    out_hashes[count++] = h;
                }
                
                if (word_len >= 4) {
                    for (size_t j = 0; j <= word_len - 2 && count < max_hashes; j++) {
                        out_hashes[count++] = fnv1a_hash(&bytes[word_start + j], 2);
                    }
                }
            }
            word_start = i + 1;
        }
    }

    if (size >= NGRAM_SIZE) {
        for (size_t i = 0; i <= size - NGRAM_SIZE && count < max_hashes; i++) {
            out_hashes[count++] = fnv1a_hash(&bytes[i], NGRAM_SIZE);
        }
    } else if (size > 0 && count == 0) {
        out_hashes[count++] = fnv1a_hash(data, size);
    }

    *out_count = count;
}

static void minhash_signature(uint64_t *hashes, uint32_t hash_count, uint64_t *out_sig, uint32_t sig_size) {
    static const uint64_t seeds[MINHASH_BUCKETS] = {
        0x9E3779B97F4A7C15ULL, 0xBF58476D1CE4E5B9ULL,
        0x94D049BB133111EBULL, 0x6A09E667F3BCC908ULL,
        0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL,
        0xA54FF53A5F1D36F1ULL, 0x510E527FADE682D1ULL,
        0x9B05688C2B3E6C1FULL, 0x1F83D9ABFB41BD6BULL,
        0x5BE0CD19137E2179ULL, 0xC3A5C85C97CB3127ULL,
        0xB492B66FBE98F273ULL, 0x9AE16A3B2F90404FULL,
        0xCBF29CE484222325ULL, 0xC4CEB9FE1A85EC53ULL
    };

    for (uint32_t b = 0; b < sig_size; b++) {
        uint64_t min_hash = UINT64_MAX;
        for (uint32_t i = 0; i < hash_count; i++) {
            uint64_t h = hashes[i] ^ seeds[b];
            h *= 0xC6A4A7935BD1E995ULL;
            h ^= h >> 47;
            if (h < min_hash) {
                min_hash = h;
            }
        }
        out_sig[b] = min_hash;
    }
}
