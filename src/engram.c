#include "engram/engram.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#define DEFAULT_MAX_NEURONS 100000
#define DEFAULT_MAX_SYNAPSES 1000000
#define DEFAULT_DECAY_RATE 0.001f
#define DEFAULT_ACTIVATION_THRESHOLD 0.95f
#define DEFAULT_LEARNING_RATE 0.1f
#define DEFAULT_NOISE_THRESHOLD 0.01f

engram_config_t engram_config_default(void) {
    return (engram_config_t){
        .enable_gpu = true,
        .max_neurons = DEFAULT_MAX_NEURONS,
        .max_synapses = DEFAULT_MAX_SYNAPSES,
        .decay_rate = DEFAULT_DECAY_RATE,
        .activation_threshold = DEFAULT_ACTIVATION_THRESHOLD,
        .learning_rate = DEFAULT_LEARNING_RATE,
        .noise_threshold = DEFAULT_NOISE_THRESHOLD
    };
}

engram_t *engram_create(const engram_config_t *config) {
    engram_t *e = calloc(1, sizeof(engram_t));
    if (!e) return NULL;
    
    if (config) {
        e->config = *config;
    } else {
        e->config = engram_config_default();
    }
    
    e->substrate = substrate_create(e->config.max_neurons, e->config.max_synapses);
    if (!e->substrate.neurons) {
        free(e);
        return NULL;
    }
    
    e->wernicke = wernicke_create();
    if (!e->wernicke.char_hashes) {
        substrate_destroy(&e->substrate);
        free(e);
        return NULL;
    }
    
    e->hippocampus = hippocampus_create(ENGRAM_MAX_ACTIVATIONS * 10);
    e->cortex = cortex_create(e->config.max_neurons);
    
    e->content_map.capacity = e->config.max_neurons;
    e->content_map.bucket_count = 16384;
    e->content_map.entries = calloc(e->content_map.capacity, sizeof(content_entry_t));
    e->content_map.buckets = malloc(e->content_map.bucket_count * sizeof(size_t));
    e->content_map.next = malloc(e->content_map.capacity * sizeof(size_t));
    for (size_t i = 0; i < e->content_map.bucket_count; i++) {
        e->content_map.buckets[i] = SIZE_MAX;
    }
    for (size_t i = 0; i < e->content_map.capacity; i++) {
        e->content_map.next[i] = SIZE_MAX;
    }
    
#ifdef ENGRAM_VULKAN_ENABLED
    if (e->config.enable_gpu) {
        e->vulkan = vulkan_create();
        e->gpu_available = e->vulkan.initialized;
    }
#endif
    
    return e;
}

void engram_destroy(engram_t *e) {
    if (!e) return;
    
#ifdef ENGRAM_VULKAN_ENABLED
    vulkan_destroy(&e->vulkan);
#endif
    
    for (size_t i = 0; i < e->content_map.count; i++) {
        free(e->content_map.entries[i].text);
    }
    free(e->content_map.entries);
    free(e->content_map.buckets);
    free(e->content_map.next);
    
    cortex_destroy(&e->cortex);
    hippocampus_destroy(&e->hippocampus);
    wernicke_destroy(&e->wernicke);
    substrate_destroy(&e->substrate);
    
    free(e);
}

static float compute_noise_score(const engram_vec_t v) {
    float variance = 0.0f;
    float mean = 0.0f;
    
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        mean += v[i];
    }
    mean /= ENGRAM_VECTOR_DIM;
    
    for (int i = 0; i < ENGRAM_VECTOR_DIM; i++) {
        float diff = v[i] - mean;
        variance += diff * diff;
    }
    variance /= ENGRAM_VECTOR_DIM;
    
    return sqrtf(variance);
}

static void content_map_insert(content_map_t *m, engram_id_t id, const char *text) {
    if (!text || m->count >= m->capacity) return;
    
    size_t idx = m->count++;
    m->entries[idx].id = id;
    size_t len = strlen(text);
    if (len >= ENGRAM_MAX_CONTENT_LEN) len = ENGRAM_MAX_CONTENT_LEN - 1;
    m->entries[idx].text = malloc(len + 1);
    if (m->entries[idx].text) {
        memcpy(m->entries[idx].text, text, len);
        m->entries[idx].text[len] = '\0';
    }
    
    size_t bucket = id % m->bucket_count;
    m->next[idx] = m->buckets[bucket];
    m->buckets[bucket] = idx;
}

