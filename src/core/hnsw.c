#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define HNSW_M 16
#define HNSW_M_MAX 32
#define HNSW_EF_CONSTRUCTION 100
#define HNSW_ML 1.0f / logf((float)HNSW_M)
#define HNSW_MAX_CANDIDATES 512

static int random_level(void) {
    float r = (float)rand() / (float)RAND_MAX;
    int level = (int)(-logf(r) * HNSW_ML);
    return level < HNSW_MAX_LEVEL ? level : HNSW_MAX_LEVEL - 1;
}

static float query_distance(substrate_t *s, const float *query, size_t idx) {
    return 1.0f - vec_dot(query, s->neurons[idx].embedding, s->vector_dim);
}

typedef struct {
    size_t idx;
    float dist;
} candidate_t;

static int candidate_cmp_min(const void *a, const void *b) {
    float da = ((candidate_t*)a)->dist;
    float db = ((candidate_t*)b)->dist;
    return da < db ? -1 : (da > db ? 1 : 0);
}

hnsw_t hnsw_create(size_t capacity) {
    hnsw_t h = {0};
    h.nodes = calloc(capacity, sizeof(hnsw_node_t));
    h.capacity = capacity;
    h.count = 0;
    h.entry_point = SIZE_MAX;
    h.max_level = 0;
    return h;
}

void hnsw_destroy(hnsw_t *h) {
    for (size_t i = 0; i < h->count; i++) {
        hnsw_node_t *node = &h->nodes[i];
        for (int l = 0; l <= node->level; l++) {
            free(node->neighbors[l]);
        }
    }
    free(h->nodes);
    memset(h, 0, sizeof(*h));
}

static size_t search_layer_greedy(hnsw_t *h, substrate_t *s, const float *query,
                                   size_t entry, int level) {
    size_t curr = entry;
    float curr_dist = query_distance(s, query, h->nodes[curr].neuron_idx);
    
    bool changed = true;
    while (changed) {
        changed = false;
        hnsw_node_t *node = &h->nodes[curr];
        if (level > node->level) break;
        
        for (int i = 0; i < node->neighbor_count[level]; i++) {
            size_t neighbor = node->neighbors[level][i];
            float dist = query_distance(s, query, h->nodes[neighbor].neuron_idx);
            if (dist < curr_dist) {
                curr = neighbor;
                curr_dist = dist;
                changed = true;
            }
        }
    }
    return curr;
}

static void search_layer_ef(hnsw_t *h, substrate_t *s, const float *query,
                            size_t entry, int level, size_t ef,
                            candidate_t *results, size_t *result_count,
                            uint8_t *visited, size_t visited_size) {
    memset(visited, 0, visited_size);
    
    candidate_t candidates[HNSW_MAX_CANDIDATES];
    size_t cand_count = 0;
    size_t cand_pos = 0;
    
    float entry_dist = query_distance(s, query, h->nodes[entry].neuron_idx);
    candidates[cand_count++] = (candidate_t){entry, entry_dist};
    visited[entry / 8] |= (1 << (entry % 8));
    
    float worst_dist = entry_dist;
    size_t result_idx = 0;
    results[result_idx++] = (candidate_t){entry, entry_dist};
    
    while (cand_pos < cand_count) {
        size_t best_idx = cand_pos;
        float best_dist = candidates[cand_pos].dist;
        for (size_t i = cand_pos + 1; i < cand_count; i++) {
            if (candidates[i].dist < best_dist) {
                best_idx = i;
                best_dist = candidates[i].dist;
            }
        }
        
        candidate_t closest = candidates[best_idx];
        candidates[best_idx] = candidates[cand_pos];
        cand_pos++;
        
        if (result_idx >= ef && closest.dist > worst_dist) break;
        
        hnsw_node_t *node = &h->nodes[closest.idx];
        if (level > node->level) continue;
        
        for (int i = 0; i < node->neighbor_count[level]; i++) {
            size_t neighbor = node->neighbors[level][i];
            if (visited[neighbor / 8] & (1 << (neighbor % 8))) continue;
            visited[neighbor / 8] |= (1 << (neighbor % 8));
            
            float dist = query_distance(s, query, h->nodes[neighbor].neuron_idx);
            
            if (dist < worst_dist || result_idx < ef) {
                if (cand_count < HNSW_MAX_CANDIDATES) {
                    candidates[cand_count++] = (candidate_t){neighbor, dist};
                }
                
                if (result_idx < ef) {
                    results[result_idx++] = (candidate_t){neighbor, dist};
                    if (dist > worst_dist) worst_dist = dist;
                } else {
                    size_t worst_idx = 0;
                    for (size_t j = 1; j < result_idx; j++) {
                        if (results[j].dist > results[worst_idx].dist) worst_idx = j;
                    }
                    if (dist < results[worst_idx].dist) {
                        results[worst_idx] = (candidate_t){neighbor, dist};
                        worst_dist = 0;
                        for (size_t j = 0; j < result_idx; j++) {
                            if (results[j].dist > worst_dist) worst_dist = results[j].dist;
                        }
                    }
                }
            }
        }
    }
    
    *result_count = result_idx;
    qsort(results, result_idx, sizeof(candidate_t), candidate_cmp_min);
}

