#ifndef ENGRAM_H
#define ENGRAM_H

#include "types.h"

engram_config_t engram_config_default(void);
engram_t *engram_create(const engram_config_t *config);
void engram_destroy(engram_t *e);
engram_response_t engram_cue(engram_t *e, const char *input);
bool engram_gpu_available(engram_t *e);
size_t engram_neuron_count(engram_t *e);
size_t engram_synapse_count(engram_t *e);

engram_status_t engram_save(engram_t *e, const char *path);
engram_status_t engram_load(engram_t *e, const char *path);

#endif
