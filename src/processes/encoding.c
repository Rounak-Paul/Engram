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

static int is_vowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

static int has_vowel(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (is_vowel(s[i])) return 1;
    }
    return 0;
}

static int ends_with(const char *s, size_t len, const char *suffix, size_t slen) {
    if (len < slen) return 0;
    for (size_t i = 0; i < slen; i++) {
        if (s[len - slen + i] != suffix[i]) return 0;
    }
    return 1;
}

static int measure(const char *s, size_t len) {
    int m = 0;
    size_t i = 0;
    while (i < len && is_vowel(s[i])) i++;
    while (i < len) {
        while (i < len && !is_vowel(s[i])) i++;
        if (i >= len) break;
        m++;
        while (i < len && is_vowel(s[i])) i++;
    }
    return m;
}

static size_t stem_word(const char *word, size_t len, char *out) {
    if (len < 3) {
        for (size_t i = 0; i < len; i++) out[i] = word[i];
        return len;
    }
    
    for (size_t i = 0; i < len; i++) out[i] = word[i];
    
    if (ends_with(out, len, "sses", 4)) { len -= 2; }
    else if (ends_with(out, len, "ies", 3)) { out[len-3] = 'i'; len -= 2; }
    else if (ends_with(out, len, "ss", 2)) { }
    else if (len > 2 && out[len-1] == 's') { len--; }
    
    if (ends_with(out, len, "eed", 3)) {
        if (measure(out, len - 3) > 0) len--;
    }
    else if (ends_with(out, len, "ed", 2) && has_vowel(out, len - 2)) {
        len -= 2;
        if (ends_with(out, len, "at", 2) || ends_with(out, len, "bl", 2) || ends_with(out, len, "iz", 2)) {
            out[len++] = 'e';
        }
        else if (len > 2 && out[len-1] == out[len-2] && out[len-1] != 'l' && out[len-1] != 's' && out[len-1] != 'z') {
            len--;
        }
    }
    else if (ends_with(out, len, "ing", 3) && has_vowel(out, len - 3)) {
        len -= 3;
        if (ends_with(out, len, "at", 2) || ends_with(out, len, "bl", 2) || ends_with(out, len, "iz", 2)) {
            out[len++] = 'e';
        }
        else if (len > 2 && out[len-1] == out[len-2] && out[len-1] != 'l' && out[len-1] != 's' && out[len-1] != 'z') {
            len--;
        }
    }
    
    if (ends_with(out, len, "y", 1) && len > 2 && has_vowel(out, len - 1)) {
        out[len-1] = 'i';
    }
    
    if (len > 7 && measure(out, len - 7) > 0) {
        if (ends_with(out, len, "ational", 7)) { len -= 5; out[len-2] = 'e'; }
        else if (ends_with(out, len, "ization", 7)) { len -= 5; out[len-2] = 'e'; }
        else if (ends_with(out, len, "iveness", 7)) { len -= 4; }
        else if (ends_with(out, len, "fulness", 7)) { len -= 4; }
        else if (ends_with(out, len, "ousness", 7)) { len -= 4; }
    }
    if (len > 6 && measure(out, len - 6) > 0) {
        if (ends_with(out, len, "tional", 6)) { len -= 2; }
    }
    if (len > 5 && measure(out, len - 5) > 0) {
        if (ends_with(out, len, "ation", 5)) { len -= 3; out[len-2] = 'e'; }
    }
    if (len > 4 && measure(out, len - 4) > 0) {
        if (ends_with(out, len, "ness", 4)) { len -= 4; }
        else if (ends_with(out, len, "ment", 4)) { len -= 4; }
        else if (ends_with(out, len, "able", 4)) { len -= 4; }
        else if (ends_with(out, len, "ible", 4)) { len -= 4; }
    }
    if (len > 3 && measure(out, len - 3) > 0) {
        if (ends_with(out, len, "ful", 3)) { len -= 3; }
        else if (ends_with(out, len, "ive", 3)) { len -= 3; }
        else if (ends_with(out, len, "ize", 3)) { len -= 3; }
        else if (ends_with(out, len, "ous", 3)) { len -= 3; }
        else if (ends_with(out, len, "ent", 3)) { len -= 3; }
        else if (ends_with(out, len, "ant", 3)) { len -= 3; }
        else if (ends_with(out, len, "ism", 3)) { len -= 3; }
        else if (ends_with(out, len, "ist", 3)) { len -= 3; }
        else if (ends_with(out, len, "ity", 3)) { len -= 3; }
        else if (ends_with(out, len, "ify", 3)) { len -= 3; }
        else if (ends_with(out, len, "ion", 3) && len > 4 && (out[len-4] == 's' || out[len-4] == 't')) { len -= 3; }
    }
    if (len > 2 && measure(out, len - 2) > 0) {
        if (ends_with(out, len, "al", 2)) { len -= 2; }
        else if (ends_with(out, len, "er", 2)) { len -= 2; }
        else if (ends_with(out, len, "ic", 2)) { len -= 2; }
        else if (ends_with(out, len, "ly", 2)) { len -= 2; }
    }
    
    if (len > 1 && measure(out, len - 1) > 1 && ends_with(out, len, "e", 1)) {
        len--;
    }
    
    if (len < 2) len = 2;
    
    return len;
}