static void select_neighbors(size_t node_idx, candidate_t *candidates, size_t cand_count,
                             size_t m, uint32_t *out, uint8_t *out_count) {
    size_t count = 0;
    for (size_t i = 0; i < cand_count && count < m; i++) {
        if (candidates[i].idx != node_idx) {
            out[count++] = (uint32_t)candidates[i].idx;
        }
    }
    *out_count = (uint8_t)count;
}

void hnsw_insert(hnsw_t *h, substrate_t *s, size_t neuron_idx) {
    if (h->count >= h->capacity) {
        size_t new_cap = h->capacity * 2;
        hnsw_node_t *new_nodes = realloc(h->nodes, new_cap * sizeof(hnsw_node_t));
        if (!new_nodes) return;
        memset(new_nodes + h->capacity, 0, (new_cap - h->capacity) * sizeof(hnsw_node_t));
        h->nodes = new_nodes;
        h->capacity = new_cap;
    }
    
    size_t idx = h->count++;
    hnsw_node_t *node = &h->nodes[idx];
    node->neuron_idx = neuron_idx;
    node->level = random_level();
    
    for (int l = 0; l <= node->level; l++) {
        node->neighbors[l] = calloc(HNSW_M_MAX, sizeof(uint32_t));
        node->neighbor_count[l] = 0;
    }
    
    if (h->entry_point == SIZE_MAX) {
        h->entry_point = idx;
        h->max_level = node->level;
        return;
    }
    
    float *query = s->neurons[neuron_idx].embedding;
    
    size_t curr = h->entry_point;
    
    for (int level = h->max_level; level > node->level; level--) {
        curr = search_layer_greedy(h, s, query, curr, level);
    }
    
    size_t visited_size = (h->count + 7) / 8;
    uint8_t *visited = malloc(visited_size);
    
    for (int level = node->level < (int)h->max_level ? node->level : (int)h->max_level; level >= 0; level--) {
        candidate_t results[HNSW_MAX_CANDIDATES];
        size_t result_count;
        search_layer_ef(h, s, query, curr, level, HNSW_EF_CONSTRUCTION, results, &result_count, visited, visited_size);
        
        size_t m = level == 0 ? HNSW_M * 2 : HNSW_M;
        select_neighbors(idx, results, result_count, m, node->neighbors[level], &node->neighbor_count[level]);
        
        for (int i = 0; i < node->neighbor_count[level]; i++) {
            size_t neighbor_idx = node->neighbors[level][i];
            hnsw_node_t *neighbor = &h->nodes[neighbor_idx];
            
            if (level <= neighbor->level && neighbor->neighbor_count[level] < HNSW_M_MAX) {
                neighbor->neighbors[level][neighbor->neighbor_count[level]++] = (uint32_t)idx;
            }
        }
        
        if (result_count > 0) curr = results[0].idx;
    }
    
    free(visited);
    
    if (node->level > h->max_level) {
        h->max_level = node->level;
        h->entry_point = idx;
    }
}

size_t hnsw_search(hnsw_t *h, substrate_t *s, const float *query,
                   size_t *results, float *distances, size_t k, size_t ef) {
    if (h->count == 0 || h->entry_point == SIZE_MAX) return 0;
    
    size_t curr = h->entry_point;
    
    for (int level = h->max_level; level > 0; level--) {
        curr = search_layer_greedy(h, s, query, curr, level);
    }
    
    size_t visited_size = (h->count + 7) / 8;
    uint8_t *visited = malloc(visited_size);
    
    candidate_t cands[HNSW_MAX_CANDIDATES];
    size_t cand_count;
    search_layer_ef(h, s, query, curr, 0, ef, cands, &cand_count, visited, visited_size);
    
    free(visited);
    
    size_t count = cand_count < k ? cand_count : k;
    for (size_t i = 0; i < count; i++) {
        results[i] = h->nodes[cands[i].idx].neuron_idx;
        distances[i] = 1.0f - cands[i].dist;
    }
    
    return count;
}
