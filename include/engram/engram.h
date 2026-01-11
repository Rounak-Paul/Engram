#ifndef ENGRAM_H
#define ENGRAM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct engram engram_t;

typedef struct engram_config {
    uint32_t neuron_count;
    uint32_t synapse_pool_size;
    float learning_rate;
    float decay_rate;
    float inhibition_threshold;
    float activation_threshold;
    uint32_t hippocampus_tick_ms;
    uint32_t consolidation_tick_ms;
    int use_vulkan;
} engram_config_t;

typedef struct engram_response {
    char *text;
    size_t text_len;
    float confidence;
    float novelty;
} engram_response_t;

engram_config_t engram_config_default(void);
engram_t *engram_create(const engram_config_t *config);
void engram_destroy(engram_t *eng);
engram_response_t engram_cue(engram_t *eng, const char *input, size_t len);
void engram_response_free(engram_response_t *resp);
int engram_save(engram_t *eng, const char *path);
engram_t *engram_load(const char *path);

uint64_t engram_neuron_count(engram_t *eng);
uint64_t engram_synapse_count(engram_t *eng);
uint64_t engram_pathway_count(engram_t *eng);

#ifdef __cplusplus
}
#endif

#endif
