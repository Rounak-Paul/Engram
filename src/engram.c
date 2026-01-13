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
        .max_neurons = DEFAULT_MAX_NEURONS,
        .max_synapses = DEFAULT_MAX_SYNAPSES,
        .vector_dim = ENGRAM_DEFAULT_VECTOR_DIM,
        .decay_rate = DEFAULT_DECAY_RATE,
        .activation_threshold = DEFAULT_ACTIVATION_THRESHOLD,
        .learning_rate = DEFAULT_LEARNING_RATE,
        .noise_threshold = DEFAULT_NOISE_THRESHOLD,
        .storage_path = NULL,
        .use_mmap = false
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
    
    if (e->config.vector_dim == 0) {
        e->config.vector_dim = ENGRAM_DEFAULT_VECTOR_DIM;
    }
    if (e->config.vector_dim > ENGRAM_MAX_VECTOR_DIM) {
        e->config.vector_dim = ENGRAM_MAX_VECTOR_DIM;
    }
    
    const char *mmap_path = e->config.use_mmap ? e->config.storage_path : NULL;
    e->substrate = substrate_create(e->config.max_neurons, e->config.max_synapses, 
                                     e->config.vector_dim, mmap_path);
    if (!e->substrate.neurons) {
        free(e);
        return NULL;
    }
    
    e->wernicke = wernicke_create(e->config.vector_dim);
    if (!e->wernicke.char_hashes) {
        substrate_destroy(&e->substrate);
        free(e);
        return NULL;
    }
    
    e->hippocampus = hippocampus_create(ENGRAM_MAX_ACTIVATIONS * 10);
    e->cortex = cortex_create(e->config.max_neurons);
    e->hnsw = hnsw_create(e->config.max_neurons);
    
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
    
    pthread_rwlock_init(&e->rwlock, NULL);
    
#ifdef ENGRAM_VULKAN_ENABLED
    e->vulkan = vulkan_create();
    e->gpu_available = e->vulkan.initialized;
    if (e->gpu_available) {
        fprintf(stderr, "[engram] using Vulkan compute\n");
    } else {
        fprintf(stderr, "[engram] using CPU (Vulkan unavailable)\n");
    }
#else
    fprintf(stderr, "[engram] using CPU\n");
#endif
    
    return e;
}

void engram_destroy(engram_t *e) {
    if (!e) return;
    
    pthread_rwlock_destroy(&e->rwlock);
    
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
    hnsw_destroy(&e->hnsw);
    hippocampus_destroy(&e->hippocampus);
    wernicke_destroy(&e->wernicke);
    substrate_destroy(&e->substrate);
    
    free(e);
}

