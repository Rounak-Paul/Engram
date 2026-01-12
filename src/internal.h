#ifndef ENGRAM_INTERNAL_H
#define ENGRAM_INTERNAL_H

#include "engram/types.h"

#ifdef ENGRAM_VULKAN_ENABLED
#include <vulkan/vulkan.h>
#endif

#define ENGRAM_HASH_SEED 0x9E3779B97F4A7C15ULL

typedef struct neuron neuron_t;
typedef struct synapse synapse_t;
typedef struct substrate substrate_t;
typedef struct wernicke wernicke_t;
typedef struct hippocampus hippocampus_t;
typedef struct cortex cortex_t;
typedef struct vulkan_ctx vulkan_ctx_t;

struct neuron {
    engram_id_t id;
    engram_vec_t embedding;
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

struct substrate {
    neuron_t *neurons;
    size_t neuron_count;
    size_t neuron_capacity;
    synapse_t *synapses;
    size_t synapse_count;
    size_t synapse_capacity;
    engram_id_t next_id;
    uint64_t tick;
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
    VkBuffer result_buffer;
    VkDeviceMemory result_memory;
    VkShaderModule similarity_shader;
    VkPipeline similarity_pipeline;
    VkPipelineLayout similarity_layout;
    VkDescriptorSetLayout descriptor_layout;
    size_t buffer_capacity;
    bool initialized;
};
#endif

struct engram {
    engram_config_t config;
    substrate_t substrate;
    wernicke_t wernicke;
    hippocampus_t hippocampus;
    cortex_t cortex;
#ifdef ENGRAM_VULKAN_ENABLED
    vulkan_ctx_t vulkan;
#endif
    bool gpu_available;
};

substrate_t substrate_create(size_t neuron_cap, size_t synapse_cap);
void substrate_destroy(substrate_t *s);
neuron_t *substrate_alloc_neuron(substrate_t *s);
synapse_t *substrate_alloc_synapse(substrate_t *s);
neuron_t *substrate_find_neuron(substrate_t *s, engram_id_t id);
void substrate_decay(substrate_t *s, float rate);
void substrate_prune(substrate_t *s, float threshold);

wernicke_t wernicke_create(void);
void wernicke_destroy(wernicke_t *w);
void wernicke_encode(wernicke_t *w, const char *text, engram_vec_t out);
void wernicke_tokenize(wernicke_t *w, const char *text, uint32_t *tokens, size_t *count, size_t max_tokens);

hippocampus_t hippocampus_create(size_t capacity);
void hippocampus_destroy(hippocampus_t *h);
void hippocampus_record(hippocampus_t *h, engram_id_t id);
void hippocampus_consolidate(hippocampus_t *h, substrate_t *s, float threshold);

cortex_t cortex_create(size_t capacity);
void cortex_destroy(cortex_t *c);
void cortex_store(cortex_t *c, engram_id_t id);
size_t cortex_query(cortex_t *c, substrate_t *s, const engram_vec_t query, 
                    engram_id_t *results, float *scores, size_t max_results, float threshold);

void propagate_activation(substrate_t *s, engram_id_t source, float activation, float decay, size_t depth);
void propagate_learning(substrate_t *s, engram_id_t *active, size_t count, float rate);

#ifdef ENGRAM_VULKAN_ENABLED
vulkan_ctx_t vulkan_create(void);
void vulkan_destroy(vulkan_ctx_t *v);
bool vulkan_sync_neurons(vulkan_ctx_t *v, substrate_t *s);
size_t vulkan_similarity_search(vulkan_ctx_t *v, const engram_vec_t query, 
                                 engram_id_t *results, float *scores, size_t max_results, float threshold);
#endif

uint64_t hash_bytes(const void *data, size_t len);
uint32_t hash_string(const char *str, size_t len);
float randf(void);
uint64_t rand64(void);
void vec_zero(engram_vec_t v);
void vec_add(engram_vec_t dst, const engram_vec_t a, const engram_vec_t b);
void vec_scale(engram_vec_t v, float s);
void vec_normalize(engram_vec_t v);
float vec_dot(const engram_vec_t a, const engram_vec_t b);
float vec_magnitude(const engram_vec_t v);
void vec_lerp(engram_vec_t dst, const engram_vec_t a, const engram_vec_t b, float t);

#endif
