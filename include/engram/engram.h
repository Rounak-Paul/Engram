#ifndef ENGRAM_H
#define ENGRAM_H

#include "types.h"

engram_config_t engram_config_default(void);
engram_t *engram_create(const engram_config_t *config);
void engram_destroy(engram_t *e);

engram_response_t engram_cue(engram_t *e, const char *input);
engram_response_t engram_query(engram_t *e, const char *input);

engram_stats_t engram_stats(engram_t *e);

size_t engram_bulk_insert(engram_t *e, const char **texts, size_t count);
size_t engram_bulk_insert_embeddings(engram_t *e, const float *embeddings, 
                                      const char **texts, size_t count);

engram_status_t engram_save(engram_t *e, const char *path);
engram_status_t engram_load(engram_t *e, const char *path);

#endif