static float compute_noise_score(const float *v, size_t dim) {
    float variance = 0.0f;
    float mean = 0.0f;
    
    for (size_t i = 0; i < dim; i++) {
        mean += v[i];
    }
    mean /= (float)dim;
    
    for (size_t i = 0; i < dim; i++) {
        float diff = v[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)dim;
    
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

static neuron_t *find_or_create_neuron(engram_t *e, const float *embedding,
                                        const char *content, float threshold) {
    float best_sim = -1.0f;
    neuron_t *best_match = NULL;
    size_t dim = e->config.vector_dim;
    
    if (e->hnsw.count > 0) {
        size_t result_idx;
        float result_sim;
        size_t found = hnsw_search(&e->hnsw, &e->substrate, embedding,
                                   &result_idx, &result_sim, 1, 50);
        if (found > 0 && result_sim >= threshold) {
            return &e->substrate.neurons[result_idx];
        }
        if (found > 0 && result_sim > best_sim) {
            best_sim = result_sim;
            best_match = &e->substrate.neurons[result_idx];
        }
    }
    
    neuron_t *n = substrate_alloc_neuron(&e->substrate);
    if (!n) return NULL;
    
    memcpy(n->embedding, embedding, dim * sizeof(float));
    content_map_insert(&e->content_map, n->id, content);
    cortex_store(&e->cortex, n->id);
    
    size_t neuron_idx = n - e->substrate.neurons;
    hnsw_insert(&e->hnsw, &e->substrate, neuron_idx);
    
    if (best_match && best_sim >= threshold * 0.7f) {
        substrate_add_synapse(&e->substrate, n->id, best_match->id, best_sim);
        substrate_add_synapse(&e->substrate, best_match->id, n->id, best_sim * 0.5f);
    }
    
    return n;
}

static engram_response_t engram_cue_internal(engram_t *e, const char *input, bool readonly) {
    engram_response_t response = {0};
    
    size_t dim = e->config.vector_dim;
    float *query_embedding = malloc(dim * sizeof(float));
    if (!query_embedding) return response;
    
    wernicke_encode(&e->wernicke, input, query_embedding, dim);
    
    float noise = compute_noise_score(query_embedding, dim);
    if (noise < e->config.noise_threshold) {
        free(query_embedding);
        return response;
    }
    
    engram_id_t *result_ids = e->response_buf.ids;
    float *result_scores = e->response_buf.relevance;
    float *result_activations = e->response_buf.activations;
    const char **result_content = e->response_buf.content;
    
    size_t count = 0;
    
    if (e->hnsw.count > 0) {
        size_t result_indices[ENGRAM_MAX_ACTIVATIONS];
        count = hnsw_search(&e->hnsw, &e->substrate, query_embedding,
                            result_indices, result_scores, ENGRAM_MAX_ACTIVATIONS, 100);
        for (size_t i = 0; i < count; i++) {
            result_ids[i] = e->substrate.neurons[result_indices[i]].id;
        }
    }
    
    for (size_t i = 0; i < count; i++) {
        neuron_t *n = substrate_find_neuron(&e->substrate, result_ids[i]);
        result_activations[i] = n ? n->activation : 0.0f;
        result_content[i] = content_map_find(&e->content_map, result_ids[i]);
    }
    
    if (!readonly) {
        neuron_t *input_neuron = find_or_create_neuron(e, query_embedding, input,
                                                        e->config.activation_threshold);
        if (input_neuron) {
            propagate_activation(&e->substrate, input_neuron->id, 1.0f, 0.7f, 3);
            
            for (size_t i = 0; i < count && i < 8; i++) {
                if (result_scores[i] >= 0.4f) {
                    propagate_activation(&e->substrate, result_ids[i], result_scores[i], 0.7f, 2);
                }
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
            
            e->substrate.tick++;
            if (e->substrate.tick - e->substrate.decay_tick >= 100) {
                substrate_decay(&e->substrate, e->config.decay_rate);
                e->substrate.decay_tick = e->substrate.tick;
            }
        }
    }
    
    free(query_embedding);
    
    response.ids = result_ids;
    response.relevance = result_scores;
    response.activations = result_activations;
    response.content = result_content;
    response.count = count;
    
    return response;
}

engram_response_t engram_cue(engram_t *e, const char *input) {
    if (!e || !input || !*input) {
        return (engram_response_t){0};
    }
    
    pthread_rwlock_wrlock(&e->rwlock);
    engram_response_t response = engram_cue_internal(e, input, false);
    pthread_rwlock_unlock(&e->rwlock);
    
    return response;
}

engram_response_t engram_query(engram_t *e, const char *input) {
    if (!e || !input || !*input) {
        return (engram_response_t){0};
    }
    
    pthread_rwlock_rdlock(&e->rwlock);
    engram_response_t response = engram_cue_internal(e, input, true);
    pthread_rwlock_unlock(&e->rwlock);
    
    return response;
}

static size_t engram_bulk_insert_internal(engram_t *e, const float *embeddings, 
                                           const char **texts, size_t count) {
    size_t dim = e->config.vector_dim;
    size_t inserted = 0;
    
    for (size_t i = 0; i < count; i++) {
        const float *emb = embeddings + i * dim;
        const char *text = texts ? texts[i] : NULL;
        
        float noise = compute_noise_score(emb, dim);
        if (noise < e->config.noise_threshold) continue;
        
        neuron_t *existing = NULL;
        if (e->hnsw.count > 0) {
            size_t result_idx;
            float result_sim;
            size_t found = hnsw_search(&e->hnsw, &e->substrate, emb,
                                       &result_idx, &result_sim, 1, 50);
            if (found > 0 && result_sim >= e->config.activation_threshold) {
                existing = &e->substrate.neurons[result_idx];
            }
        }
        
        if (existing) continue;
        
        neuron_t *n = substrate_alloc_neuron(&e->substrate);
        if (!n) break;
        
        memcpy(n->embedding, emb, dim * sizeof(float));
        if (text) {
            content_map_insert(&e->content_map, n->id, text);
        }
        cortex_store(&e->cortex, n->id);
        
        size_t neuron_idx = n - e->substrate.neurons;
        hnsw_insert(&e->hnsw, &e->substrate, neuron_idx);
        
        inserted++;
    }
    
    return inserted;
}

size_t engram_bulk_insert(engram_t *e, const char **texts, size_t count) {
    if (!e || !texts || count == 0) return 0;
    
    size_t dim = e->config.vector_dim;
    float *embeddings = malloc(count * dim * sizeof(float));
    if (!embeddings) return 0;
    
    for (size_t i = 0; i < count; i++) {
        if (texts[i]) {
            wernicke_encode(&e->wernicke, texts[i], embeddings + i * dim, dim);
        } else {
            vec_zero(embeddings + i * dim, dim);
        }
    }
    
    pthread_rwlock_wrlock(&e->rwlock);
    size_t inserted = engram_bulk_insert_internal(e, embeddings, texts, count);
    pthread_rwlock_unlock(&e->rwlock);
    
    free(embeddings);
    return inserted;
}

size_t engram_bulk_insert_embeddings(engram_t *e, const float *embeddings, 
                                      const char **texts, size_t count) {
    if (!e || !embeddings || count == 0) return 0;
    
    pthread_rwlock_wrlock(&e->rwlock);
    size_t inserted = engram_bulk_insert_internal(e, embeddings, texts, count);
    pthread_rwlock_unlock(&e->rwlock);
    
    return inserted;
}

const char *engram_get_content(engram_t *e, engram_id_t id) {
    if (!e) return NULL;
    return content_map_find(&e->content_map, id);
}

engram_stats_t engram_stats(engram_t *e) {
    engram_stats_t s = {0};
    if (!e) return s;
    
    s.neuron_count = e->substrate.neuron_count;
    s.neuron_capacity = e->substrate.neuron_capacity;
    s.synapse_count = e->substrate.synapse_count;
    s.synapse_capacity = e->substrate.synapse_capacity;
    s.content_count = e->content_map.count;
    s.vector_dim = e->config.vector_dim;
    
    s.memory_neurons = e->substrate.neuron_capacity * sizeof(neuron_t);
    s.memory_synapses = e->substrate.synapse_capacity * sizeof(synapse_t);
    s.memory_embeddings = e->substrate.neuron_capacity * e->config.vector_dim * sizeof(float);
    
    size_t content_mem = e->content_map.capacity * sizeof(content_entry_t);
    content_mem += e->content_map.bucket_count * sizeof(size_t);
    content_mem += e->content_map.capacity * sizeof(size_t);
    for (size_t i = 0; i < e->content_map.count; i++) {
        if (e->content_map.entries[i].text) {
            content_mem += strlen(e->content_map.entries[i].text) + 1;
        }
    }
    s.memory_content = content_mem;
    
    s.memory_total = s.memory_neurons + s.memory_synapses + s.memory_content + s.memory_embeddings;
    s.memory_total += e->substrate.neuron_idx.bucket_count * sizeof(size_t);
    s.memory_total += e->substrate.neuron_capacity * sizeof(size_t);
    s.memory_total += e->substrate.synapse_idx.bucket_count * sizeof(uint32_t);
    s.memory_total += e->substrate.synapse_capacity * sizeof(uint32_t);
    
    s.device = e->gpu_available ? ENGRAM_DEVICE_VULKAN : ENGRAM_DEVICE_CPU;
    
    return s;
}

#define ENGRAM_FILE_MAGIC 0x454E4752414D0002ULL

engram_status_t engram_save(engram_t *e, const char *path) {
    if (!e || !path) return ENGRAM_ERR_INVALID;
    
    FILE *f = fopen(path, "wb");
    if (!f) return ENGRAM_ERR_IO;
    
    uint64_t magic = ENGRAM_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, f);
    
    size_t vector_dim = e->config.vector_dim;
    fwrite(&vector_dim, sizeof(size_t), 1, f);
    
    fwrite(&e->substrate.neuron_count, sizeof(size_t), 1, f);
    fwrite(&e->substrate.synapse_count, sizeof(size_t), 1, f);
    fwrite(&e->substrate.next_id, sizeof(engram_id_t), 1, f);
    fwrite(&e->substrate.tick, sizeof(uint64_t), 1, f);
    
    for (size_t i = 0; i < e->substrate.neuron_count; i++) {
        neuron_t *n = &e->substrate.neurons[i];
        fwrite(&n->id, sizeof(engram_id_t), 1, f);
        fwrite(n->embedding, sizeof(float), vector_dim, f);
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
    
    size_t file_vector_dim;
    fread(&file_vector_dim, sizeof(size_t), 1, f);
    
    if (file_vector_dim != e->config.vector_dim) {
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
        fread(n->embedding, sizeof(float), file_vector_dim, f);
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
