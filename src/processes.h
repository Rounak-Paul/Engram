#ifndef ENGRAM_PROCESSES_H
#define ENGRAM_PROCESSES_H

#include "internal.h"

void encoding_init(void);
int encoding_cue_to_sdr(engram_t *eng, const engram_cue_t *cue, uint32_t *out_neuron_ids, uint32_t *out_count, uint32_t max_neurons);
int encoding_text_to_primary_neurons(engram_t *eng, const char *text, size_t size, uint32_t *out_neuron_ids, uint32_t *out_count, uint32_t max_neurons);
uint64_t encoding_hash(const void *data, size_t size);
float encoding_similarity(engram_t *eng, const engram_cue_t *cue1, const engram_cue_t *cue2);
int encoding_register_words(engram_t *eng, const char *text, size_t size);
int encoding_neurons_to_text(engram_t *eng, uint32_t *neuron_ids, uint32_t count, char *out_buffer, size_t buffer_size);

void plasticity_apply(engram_t *eng, uint64_t tick);
void plasticity_stdp(engram_t *eng, uint32_t synapse_idx, int32_t timing_diff, float eligibility);

void decay_step(engram_t *eng, uint64_t tick);
void decay_synapses(engram_t *eng, float dt);
void decay_activations(engram_t *eng, float dt);

void consolidation_step(engram_t *eng, uint64_t tick);
int consolidation_check_pathway(engram_t *eng, uint32_t pathway_id);

void replay_step(engram_t *eng, uint64_t tick);
void replay_activate_trace(engram_t *eng, uint32_t *neuron_ids, uint32_t count, float intensity);

int working_memory_init(engram_t *eng);
void working_memory_destroy(engram_t *eng);

#endif
