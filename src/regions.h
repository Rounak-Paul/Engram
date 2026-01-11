#ifndef ENGRAM_REGIONS_H
#define ENGRAM_REGIONS_H

#include "internal.h"

int brainstem_init(engram_t *eng);
void brainstem_destroy(engram_t *eng);
void brainstem_start(engram_t *eng);
void brainstem_stop(engram_t *eng);
void *brainstem_thread_fn(void *arg);

int thalamus_init(engram_t *eng);
void thalamus_destroy(engram_t *eng);
int thalamus_gate_cue(engram_t *eng, const engram_cue_t *cue);

int hippocampus_init(engram_t *eng);
void hippocampus_destroy(engram_t *eng);
int hippocampus_store_trace(engram_t *eng, const engram_cue_t *cue, uint32_t *neuron_ids, uint32_t count, uint64_t tick);
memory_trace_t *hippocampus_find_by_hash(engram_t *eng, uint64_t content_hash);
memory_trace_t *hippocampus_find_by_pattern(engram_t *eng, uint32_t *neuron_ids, uint32_t count, float threshold);
int hippocampus_get_replay_batch(engram_t *eng, memory_trace_t ***out_traces, uint32_t *out_count, uint32_t batch_size);
void hippocampus_mark_replayed(engram_t *eng, memory_trace_t *trace);

int amygdala_init(engram_t *eng);
void amygdala_destroy(engram_t *eng);

int neocortex_init(engram_t *eng);
void neocortex_destroy(engram_t *eng);
int neocortex_consolidate_pathway(engram_t *eng, uint32_t pathway_id);
uint32_t neocortex_find_pathway(engram_t *eng, uint32_t *neuron_ids, uint32_t count);

#endif
