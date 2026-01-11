#include "../internal.h"
#include "../core.h"
#include "../system.h"
#include <stdio.h>
#include <string.h>

#define ENGRAM_FILE_MAGIC 0x454E4752
#define ENGRAM_FILE_VERSION 1

typedef struct file_header {
    uint32_t magic;
    uint32_t version;
    uint32_t neuron_count;
    uint32_t synapse_count;
    uint32_t pathway_count;
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
        .tick_count = eng->brainstem.tick_count
    };

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        engram_mutex_unlock(&eng->state_mutex);
        fclose(f);
        return -1;
    }

    if (fwrite(eng->neurons.neurons, sizeof(engram_neuron_t), eng->neurons.count, f) != eng->neurons.count) {
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

    if (header.neuron_count <= eng->neurons.capacity) {
        if (fread(eng->neurons.neurons, sizeof(engram_neuron_t), header.neuron_count, f) != header.neuron_count) {
            engram_mutex_unlock(&eng->state_mutex);
            fclose(f);
            engram_destroy(eng);
            return NULL;
        }
    }

    for (uint32_t i = 0; i < header.synapse_count; i++) {
        engram_synapse_t s;
        if (fread(&s, sizeof(engram_synapse_t), 1, f) != 1) {
            break;
        }
        synapse_create(eng, s.pre_neuron_id, s.post_neuron_id, s.weight);
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
        if (fread(&strength, sizeof(float), 1, f) != 1) {
            engram_free(eng, neuron_ids);
            break;
        }
        if (fread(&activation_count, sizeof(uint32_t), 1, f) != 1) {
            engram_free(eng, neuron_ids);
            break;
        }

        uint32_t pid = pathway_create(eng, neuron_ids, neuron_count, 0);
        if (pid != UINT32_MAX) {
            engram_pathway_t *p = pathway_get(eng, pid);
            if (p) {
                p->strength = strength;
                p->activation_count = activation_count;
            }
        }

        engram_free(eng, neuron_ids);
    }

    eng->brainstem.tick_count = header.tick_count;

    engram_mutex_unlock(&eng->state_mutex);
    fclose(f);

    return eng;
}
