#ifndef ENGRAM_INTERNAL_H
#define ENGRAM_INTERNAL_H

#include "engram/types.h"
#include <pthread.h>

#ifdef ENGRAM_VULKAN_ENABLED
#include <vulkan/vulkan.h>
#endif

#define ENGRAM_HASH_SEED 0x9E3779B97F4A7C15ULL
#define HNSW_MAX_LEVEL 16

typedef struct neuron neuron_t;
typedef struct synapse synapse_t;
typedef struct substrate substrate_t;
typedef struct wernicke wernicke_t;
typedef struct hippocampus hippocampus_t;
typedef struct cortex cortex_t;
typedef struct vulkan_ctx vulkan_ctx_t;
typedef struct hnsw hnsw_t;
typedef struct hnsw_node hnsw_node_t;

struct neuron {
    engram_id_t id;
    float *embedding;
    float activation;
    float importance;
    uint64_t last_access;
    uint64_t access_count;
    uint32_t synapse_offset;
    uint32_t synapse_count;
};

struct synapse {
    engram_id_t source;
    engram_id_t target;
    float weight;
    uint64_t last_activation;
};

typedef struct synapse_index synapse_index_t;
struct synapse_index {
    uint32_t *buckets;
    uint32_t *next;
    size_t bucket_count;
};

typedef struct synapse_src_index synapse_src_index_t;
struct synapse_src_index {
    uint32_t *buckets;
    uint32_t *next;
    size_t bucket_count;
};

typedef struct neuron_index neuron_index_t;
struct neuron_index {
    size_t *buckets;
    size_t *next;
    size_t bucket_count;
};

typedef struct content_entry content_entry_t;
struct content_entry {
    engram_id_t id;
    char *text;
};

typedef struct content_map content_map_t;
struct content_map {
    content_entry_t *entries;
    size_t count;
    size_t capacity;
    size_t *buckets;
    size_t *next;
    size_t bucket_count;
};

struct substrate {
    neuron_t *neurons;
    size_t neuron_count;
    size_t neuron_capacity;
    float *embeddings;
    size_t vector_dim;
    synapse_t *synapses;
    size_t synapse_count;
    size_t synapse_capacity;
    engram_id_t next_id;
    uint64_t tick;
    uint64_t decay_tick;
    neuron_index_t neuron_idx;
    synapse_index_t synapse_idx;
    synapse_src_index_t synapse_src_idx;
    int mmap_fd;
    void *mmap_base;
    size_t mmap_size;
};

struct wernicke {
    uint32_t *char_hashes;
    size_t hash_table_size;
};

struct hippocampus {
    engram_id_t *recent_activations;
    size_t recent_count;
    size_t recent_capacity;
    float consolidation_threshold;
};

struct cortex {
    engram_id_t *index;
    size_t index_size;
    float *similarity_cache;
    size_t cache_size;
};

struct hnsw_node {
    size_t neuron_idx;
    uint32_t *neighbors[HNSW_MAX_LEVEL];
    uint8_t neighbor_count[HNSW_MAX_LEVEL];
    uint8_t level;
};

struct hnsw {
    hnsw_node_t *nodes;
    size_t count;
    size_t capacity;
    size_t entry_point;
    uint8_t max_level;
};

#ifdef ENGRAM_VULKAN_ENABLED
struct vulkan_ctx {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue compute_queue;
    uint32_t compute_family;
    VkCommandPool command_pool;
    VkDescriptorPool descriptor_pool;
    VkBuffer neuron_buffer;
    VkDeviceMemory neuron_memory;
    VkBuffer query_buffer;
    VkDeviceMemory query_memory;
    VkBuffer result_buffer;
    VkDeviceMemory result_memory;
    VkShaderModule similarity_shader;
    VkPipeline similarity_pipeline;
    VkPipelineLayout similarity_layout;
    VkDescriptorSetLayout descriptor_layout;
    size_t buffer_capacity;
    size_t synced_count;
    bool initialized;
};
#endif