static const char *content_map_find(content_map_t *m, engram_id_t id) {
    size_t bucket = id % m->bucket_count;
    size_t idx = m->buckets[bucket];
    while (idx != SIZE_MAX) {
        if (m->entries[idx].id == id) return m->entries[idx].text;
        idx = m->next[idx];
    }
    return NULL;
}

static neuron_t *find_or_create_neuron(engram_t *e, const engram_vec_t embedding,
                                        const char *content, float threshold) {
    float best_sim = -1.0f;
    neuron_t *best_match = NULL;
    
#ifdef ENGRAM_VULKAN_ENABLED
    if (e->gpu_available && e->vulkan.synced_count > 0) {
        engram_id_t result_id;
        float result_score;
        size_t found = vulkan_similarity_search(&e->vulkan, &e->substrate, embedding,
                                                 &result_id, &result_score, 1, threshold);
        if (found > 0) {
            for (size_t i = 0; i < e->substrate.neuron_count; i++) {
                if (e->substrate.neurons[i].id == result_id) {
                    return &e->substrate.neurons[i];
                }
            }
        }
        
        for (size_t i = e->vulkan.synced_count; i < e->substrate.neuron_count; i++) {
            neuron_t *n = &e->substrate.neurons[i];
            float sim = vec_dot(embedding, n->embedding);
            if (sim > best_sim) {
                best_sim = sim;
                best_match = n;
            }
        }
        
        if (best_match && best_sim >= threshold) {
            return best_match;
        }
    } else
#endif
    {
        for (size_t i = 0; i < e->substrate.neuron_count; i++) {
            neuron_t *n = &e->substrate.neurons[i];
            float sim = vec_dot(embedding, n->embedding);
            if (sim > best_sim) {
                best_sim = sim;
                best_match = n;
            }
        }
        
        if (best_match && best_sim >= threshold) {
            return best_match;
        }
    }
    
    neuron_t *n = substrate_alloc_neuron(&e->substrate);
    if (!n) return NULL;
    
    memcpy(n->embedding, embedding, sizeof(engram_vec_t));
    content_map_insert(&e->content_map, n->id, content);
    cortex_store(&e->cortex, n->id);
    
    if (best_match && best_sim >= threshold * 0.7f) {
        synapse_t *syn = substrate_alloc_synapse(&e->substrate);
        if (syn) {
            syn->source = n->id;
            syn->target = best_match->id;
            syn->weight = best_sim;
            syn->last_activation = e->substrate.tick;
        }
        syn = substrate_alloc_synapse(&e->substrate);
        if (syn) {
            syn->source = best_match->id;
            syn->target = n->id;
            syn->weight = best_sim * 0.5f;
            syn->last_activation = e->substrate.tick;
        }
    }
    
    return n;
}

