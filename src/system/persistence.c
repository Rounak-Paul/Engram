#include "../internal.h"
#include "../core.h"
#include "../system.h"
#include <stdio.h>
#include <string.h>

#define ENGRAM_FILE_MAGIC 0x454E4752
#define ENGRAM_FILE_VERSION 2

typedef struct file_header {
    uint32_t magic;
    uint32_t version;
    uint32_t neuron_count;
    uint32_t synapse_count;
    uint32_t pathway_count;
    uint32_t word_memory_count;
    uint64_t tick_count;
} file_header_t;

int persistence_save(engram_t *eng, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }

    engram_mutex_lock(&eng->state_mutex);

    file_header_t header = {
        .magic = ENGRAM_FILE_MAGIC,
        .version = ENGRAM_FILE_VERSION,
        .neuron_count = eng->neurons.count,
        .synapse_count = eng->synapses.count,
        .pathway_count = eng->pathways.count,
        .word_memory_count = eng->word_memory.count,
        .tick_count = eng->brainstem.tick_count
    };

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        engram_mutex_unlock(&eng->state_mutex);
        fclose(f);
        return -1;
    }

    for (uint32_t i = 0; i < eng->synapses.capacity; i++) {
        engram_synapse_t *s = &eng->synapses.synapses[i];
        if (s->weight > 0.001f) {
            if (fwrite(s, sizeof(engram_synapse_t), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
        }
    }

    for (uint32_t i = 0; i < eng->pathways.capacity; i++) {
        engram_pathway_t *p = pathway_get(eng, i);
        if (p) {
            if (fwrite(&p->neuron_count, sizeof(uint32_t), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
            if (fwrite(p->neuron_ids, sizeof(uint32_t), p->neuron_count, f) != p->neuron_count) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
            if (fwrite(&p->strength, sizeof(float), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
            if (fwrite(&p->activation_count, sizeof(uint32_t), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
            if (fwrite(&p->content_hash, sizeof(uint64_t), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
        }
    }

    for (uint32_t i = 0; i < eng->word_memory.count; i++) {
        word_memory_entry_t *e = &eng->word_memory.entries[i];
        if (fwrite(e, sizeof(word_memory_entry_t), 1, f) != 1) {
            engram_mutex_unlock(&eng->state_mutex);
            fclose(f);
            return -1;
        }
    }

    for (uint32_t i = 0; i < eng->neurons.count; i++) {
        engram_neuron_t *n = &eng->neurons.neurons[i];
        if (n->outgoing_count > 0) {
            uint32_t id = i;
            if (fwrite(&id, sizeof(uint32_t), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
            if (fwrite(&n->outgoing_count, sizeof(uint16_t), 1, f) != 1) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
            if (fwrite(n->outgoing_synapses, sizeof(uint32_t), n->outgoing_count, f) != n->outgoing_count) {
                engram_mutex_unlock(&eng->state_mutex);
                fclose(f);
                return -1;
            }
        }
    }

    engram_mutex_unlock(&eng->state_mutex);
    fclose(f);
    return 0;
}

engram_t *persistence_load(const char *path, const engram_config_t *config) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    file_header_t header;
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        return NULL;
    }

    if (header.magic != ENGRAM_FILE_MAGIC || header.version != ENGRAM_FILE_VERSION) {
        fclose(f);
        return NULL;
    }

    engram_t *eng = engram_create(config);
    if (!eng) {
        fclose(f);
        return NULL;
    }

    engram_mutex_lock(&eng->state_mutex);

    for (uint32_t i = 0; i < header.synapse_count; i++) {
        engram_synapse_t s;
        if (fread(&s, sizeof(engram_synapse_t), 1, f) != 1) {
            break;
        }
        if (s.weight > 0.001f) {
            uint32_t idx = synapse_create(eng, s.pre_neuron_id, s.post_neuron_id, s.weight);
            if (idx != UINT32_MAX) {
                engram_synapse_t *ns = synapse_get(eng, idx);
                if (ns) {
                    ns->last_active_tick = s.last_active_tick;
                    ns->potentiation_count = s.potentiation_count;
                }
            }
        }
    }

    for (uint32_t i = 0; i < header.pathway_count; i++) {
        uint32_t neuron_count;
        if (fread(&neuron_count, sizeof(uint32_t), 1, f) != 1) {
            break;
        }

        uint32_t *neuron_ids = engram_alloc(eng, neuron_count * sizeof(uint32_t));
        if (!neuron_ids) {
            break;
        }

        if (fread(neuron_ids, sizeof(uint32_t), neuron_count, f) != neuron_count) {
            engram_free(eng, neuron_ids);
            break;
        }

        float strength;
        uint32_t activation_count;
        uint64_t content_hash;
        if (fread(&strength, sizeof(float), 1, f) != 1) {
            engram_free(eng, neuron_ids);
            break;
        }
        if (fread(&activation_count, sizeof(uint32_t), 1, f) != 1) {
            engram_free(eng, neuron_ids);
            break;
        }
        if (fread(&content_hash, sizeof(uint64_t), 1, f) != 1) {
            engram_free(eng, neuron_ids);
            break;
        }

        uint32_t pid = pathway_create_with_data(eng, neuron_ids, neuron_count, 0, 
                                                 ENGRAM_MODALITY_TEXT, content_hash);
        if (pid != UINT32_MAX) {
            engram_pathway_t *p = pathway_get(eng, pid);
            if (p) {
                p->strength = strength;
                p->activation_count = activation_count;
            }
        }

        engram_free(eng, neuron_ids);
    }

    if (header.word_memory_count > 0) {
        if (eng->word_memory.capacity < header.word_memory_count) {
            uint32_t new_cap = header.word_memory_count + 1024;
            word_memory_entry_t *new_entries = engram_realloc(eng, eng->word_memory.entries,
                                                               new_cap * sizeof(word_memory_entry_t));
            if (new_entries) {
                eng->word_memory.entries = new_entries;
                eng->word_memory.capacity = new_cap;
            }
        }

        for (uint32_t i = 0; i < header.word_memory_count; i++) {
            word_memory_entry_t e;
            if (fread(&e, sizeof(word_memory_entry_t), 1, f) != 1) {
                break;
            }
            if (eng->word_memory.count < eng->word_memory.capacity) {
                eng->word_memory.entries[eng->word_memory.count++] = e;
            }
        }
    }

    while (!feof(f)) {
        uint32_t neuron_id;
        uint16_t outgoing_count;
        if (fread(&neuron_id, sizeof(uint32_t), 1, f) != 1) break;
        if (fread(&outgoing_count, sizeof(uint16_t), 1, f) != 1) break;
        
        if (neuron_id < eng->neurons.capacity && outgoing_count > 0) {
            engram_neuron_t *n = &eng->neurons.neurons[neuron_id];
            if (n->outgoing_capacity < outgoing_count) {
                uint32_t *new_out = engram_realloc(eng, n->outgoing_synapses,
                                                    outgoing_count * sizeof(uint32_t));
                if (new_out) {
                    n->outgoing_synapses = new_out;
                    n->outgoing_capacity = outgoing_count;
                }
            }
            if (n->outgoing_capacity >= outgoing_count) {
                if (fread(n->outgoing_synapses, sizeof(uint32_t), outgoing_count, f) == outgoing_count) {
                    n->outgoing_count = outgoing_count;
                }
            } else {
                fseek(f, outgoing_count * sizeof(uint32_t), SEEK_CUR);
            }
        } else if (outgoing_count > 0) {
            fseek(f, outgoing_count * sizeof(uint32_t), SEEK_CUR);
        }
    }

    eng->brainstem.tick_count = header.tick_count;

    engram_mutex_unlock(&eng->state_mutex);
    fclose(f);

    return eng;
}
