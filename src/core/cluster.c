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

void cluster_update_activation(engram_t *eng, uint32_t id) {
    engram_cluster_t *c = cluster_get(eng, id);
    if (!c) {
        return;
    }

    float sum = 0.0f;
    for (uint32_t i = 0; i < c->neuron_count; i++) {
        uint32_t nid = c->neuron_start + i;
        engram_neuron_t *n = neuron_get(eng, nid);
        if (n && n->activation > 0.1f) {
            sum += n->activation;
        }
    }

    c->avg_activation = (c->neuron_count > 0) ? sum / c->neuron_count : 0.0f;
}

void cluster_lateral_inhibition(engram_t *eng, uint32_t id, float inhibition_strength) {
    engram_cluster_t *c = cluster_get(eng, id);
    if (!c) {
        return;
    }

    float max_activation = 0.0f;
    uint32_t winner_id = UINT32_MAX;

    for (uint32_t i = 0; i < c->neuron_count; i++) {
        uint32_t nid = c->neuron_start + i;
        engram_neuron_t *n = neuron_get(eng, nid);
        if (n && n->activation > max_activation) {
            max_activation = n->activation;
            winner_id = nid;
        }
    }

    if (winner_id == UINT32_MAX) {
        return;
    }

    for (uint32_t i = 0; i < c->neuron_count; i++) {
        uint32_t nid = c->neuron_start + i;
        if (nid == winner_id) {
            continue;
        }
        engram_neuron_t *n = neuron_get(eng, nid);
        if (n) {
            n->activation *= (1.0f - inhibition_strength);
        }
    }
}

void cluster_propagate_all(engram_t *eng, uint64_t tick) {
    for (uint32_t i = 0; i < eng->clusters.count; i++) {
        cluster_update_activation(eng, i);
    }

    for (uint32_t i = 0; i < eng->synapses.capacity; i++) {
        engram_synapse_t *s = &eng->synapses.synapses[i];
        if (s->weight > 0.001f) {
            synapse_propagate(eng, i, tick);
        }
    }

    for (uint32_t i = 0; i < eng->clusters.count; i++) {
        cluster_lateral_inhibition(eng, i, 0.3f);
    }
}

uint32_t cluster_get_winner(engram_t *eng, uint32_t id) {
    engram_cluster_t *c = cluster_get(eng, id);
    if (!c) {
        return UINT32_MAX;
    }

    float max_activation = 0.0f;
    uint32_t winner_id = UINT32_MAX;

    for (uint32_t i = 0; i < c->neuron_count; i++) {
        uint32_t nid = c->neuron_start + i;
        engram_neuron_t *n = neuron_get(eng, nid);
        if (n && n->activation > max_activation) {
            max_activation = n->activation;
            winner_id = nid;
        }
    }

    return winner_id;
}
