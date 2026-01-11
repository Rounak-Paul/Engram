#ifndef ENGRAM_SYSTEM_H
#define ENGRAM_SYSTEM_H

#include "internal.h"

uint64_t clock_get_tick(engram_t *eng);
float clock_get_rate(engram_t *eng);

void arousal_auto_update(engram_t *eng);
int arousal_set(engram_t *eng, engram_arousal_t state);
engram_arousal_t arousal_get(engram_t *eng);

int governor_init(engram_t *eng);
void governor_destroy(engram_t *eng);
void governor_update(engram_t *eng);
int governor_permits_tick(engram_t *eng);
void governor_set_limits(engram_t *eng, const engram_resource_limits_t *limits);

int persistence_save(engram_t *eng, const char *path);
engram_t *persistence_load(const char *path, const engram_config_t *config);

#endif