engram_response_t engram_cue(engram_t *e, const char *input) {
    engram_response_t response = {0};
    
    if (!e || !input || !*input) {
        return response;
    }
    
#ifdef ENGRAM_VULKAN_ENABLED
    if (e->gpu_available) {
        size_t unsynced = e->substrate.neuron_count - e->vulkan.synced_count;
        if (unsynced > 50) {
            vulkan_sync_neurons(&e->vulkan, &e->substrate);
        }
    }
#endif
    
    engram_vec_t query_embedding;
    wernicke_encode(&e->wernicke, input, query_embedding);
    
    float noise = compute_noise_score(query_embedding);
    if (noise < e->config.noise_threshold) {
        return response;
    }
    
    neuron_t *input_neuron = find_or_create_neuron(e, query_embedding, input,
                                                    e->config.activation_threshold);
    if (!input_neuron) {
        return response;
    }
    
    engram_id_t *result_ids = e->response_buf.ids;
    float *result_scores = e->response_buf.relevance;
    float *result_activations = e->response_buf.activations;
    const char **result_content = e->response_buf.content;
    
    size_t count = 0;
#ifdef ENGRAM_VULKAN_ENABLED
    if (e->gpu_available && e->vulkan.synced_count > 0) {
        count = vulkan_similarity_search(&e->vulkan, &e->substrate, query_embedding,
                                          result_ids, result_scores, ENGRAM_MAX_ACTIVATIONS, 0.3f);
    } else
#endif
    {
        count = cortex_query(&e->cortex, &e->substrate, query_embedding,
                             result_ids, result_scores, ENGRAM_MAX_ACTIVATIONS, 0.3f);
    }
    
    propagate_activation(&e->substrate, input_neuron->id, 1.0f, 0.7f, 3);
    
    for (size_t i = 0; i < count && i < 8; i++) {
        if (result_scores[i] >= 0.4f) {
            propagate_activation(&e->substrate, result_ids[i], result_scores[i], 0.7f, 2);
        }
    }
    
    for (size_t i = 0; i < count; i++) {
        neuron_t *n = substrate_find_neuron(&e->substrate, result_ids[i]);
        result_activations[i] = n ? n->activation : 0.0f;
        result_content[i] = content_map_find(&e->content_map, result_ids[i]);
    }
    
    engram_id_t active_neurons[8];
    size_t active_count = 0;
    
    active_neurons[active_count++] = input_neuron->id;
    
    for (size_t i = 0; i < count && active_count < 4; i++) {
        if (result_scores[i] >= 0.5f) {
            active_neurons[active_count++] = result_ids[i];
            hippocampus_record(&e->hippocampus, result_ids[i]);
        }
    }
    
    if (active_count > 1) {
        propagate_learning(&e->substrate, active_neurons, active_count, e->config.learning_rate);
    }
    
    hippocampus_consolidate(&e->hippocampus, &e->substrate, e->config.activation_threshold);
    
    if (e->substrate.synapse_count > e->config.max_synapses * 0.9) {
        substrate_prune(&e->substrate, e->config.noise_threshold);
    }
    
    substrate_decay(&e->substrate, e->config.decay_rate * 0.5f);
    
    response.ids = result_ids;
    response.relevance = result_scores;
    response.activations = result_activations;
    response.content = result_content;
    response.count = count;
    
    return response;
}

const char *engram_get_content(engram_t *e, engram_id_t id) {
    if (!e) return NULL;
    return content_map_find(&e->content_map, id);
}

bool engram_gpu_available(engram_t *e) {
    return e ? e->gpu_available : false;
}

size_t engram_neuron_count(engram_t *e) {
    return e ? e->substrate.neuron_count : 0;
}

size_t engram_synapse_count(engram_t *e) {
    return e ? e->substrate.synapse_count : 0;
}

#define ENGRAM_FILE_MAGIC 0x454E4752414D0001ULL

engram_status_t engram_save(engram_t *e, const char *path) {
    if (!e || !path) return ENGRAM_ERR_INVALID;
    
    FILE *f = fopen(path, "wb");
    if (!f) return ENGRAM_ERR_IO;
    
    uint64_t magic = ENGRAM_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, f);
    
    fwrite(&e->substrate.neuron_count, sizeof(size_t), 1, f);
    fwrite(&e->substrate.synapse_count, sizeof(size_t), 1, f);
    fwrite(&e->substrate.next_id, sizeof(engram_id_t), 1, f);
    fwrite(&e->substrate.tick, sizeof(uint64_t), 1, f);
    
    for (size_t i = 0; i < e->substrate.neuron_count; i++) {
        neuron_t *n = &e->substrate.neurons[i];
        fwrite(&n->id, sizeof(engram_id_t), 1, f);
        fwrite(n->embedding, sizeof(engram_vec_t), 1, f);
        fwrite(&n->activation, sizeof(float), 1, f);
        fwrite(&n->importance, sizeof(float), 1, f);
        fwrite(&n->last_access, sizeof(uint64_t), 1, f);
        fwrite(&n->access_count, sizeof(uint64_t), 1, f);
    }
    
    for (size_t i = 0; i < e->substrate.synapse_count; i++) {
        synapse_t *s = &e->substrate.synapses[i];
        fwrite(&s->source, sizeof(engram_id_t), 1, f);
        fwrite(&s->target, sizeof(engram_id_t), 1, f);
        fwrite(&s->weight, sizeof(float), 1, f);
        fwrite(&s->last_activation, sizeof(uint64_t), 1, f);
    }
    
    fwrite(&e->cortex.index_size, sizeof(size_t), 1, f);
    fwrite(e->cortex.index, sizeof(engram_id_t), e->cortex.index_size, f);
    
    fwrite(&e->content_map.count, sizeof(size_t), 1, f);
    for (size_t i = 0; i < e->content_map.count; i++) {
        fwrite(&e->content_map.entries[i].id, sizeof(engram_id_t), 1, f);
        size_t len = e->content_map.entries[i].text ? strlen(e->content_map.entries[i].text) : 0;
        fwrite(&len, sizeof(size_t), 1, f);
        if (len > 0) {
            fwrite(e->content_map.entries[i].text, 1, len, f);
        }
    }
    
    fclose(f);
    return ENGRAM_OK;
}

