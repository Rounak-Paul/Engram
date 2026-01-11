#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0x454E4752
#define VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t neuron_count;
    uint32_t synapse_count;
    uint32_t pathway_count;
    uint32_t vocab_count;
} engram_file_header_t;

int engram_save(engram_t *eng, const char *path) {
    if (!eng || !path) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    engram_mutex_lock(eng->substrate.lock);
    engram_mutex_lock(eng->wernicke.vocab.lock);
    
    engram_file_header_t header = {
        .magic = MAGIC,
        .version = VERSION,
        .neuron_count = eng->substrate.neuron_count,
        .synapse_count = eng->substrate.synapse_count,
        .pathway_count = eng->substrate.pathway_count,
        .vocab_count = eng->wernicke.vocab.count
    };
    
    fwrite(&header, sizeof(header), 1, f);
    fwrite(eng->substrate.neurons, sizeof(engram_neuron_t), eng->substrate.neuron_count, f);
    fwrite(eng->substrate.synapses, sizeof(engram_synapse_t), eng->substrate.synapse_count, f);
    
    for (uint32_t i = 0; i < eng->substrate.pathway_count; i++) {
        engram_pathway_t *p = &eng->substrate.pathways[i];
        fwrite(&p->id, sizeof(uint32_t), 1, f);
        fwrite(&p->neuron_count, sizeof(uint32_t), 1, f);
        fwrite(p->neuron_ids, sizeof(uint32_t), p->neuron_count, f);
        fwrite(&p->strength, sizeof(float), 1, f);
        fwrite(&p->access_count, sizeof(uint32_t), 1, f);
    }
    
    for (uint32_t i = 0; i < eng->wernicke.vocab.count; i++) {
        engram_token_t *t = &eng->wernicke.vocab.tokens[i];
        fwrite(&t->len, sizeof(uint16_t), 1, f);
        fwrite(t->text, 1, t->len, f);
        fwrite(&t->pattern, sizeof(engram_pattern_t), 1, f);
    }
    
    engram_mutex_unlock(eng->wernicke.vocab.lock);
    engram_mutex_unlock(eng->substrate.lock);
    
    fclose(f);
    return 0;
}

engram_t *engram_load(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    engram_file_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return NULL;
    }
    
    if (header.magic != MAGIC || header.version != VERSION) {
        fclose(f);
        return NULL;
    }
    
    engram_config_t config = engram_config_default();
    config.neuron_count = header.neuron_count;
    config.synapse_pool_size = header.synapse_count * 2;
    
    engram_t *eng = engram_create(&config);
    if (!eng) {
        fclose(f);
        return NULL;
    }
    
    engram_mutex_lock(eng->substrate.lock);
    engram_mutex_lock(eng->wernicke.vocab.lock);
    
    fread(eng->substrate.neurons, sizeof(engram_neuron_t), header.neuron_count, f);
    fread(eng->substrate.synapses, sizeof(engram_synapse_t), header.synapse_count, f);
    eng->substrate.synapse_count = header.synapse_count;
    
    for (uint32_t i = 0; i < header.pathway_count; i++) {
        if (eng->substrate.pathway_count >= eng->substrate.pathway_capacity) break;
        
        engram_pathway_t *p = &eng->substrate.pathways[eng->substrate.pathway_count];
        fread(&p->id, sizeof(uint32_t), 1, f);
        fread(&p->neuron_count, sizeof(uint32_t), 1, f);
        p->neuron_ids = malloc(p->neuron_count * sizeof(uint32_t));
        fread(p->neuron_ids, sizeof(uint32_t), p->neuron_count, f);
        fread(&p->strength, sizeof(float), 1, f);
        fread(&p->access_count, sizeof(uint32_t), 1, f);
        eng->substrate.pathway_count++;
    }
    
    for (uint32_t i = 0; i < header.vocab_count; i++) {
        if (eng->wernicke.vocab.count >= eng->wernicke.vocab.capacity) break;
        
        engram_token_t *t = &eng->wernicke.vocab.tokens[eng->wernicke.vocab.count];
        fread(&t->len, sizeof(uint16_t), 1, f);
        fread(t->text, 1, t->len, f);
        t->text[t->len] = '\0';
        t->id = (uint16_t)eng->wernicke.vocab.count;
        fread(&t->pattern, sizeof(engram_pattern_t), 1, f);
        eng->wernicke.vocab.count++;
    }
    
    engram_mutex_unlock(eng->wernicke.vocab.lock);
    engram_mutex_unlock(eng->substrate.lock);
    
    fclose(f);
    return eng;
}
