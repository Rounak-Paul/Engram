#ifndef ENGRAM_CORE_H
#define ENGRAM_CORE_H

#include "internal.h"

int neuron_pool_init(engram_t *eng, uint32_t capacity);
void neuron_pool_destroy(engram_t *eng);
engram_neuron_t *neuron_get(engram_t *eng, uint32_t id);
int neuron_fire(engram_t *eng, uint32_t id, uint64_t tick);
void neuron_update(engram_t *eng, uint32_t id, float dt);
void neuron_stimulate(engram_t *eng, uint32_t id, float input);
void neuron_process_active(engram_t *eng, uint64_t tick, uint32_t max_iterations);
uint32_t neuron_count_active(engram_t *eng);

int synapse_pool_init(engram_t *eng, uint32_t initial_capacity);
void synapse_pool_destroy(engram_t *eng);
uint32_t synapse_create(engram_t *eng, uint32_t pre_id, uint32_t post_id, float weight);
void synapse_destroy(engram_t *eng, uint32_t idx);
engram_synapse_t *synapse_get(engram_t *eng, uint32_t idx);
void synapse_update_eligibility(engram_t *eng, uint32_t idx, float dt);
int synapse_prune_weak(engram_t *eng, float threshold);
uint32_t synapse_find(engram_t *eng, uint32_t pre_id, uint32_t post_id);

int cluster_pool_init(engram_t *eng);
void cluster_pool_destroy(engram_t *eng);
engram_cluster_t *cluster_get(engram_t *eng, uint32_t id);

int pathway_pool_init(engram_t *eng);
void pathway_pool_destroy(engram_t *eng);
uint32_t pathway_create(engram_t *eng, uint32_t *neuron_ids, uint32_t count, uint64_t tick);
uint32_t pathway_create_with_data(engram_t *eng, uint32_t *neuron_ids, uint32_t count, uint64_t tick,
                                  const void *data, size_t data_size, uint32_t modality, uint64_t content_hash);
void pathway_destroy(engram_t *eng, uint32_t idx);
engram_pathway_t *pathway_get(engram_t *eng, uint32_t idx);
void pathway_activate(engram_t *eng, uint32_t idx, uint64_t tick);
uint32_t pathway_find_matching(engram_t *eng, uint32_t *neuron_ids, uint32_t count, float threshold);
uint32_t pathway_find_by_hash(engram_t *eng, uint64_t content_hash);
void pathway_prune_weak(engram_t *eng, float min_strength);

#endif