static uint32_t word_to_primary_neuron(engram_t *eng, const char *word, size_t len) {
    char stemmed[64];
    size_t stem_len = stem_word(word, len, stemmed);
    
    uint64_t word_hash = fnv1a_hash(stemmed, stem_len);
    uint64_t h = fnv1a_hash_seeded(&word_hash, sizeof(word_hash), 0);
    return (uint32_t)(h % eng->neurons.count);
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

float encoding_pattern_overlap(uint32_t *pattern_a, uint32_t count_a, uint32_t *pattern_b, uint32_t count_b) {
    if (count_a == 0 || count_b == 0) return 0.0f;
    
    uint32_t matches = 0;
    for (uint32_t i = 0; i < count_a; i++) {
        for (uint32_t j = 0; j < count_b; j++) {
            if (pattern_a[i] == pattern_b[j]) {
                matches++;
                break;
            }
        }
    }
    
    uint32_t smaller = count_a < count_b ? count_a : count_b;
    return (float)matches / (float)smaller;
}

void word_memory_learn(engram_t *eng, const char *text, size_t size, uint32_t tick) {
    char normalized[1024];
    size_t norm_size = 0;
    size_t input_size = size < 1023 ? size : 1023;
    normalize_text(text, input_size, normalized, &norm_size);
    
    size_t word_start = 0;
    for (size_t i = 0; i <= norm_size; i++) {
        if (i == norm_size || normalized[i] == ' ') {
            if (i > word_start) {
                size_t word_len = i - word_start;
                if (word_len > 31) word_len = 31;
                
                uint32_t neuron_id = word_to_primary_neuron(eng, &normalized[word_start], word_len);
                
                int found = 0;
                for (uint32_t j = 0; j < eng->word_memory.count; j++) {
                    if (eng->word_memory.entries[j].neuron_id == neuron_id) {
                        eng->word_memory.entries[j].strength += 0.1f;
                        if (eng->word_memory.entries[j].strength > 2.0f) {
                            eng->word_memory.entries[j].strength = 2.0f;
                        }
                        eng->word_memory.entries[j].last_seen_tick = tick;
                        eng->word_memory.entries[j].exposure_count++;
                        eng->word_memory.entries[j].connection_count++;
                        found = 1;
                        break;
                    }
                }
                
                if (!found) {
                    if (eng->word_memory.count >= eng->word_memory.capacity) {
                        uint32_t new_cap = eng->word_memory.capacity == 0 ? 2048 : eng->word_memory.capacity * 2;
                        word_memory_entry_t *new_entries = engram_realloc(eng, eng->word_memory.entries,
                                                                           new_cap * sizeof(word_memory_entry_t));
                        if (!new_entries) continue;
                        eng->word_memory.entries = new_entries;
                        eng->word_memory.capacity = new_cap;
                    }
                    
                    char stemmed[64];
                    size_t stem_len = stem_word(&normalized[word_start], word_len, stemmed);
                    if (stem_len > 31) stem_len = 31;
                    
                    word_memory_entry_t *entry = &eng->word_memory.entries[eng->word_memory.count++];
                    for (size_t k = 0; k < stem_len; k++) {
                        entry->token[k] = stemmed[k];
                    }
                    entry->token[stem_len] = '\0';
                    entry->neuron_id = neuron_id;
                    entry->strength = 0.5f;
                    entry->last_seen_tick = tick;
                    entry->exposure_count = 1;
                    entry->connection_count = 1;
                }
            }
            word_start = i + 1;
        }
    }
}

int word_memory_reconstruct(engram_t *eng, uint32_t *neuron_ids, uint32_t count, char *out_buffer, size_t buffer_size) {
    if (!out_buffer || buffer_size == 0) return -1;
    
    out_buffer[0] = '\0';
    size_t pos = 0;
    uint32_t words_found = 0;
    
    word_memory_entry_t *matches[48];
    float scores[48];
    uint32_t match_count = 0;
    
    for (uint32_t i = 0; i < count && match_count < 48; i++) {
        uint32_t neuron_id = neuron_ids[i];
        
        for (uint32_t j = 0; j < eng->word_memory.count; j++) {
            if (eng->word_memory.entries[j].neuron_id == neuron_id &&
                eng->word_memory.entries[j].strength > 0.1f) {
                
                word_memory_entry_t *entry = &eng->word_memory.entries[j];
                float score = entry->strength;
                
                int already_added = 0;
                for (uint32_t k = 0; k < match_count; k++) {
                    if (matches[k] == entry) {
                        already_added = 1;
                        break;
                    }
                }
                
                if (!already_added) {
                    matches[match_count] = entry;
                    scores[match_count] = score;
                    match_count++;
                }
                break;
            }
        }
    }
    
    for (uint32_t i = 0; i < match_count - 1; i++) {
        for (uint32_t j = i + 1; j < match_count; j++) {
            if (scores[j] > scores[i]) {
                word_memory_entry_t *tmp = matches[i];
                float tmp_score = scores[i];
                matches[i] = matches[j];
                scores[i] = scores[j];
                matches[j] = tmp;
                scores[j] = tmp_score;
            }
        }
    }
    
    uint32_t max_words = match_count < 8 ? match_count : 8;
    
    for (uint32_t i = 0; i < max_words && pos < buffer_size - 1; i++) {
        word_memory_entry_t *entry = matches[i];
        
        if (words_found > 0 && pos < buffer_size - 1) {
            out_buffer[pos++] = ' ';
        }
        
        const char *word = entry->token;
        size_t word_len = 0;
        while (word[word_len] && pos + word_len < buffer_size - 1) {
            out_buffer[pos + word_len] = word[word_len];
            word_len++;
        }
        pos += word_len;
        words_found++;
        
        entry->strength += 0.05f;
        if (entry->strength > 2.0f) entry->strength = 2.0f;
    }
    
    out_buffer[pos] = '\0';
    return words_found > 0 ? 0 : -1;
}

void word_memory_decay(engram_t *eng, uint32_t current_tick) {
    float decay_rate = 0.001f;
    
    for (uint32_t i = 0; i < eng->word_memory.count; i++) {
        uint32_t age = current_tick - eng->word_memory.entries[i].last_seen_tick;
        float decay = decay_rate * (float)(age / 100);
        eng->word_memory.entries[i].strength -= decay;
        
        if (eng->word_memory.entries[i].strength < 0.05f) {
            eng->word_memory.entries[i] = eng->word_memory.entries[--eng->word_memory.count];
            i--;
        }
    }
}
