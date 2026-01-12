#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define WERNICKE_HASH_SIZE 65536
#define NGRAM_SIZE 3
#define MAX_WORD_LEN 64

wernicke_t wernicke_create(void) {
    wernicke_t w = {0};
    w.char_hashes = calloc(256, sizeof(uint32_t));
    w.hash_table_size = WERNICKE_HASH_SIZE;
    for (int i = 0; i < 256; i++) {
        w.char_hashes[i] = hash_string((const char *)&i, 1);
    }
    return w;
}

void wernicke_destroy(wernicke_t *w) {
    free(w->char_hashes);
    memset(w, 0, sizeof(*w));
}

static void normalize_text(const char *src, char *dst, size_t max_len) {
    size_t j = 0;
    bool prev_space = true;
    for (size_t i = 0; src[i] && j < max_len - 1; i++) {
        char c = src[i];
        if (isalpha((unsigned char)c)) {
            dst[j++] = (char)tolower((unsigned char)c);
            prev_space = false;
        } else if (isdigit((unsigned char)c)) {
            dst[j++] = c;
            prev_space = false;
        } else if (!prev_space && j > 0) {
            dst[j++] = ' ';
            prev_space = true;
        }
    }
    if (j > 0 && dst[j-1] == ' ') j--;
    dst[j] = '\0';
}

static uint64_t hash_word(const char *str, size_t len) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)str[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

void wernicke_tokenize(wernicke_t *w, const char *text, uint32_t *tokens, size_t *count, size_t max_tokens) {
    (void)w;
    char normalized[4096];
    normalize_text(text, normalized, sizeof(normalized));
    
    size_t tok_count = 0;
    
    char *word = normalized;
    while (*word && tok_count < max_tokens) {
        char *end = word;
        while (*end && *end != ' ') end++;
        
        size_t word_len = end - word;
        if (word_len > 0 && word_len <= MAX_WORD_LEN) {
            tokens[tok_count++] = (uint32_t)hash_word(word, word_len);
        }
        
        word = end;
        while (*word == ' ') word++;
    }
    
    *count = tok_count;
}

void wernicke_encode(wernicke_t *w, const char *text, engram_vec_t out) {
    (void)w;
    vec_zero(out);
    
    char normalized[4096];
    normalize_text(text, normalized, sizeof(normalized));
    size_t text_len = strlen(normalized);
    
    if (text_len == 0) return;
    
    char *words[256];
    size_t word_lens[256];
    size_t word_count = 0;
    
    char *p = normalized;
    while (*p && word_count < 256) {
        while (*p == ' ') p++;
        if (!*p) break;
        
        words[word_count] = p;
        char *end = p;
        while (*end && *end != ' ') end++;
        word_lens[word_count] = end - p;
        word_count++;
        p = end;
    }
    
    if (word_count == 0) return;
    
    for (size_t i = 0; i < word_count; i++) {
        uint64_t h = hash_word(words[i], word_lens[i]);
        
        for (int d = 0; d < ENGRAM_VECTOR_DIM; d++) {
            uint64_t mixed = h ^ ((uint64_t)d * 0x9e3779b97f4a7c15ULL);
            mixed ^= mixed >> 33;
            mixed *= 0xff51afd7ed558ccdULL;
            mixed ^= mixed >> 33;
            
            float val = ((mixed & 1) ? 1.0f : -1.0f);
            float pos_scale = 1.0f + 0.5f * (1.0f - (float)i / (float)word_count);
            out[d] += val * pos_scale;
        }
    }
    
    for (size_t i = 0; i + 1 < word_count; i++) {
        char bigram[MAX_WORD_LEN * 2 + 2];
        size_t len1 = word_lens[i] < MAX_WORD_LEN ? word_lens[i] : MAX_WORD_LEN;
        size_t len2 = word_lens[i+1] < MAX_WORD_LEN ? word_lens[i+1] : MAX_WORD_LEN;
        
        memcpy(bigram, words[i], len1);
        bigram[len1] = '_';
        memcpy(bigram + len1 + 1, words[i+1], len2);
        
        uint64_t h = hash_word(bigram, len1 + 1 + len2);
        
        for (int d = 0; d < ENGRAM_VECTOR_DIM; d++) {
            uint64_t mixed = h ^ ((uint64_t)d * 0x9e3779b97f4a7c15ULL);
            mixed ^= mixed >> 33;
            mixed *= 0xff51afd7ed558ccdULL;
            mixed ^= mixed >> 33;
            
            float val = ((mixed & 1) ? 0.5f : -0.5f);
            out[d] += val;
        }
    }
    
    for (size_t i = 0; i < word_count; i++) {
        for (size_t j = 0; j + NGRAM_SIZE <= word_lens[i]; j++) {
            uint64_t h = hash_word(words[i] + j, NGRAM_SIZE);
            
            for (int d = 0; d < ENGRAM_VECTOR_DIM; d++) {
                uint64_t mixed = h ^ ((uint64_t)d * 0x9e3779b97f4a7c15ULL);
                mixed ^= mixed >> 33;
                
                float val = ((mixed & 1) ? 0.25f : -0.25f);
                out[d] += val;
            }
        }
    }
    
    vec_normalize(out);
}
