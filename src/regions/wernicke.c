#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void engram_wernicke_init(engram_wernicke_t *w) {
    w->vocab.capacity = ENGRAM_VOCAB_SIZE;
    w->vocab.count = 0;
    w->vocab.tokens = calloc(w->vocab.capacity, sizeof(engram_token_t));
    w->vocab.lock = engram_mutex_create();
    w->embeddings = NULL;
    w->embedding_count = 0;
}

void engram_wernicke_free(engram_wernicke_t *w) {
    free(w->vocab.tokens);
    engram_mutex_destroy(w->vocab.lock);
    free(w->embeddings);
}

static uint16_t find_or_add_token(engram_wernicke_t *w, const char *text, uint16_t len) {
    if (len > ENGRAM_TOKEN_MAX_LEN - 1) len = ENGRAM_TOKEN_MAX_LEN - 1;
    
    engram_mutex_lock(w->vocab.lock);
    
    for (uint32_t i = 0; i < w->vocab.count; i++) {
        if (w->vocab.tokens[i].len == len && 
            memcmp(w->vocab.tokens[i].text, text, len) == 0) {
            engram_mutex_unlock(w->vocab.lock);
            return w->vocab.tokens[i].id;
        }
    }
    
    if (w->vocab.count >= w->vocab.capacity) {
        engram_mutex_unlock(w->vocab.lock);
        return 0;
    }
    
    engram_token_t *t = &w->vocab.tokens[w->vocab.count];
    memcpy(t->text, text, len);
    t->text[len] = '\0';
    t->len = len;
    t->id = (uint16_t)w->vocab.count;
    
    engram_pattern_from_text(t->text, len, &t->pattern);
    
    w->vocab.count++;
    
    engram_mutex_unlock(w->vocab.lock);
    return t->id;
}

void engram_wernicke_tokenize(engram_wernicke_t *w, const char *text, size_t len,
                               uint16_t **ids, uint32_t *count) {
    uint32_t capacity = 256;
    *ids = malloc(capacity * sizeof(uint16_t));
    *count = 0;
    
    size_t i = 0;
    while (i < len) {
        while (i < len && isspace((unsigned char)text[i])) i++;
        if (i >= len) break;
        
        size_t start = i;
        while (i < len && !isspace((unsigned char)text[i])) i++;
        
        uint16_t token_len = (uint16_t)(i - start);
        if (token_len > 0) {
            if (*count >= capacity) {
                capacity *= 2;
                *ids = realloc(*ids, capacity * sizeof(uint16_t));
            }
            (*ids)[(*count)++] = find_or_add_token(w, text + start, token_len);
        }
    }
}

void engram_wernicke_detokenize(engram_wernicke_t *w, const uint16_t *ids, uint32_t count,
                                 char *out, size_t out_size) {
    size_t pos = 0;
    
    engram_mutex_lock(w->vocab.lock);
    
    for (uint32_t i = 0; i < count && pos < out_size - 1; i++) {
        if (ids[i] < w->vocab.count) {
            engram_token_t *t = &w->vocab.tokens[ids[i]];
            size_t copy_len = t->len;
            if (pos + copy_len + 1 >= out_size) {
                copy_len = out_size - pos - 2;
            }
            memcpy(out + pos, t->text, copy_len);
            pos += copy_len;
            if (i + 1 < count && pos < out_size - 1) {
                out[pos++] = ' ';
            }
        }
    }
    
    engram_mutex_unlock(w->vocab.lock);
    out[pos] = '\0';
}

void engram_wernicke_embed(engram_wernicke_t *w, uint16_t id, engram_pattern_t *out) {
    engram_mutex_lock(w->vocab.lock);
    
    if (id < w->vocab.count) {
        memcpy(out, &w->vocab.tokens[id].pattern, sizeof(engram_pattern_t));
    } else {
        memset(out, 0, sizeof(engram_pattern_t));
    }
    
    engram_mutex_unlock(w->vocab.lock);
}

engram_token_t *engram_wernicke_get_token(engram_wernicke_t *w, uint16_t id) {
    if (id >= w->vocab.count) return NULL;
    return &w->vocab.tokens[id];
}

uint32_t engram_wernicke_vocab_size(engram_wernicke_t *w) {
    return w->vocab.count;
}
