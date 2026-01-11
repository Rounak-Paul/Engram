#include "../internal.h"
#include "../core.h"
#include "../processes.h"
#include <string.h>
#include <ctype.h>

static uint64_t fnv1a_hash(const void *data, size_t size);
static uint64_t fnv1a_hash_seeded(const void *data, size_t size, uint64_t seed);
static void normalize_text(const char *input, size_t size, char *output, size_t *out_size);

void encoding_init(void) {
}

int encoding_cue_to_sdr(engram_t *eng, const engram_cue_t *cue, uint32_t *out_neuron_ids, uint32_t *out_count, uint32_t max_neurons) {
    if (!cue->data || cue->size == 0) {
        *out_count = 0;
        return 0;
    }

    uint32_t neurons_per_word = 8;
    uint32_t idx = 0;

    if (cue->modality == ENGRAM_MODALITY_TEXT) {
        char normalized[1024];
        size_t norm_size = 0;
        size_t input_size = cue->size < 1023 ? cue->size : 1023;
        normalize_text((const char *)cue->data, input_size, normalized, &norm_size);

        const char *text = normalized;
        size_t word_start = 0;
        
        for (size_t i = 0; i <= norm_size && idx < max_neurons; i++) {
            if (i == norm_size || text[i] == ' ') {
                if (i > word_start) {
                    size_t word_len = i - word_start;
                    uint64_t word_hash = fnv1a_hash(&text[word_start], word_len);
                    
                    for (uint32_t j = 0; j < neurons_per_word && idx < max_neurons; j++) {
                        uint64_t h = fnv1a_hash_seeded(&word_hash, sizeof(word_hash), j * 0x9E3779B97F4A7C15ULL);
                        uint32_t neuron_id = (uint32_t)(h % eng->neurons.count);
                        
                        int dup = 0;
                        for (uint32_t k = 0; k < idx; k++) {
                            if (out_neuron_ids[k] == neuron_id) {
                                dup = 1;
                                break;
                            }
                        }
                        if (!dup) {
                            out_neuron_ids[idx++] = neuron_id;
                        }
                    }
                }
                word_start = i + 1;
            }
        }
    } else {
        uint64_t hash = fnv1a_hash(cue->data, cue->size);
        for (uint32_t i = 0; i < 16 && idx < max_neurons; i++) {
            uint64_t h = fnv1a_hash_seeded(&hash, sizeof(hash), i);
            out_neuron_ids[idx++] = (uint32_t)(h % eng->neurons.count);
        }
    }

    *out_count = idx;
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

static uint32_t word_to_primary_neuron(engram_t *eng, const char *word, size_t len) {
    uint64_t word_hash = fnv1a_hash(word, len);
    uint64_t h = fnv1a_hash_seeded(&word_hash, sizeof(word_hash), 0);
    return (uint32_t)(h % eng->neurons.count);
}

static void lexicon_add_word(engram_t *eng, const char *word, size_t len, uint32_t primary_neuron) {
    if (len == 0 || len > 31) return;
    
    for (uint32_t i = 0; i < eng->lexicon.count; i++) {
        if (eng->lexicon.entries[i].primary_neuron == primary_neuron) {
            return;
        }
    }
    
    if (eng->lexicon.count >= eng->lexicon.capacity) {
        uint32_t new_cap = eng->lexicon.capacity == 0 ? 1024 : eng->lexicon.capacity * 2;
        lexicon_entry_t *new_entries = engram_realloc(eng, eng->lexicon.entries, 
                                                       new_cap * sizeof(lexicon_entry_t));
        if (!new_entries) return;
        eng->lexicon.entries = new_entries;
        eng->lexicon.capacity = new_cap;
    }
    
    lexicon_entry_t *entry = &eng->lexicon.entries[eng->lexicon.count++];
    size_t copy_len = len < 31 ? len : 31;
    for (size_t i = 0; i < copy_len; i++) {
        entry->word[i] = word[i];
    }
    entry->word[copy_len] = '\0';
    entry->primary_neuron = primary_neuron;
}

int encoding_register_words(engram_t *eng, const char *text, size_t size) {
    char normalized[1024];
    size_t norm_size = 0;
    size_t input_size = size < 1023 ? size : 1023;
    normalize_text(text, input_size, normalized, &norm_size);
    
    size_t word_start = 0;
    for (size_t i = 0; i <= norm_size; i++) {
        if (i == norm_size || normalized[i] == ' ') {
            if (i > word_start) {
                size_t word_len = i - word_start;
                uint32_t primary = word_to_primary_neuron(eng, &normalized[word_start], word_len);
                lexicon_add_word(eng, &normalized[word_start], word_len, primary);
            }
            word_start = i + 1;
        }
    }
    return 0;
}

int encoding_text_to_primary_neurons(engram_t *eng, const char *text, size_t size, uint32_t *out_neuron_ids, uint32_t *out_count, uint32_t max_neurons) {
    char normalized[1024];
    size_t norm_size = 0;
    size_t input_size = size < 1023 ? size : 1023;
    normalize_text(text, input_size, normalized, &norm_size);
    
    uint32_t idx = 0;
    size_t word_start = 0;
    for (size_t i = 0; i <= norm_size && idx < max_neurons; i++) {
        if (i == norm_size || normalized[i] == ' ') {
            if (i > word_start) {
                size_t word_len = i - word_start;
                uint32_t primary = word_to_primary_neuron(eng, &normalized[word_start], word_len);
                int dup = 0;
                for (uint32_t k = 0; k < idx; k++) {
                    if (out_neuron_ids[k] == primary) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup) {
                    out_neuron_ids[idx++] = primary;
                }
            }
            word_start = i + 1;
        }
    }
    *out_count = idx;
    return 0;
}

int encoding_neurons_to_text(engram_t *eng, uint32_t *neuron_ids, uint32_t count, char *out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0) return -1;
    
    out_buffer[0] = '\0';
    size_t pos = 0;
    uint32_t words_found = 0;
    
    for (uint32_t i = 0; i < count && pos < buffer_size - 1; i++) {
        uint32_t neuron_id = neuron_ids[i];
        
        for (uint32_t j = 0; j < eng->lexicon.count; j++) {
            if (eng->lexicon.entries[j].primary_neuron == neuron_id) {
                if (words_found > 0 && pos < buffer_size - 1) {
                    out_buffer[pos++] = ' ';
                }
                
                const char *word = eng->lexicon.entries[j].word;
                size_t word_len = 0;
                while (word[word_len] && pos + word_len < buffer_size - 1) {
                    out_buffer[pos + word_len] = word[word_len];
                    word_len++;
                }
                pos += word_len;
                words_found++;
                break;
            }
        }
    }
    
    out_buffer[pos] = '\0';
    return words_found > 0 ? 0 : -1;
}
