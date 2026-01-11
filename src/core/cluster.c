#include "internal.h"
#include "core.h"
#include <string.h>

int cluster_pool_init(engram_t *eng) {
    uint32_t cluster_count = (eng->config.neuron_count + eng->config.cluster_size - 1) / eng->config.cluster_size;
    if (cluster_count > ENGRAM_MAX_CLUSTERS) {
        cluster_count = ENGRAM_MAX_CLUSTERS;
    }

    eng->clusters.clusters = engram_alloc(eng, cluster_count * sizeof(engram_cluster_t));
    if (!eng->clusters.clusters) {
        return -1;
    }
    memset(eng->clusters.clusters, 0, cluster_count * sizeof(engram_cluster_t));

    for (uint32_t i = 0; i < cluster_count; i++) {
        eng->clusters.clusters[i].id = i;
        eng->clusters.clusters[i].neuron_start = i * eng->config.cluster_size;
        eng->clusters.clusters[i].neuron_count = eng->config.cluster_size;
        eng->clusters.clusters[i].synapse_start = 0;
        eng->clusters.clusters[i].synapse_count = 0;
        eng->clusters.clusters[i].avg_activation = 0.0f;
        eng->clusters.clusters[i].last_active_tick = 0;

        if (eng->clusters.clusters[i].neuron_start + eng->clusters.clusters[i].neuron_count > eng->config.neuron_count) {
            eng->clusters.clusters[i].neuron_count = eng->config.neuron_count - eng->clusters.clusters[i].neuron_start;
        }
    }

    eng->clusters.capacity = cluster_count;
    eng->clusters.count = cluster_count;

    return 0;
}

void cluster_pool_destroy(engram_t *eng) {
    if (eng->clusters.clusters) {
        engram_free(eng, eng->clusters.clusters);
        eng->clusters.clusters = NULL;
    }
    eng->clusters.capacity = 0;
    eng->clusters.count = 0;
}

engram_cluster_t *cluster_get(engram_t *eng, uint32_t id) {
    if (id >= eng->clusters.count) {
        return NULL;
    }
    return &eng->clusters.clusters[id];
}