engram_status_t engram_load(engram_t *e, const char *path) {
    if (!e || !path) return ENGRAM_ERR_INVALID;
    
    FILE *f = fopen(path, "rb");
    if (!f) return ENGRAM_ERR_IO;
    
    uint64_t magic;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != ENGRAM_FILE_MAGIC) {
        fclose(f);
        return ENGRAM_ERR_INVALID;
    }
    
    size_t neuron_count, synapse_count;
    fread(&neuron_count, sizeof(size_t), 1, f);
    fread(&synapse_count, sizeof(size_t), 1, f);
    fread(&e->substrate.next_id, sizeof(engram_id_t), 1, f);
    fread(&e->substrate.tick, sizeof(uint64_t), 1, f);
    
    for (size_t i = 0; i < neuron_count; i++) {
        neuron_t *n = substrate_alloc_neuron_raw(&e->substrate);
        if (!n) {
            fclose(f);
            return ENGRAM_ERR_MEMORY;
        }
        fread(&n->id, sizeof(engram_id_t), 1, f);
        fread(n->embedding, sizeof(engram_vec_t), 1, f);
        fread(&n->activation, sizeof(float), 1, f);
        fread(&n->importance, sizeof(float), 1, f);
        fread(&n->last_access, sizeof(uint64_t), 1, f);
        fread(&n->access_count, sizeof(uint64_t), 1, f);
    }
    
    for (size_t i = 0; i < synapse_count; i++) {
        synapse_t *s = substrate_alloc_synapse_raw(&e->substrate);
        if (!s) {
            fclose(f);
            return ENGRAM_ERR_MEMORY;
        }
        fread(&s->source, sizeof(engram_id_t), 1, f);
        fread(&s->target, sizeof(engram_id_t), 1, f);
        fread(&s->weight, sizeof(float), 1, f);
        fread(&s->last_activation, sizeof(uint64_t), 1, f);
    }
    
    substrate_rebuild_indices(&e->substrate);
    
    size_t index_size;
    fread(&index_size, sizeof(size_t), 1, f);
    for (size_t i = 0; i < index_size; i++) {
        engram_id_t id;
        fread(&id, sizeof(engram_id_t), 1, f);
        cortex_store(&e->cortex, id);
    }
    
    size_t content_count;
    if (fread(&content_count, sizeof(size_t), 1, f) == 1) {
        for (size_t i = 0; i < content_count; i++) {
            engram_id_t id;
            size_t len;
            fread(&id, sizeof(engram_id_t), 1, f);
            fread(&len, sizeof(size_t), 1, f);
            char *text = NULL;
            if (len > 0 && len < ENGRAM_MAX_CONTENT_LEN) {
                text = malloc(len + 1);
                if (text) {
                    fread(text, 1, len, f);
                    text[len] = '\0';
                }
            }
            if (e->content_map.count < e->content_map.capacity) {
                size_t idx = e->content_map.count++;
                e->content_map.entries[idx].id = id;
                e->content_map.entries[idx].text = text;
                size_t bucket = id % e->content_map.bucket_count;
                e->content_map.next[idx] = e->content_map.buckets[bucket];
                e->content_map.buckets[bucket] = idx;
            } else {
                free(text);
            }
        }
    }
    
    fclose(f);
    return ENGRAM_OK;
}