typedef struct response_buffer response_buffer_t;
struct response_buffer {
    engram_id_t ids[ENGRAM_MAX_ACTIVATIONS];
    float relevance[ENGRAM_MAX_ACTIVATIONS];
    float activations[ENGRAM_MAX_ACTIVATIONS];
    const char *content[ENGRAM_MAX_ACTIVATIONS];
};

struct engram {
    engram_config_t config;
    substrate_t substrate;
    wernicke_t wernicke;
    hippocampus_t hippocampus;
    cortex_t cortex;
    hnsw_t hnsw;
    content_map_t content_map;
    response_buffer_t response_buf;
    pthread_rwlock_t rwlock;
#ifdef ENGRAM_VULKAN_ENABLED
    vulkan_ctx_t vulkan;
#endif
    bool gpu_available;
};

substrate_t substrate_create(size_t neuron_cap, size_t synapse_cap, size_t vector_dim, const char *mmap_path);
void substrate_destroy(substrate_t *s);
neuron_t *substrate_alloc_neuron(substrate_t *s);
neuron_t *substrate_alloc_neuron_raw(substrate_t *s);
synapse_t *substrate_alloc_synapse(substrate_t *s);
synapse_t *substrate_alloc_synapse_raw(substrate_t *s);
synapse_t *substrate_add_synapse(substrate_t *s, engram_id_t src, engram_id_t dst, float weight);
void substrate_rebuild_indices(substrate_t *s);
neuron_t *substrate_find_neuron(substrate_t *s, engram_id_t id);
synapse_t *substrate_find_synapse(substrate_t *s, engram_id_t src, engram_id_t dst);
void substrate_for_each_synapse(substrate_t *s, engram_id_t src, void (*fn)(synapse_t*, void*), void *ctx);
void substrate_decay(substrate_t *s, float rate);
void substrate_prune(substrate_t *s, float threshold);

wernicke_t wernicke_create(size_t vector_dim);
void wernicke_destroy(wernicke_t *w);
void wernicke_encode(wernicke_t *w, const char *text, float *out, size_t dim);
void wernicke_tokenize(wernicke_t *w, const char *text, uint32_t *tokens, size_t *count, size_t max_tokens);

hippocampus_t hippocampus_create(size_t capacity);
void hippocampus_destroy(hippocampus_t *h);
void hippocampus_record(hippocampus_t *h, engram_id_t id);
void hippocampus_consolidate(hippocampus_t *h, substrate_t *s, float threshold);

cortex_t cortex_create(size_t capacity);
void cortex_destroy(cortex_t *c);
void cortex_store(cortex_t *c, engram_id_t id);
size_t cortex_query(cortex_t *c, substrate_t *s, const float *query, 
                    engram_id_t *results, float *scores, size_t max_results, float threshold);

hnsw_t hnsw_create(size_t capacity);
void hnsw_destroy(hnsw_t *h);
void hnsw_insert(hnsw_t *h, substrate_t *s, size_t neuron_idx);
size_t hnsw_search(hnsw_t *h, substrate_t *s, const float *query,
                   size_t *results, float *distances, size_t k, size_t ef);

void propagate_activation(substrate_t *s, engram_id_t source, float activation, float decay, size_t depth);
void propagate_learning(substrate_t *s, engram_id_t *active, size_t count, float rate);

#ifdef ENGRAM_VULKAN_ENABLED
vulkan_ctx_t vulkan_create(void);
void vulkan_destroy(vulkan_ctx_t *v);
#endif

uint64_t hash_bytes(const void *data, size_t len);
uint32_t hash_string(const char *str, size_t len);
float randf(void);
uint64_t rand64(void);
void vec_zero(float *v, size_t dim);
void vec_add(float *dst, const float *a, const float *b, size_t dim);
void vec_scale(float *v, float s, size_t dim);
void vec_normalize(float *v, size_t dim);
float vec_dot(const float *a, const float *b, size_t dim);
float vec_magnitude(const float *v, size_t dim);
void vec_lerp(float *dst, const float *a, const float *b, float t, size_t dim);

#endif
