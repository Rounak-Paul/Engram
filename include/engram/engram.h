#ifndef ENGRAM_H
#define ENGRAM_H

#include "engram/types.h"
#include "engram/config.h"
#include "engram/allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

engram_t *engram_create(const engram_config_t *config);
void engram_destroy(engram_t *eng);

int engram_stimulate(engram_t *eng, const engram_cue_t *cue);
int engram_recall(engram_t *eng, const engram_cue_t *cue, engram_recall_t *result);
void engram_recall_free(engram_recall_t *result);
int engram_associate(engram_t *eng, const engram_cue_t *cues, size_t count);

int engram_set_arousal(engram_t *eng, engram_arousal_t state);
engram_arousal_t engram_get_arousal(engram_t *eng);
int engram_set_arousal_auto(engram_t *eng, int enable);

int engram_set_resource_limits(engram_t *eng, const engram_resource_limits_t *limits);
void engram_get_resource_usage(engram_t *eng, engram_resource_usage_t *usage);

int engram_save(engram_t *eng, const char *path);
engram_t *engram_load(const char *path, const engram_config_t *config);
int engram_checkpoint(engram_t *eng);

void engram_stats(engram_t *eng, engram_stats_t *stats);
size_t engram_pathway_count(engram_t *eng);
float engram_memory_pressure(engram_t *eng);
uint64_t engram_tick_count(engram_t *eng);
float engram_current_tick_rate(engram_t *eng);

#ifdef __cplusplus
}
#endif

#endif
