#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

static void benchmark_insert(engram_t *e, int count) {
    char buf[512];
    double start = get_time_us();
    
    for (int i = 0; i < count; i++) {
        snprintf(buf, sizeof(buf), "Test document %d with some content about topic %d and category %d", 
                 i, i % 100, i % 10);
        engram_cue(e, buf);
    }
    
    double elapsed = get_time_us() - start;
    double per_op = elapsed / count;
    double ops_per_sec = 1000000.0 / per_op;
    
    printf("INSERT %d docs: %.2f ms (%.1f μs/op, %.0f ops/sec)\n", 
           count, elapsed / 1000.0, per_op, ops_per_sec);
}

static void benchmark_query(engram_t *e, int count) {
    const char *queries[] = {
        "black hole event horizon",
        "quantum mechanics wave function",
        "radar systems doppler",
        "synthetic aperture imaging",
        "gravitational singularity",
        "particle physics energy",
        "electromagnetic radiation",
        "speed of light",
        "entropy thermodynamics",
        "nuclear fusion"
    };
    int nq = sizeof(queries) / sizeof(queries[0]);
    
    double start = get_time_us();
    
    for (int i = 0; i < count; i++) {
        engram_response_t r = engram_cue(e, queries[i % nq]);
        (void)r;
    }
    
    double elapsed = get_time_us() - start;
    double per_op = elapsed / count;
    double ops_per_sec = 1000000.0 / per_op;
    
    printf("QUERY  %d times: %.2f ms (%.1f μs/op, %.0f ops/sec)\n", 
           count, elapsed / 1000.0, per_op, ops_per_sec);
}

static void benchmark_cold_query(engram_t *e, int count) {
    const char *novel[] = {
        "cryptocurrency blockchain mining",
        "machine learning neural network",
        "climate change carbon dioxide",
        "autonomous vehicle lidar sensor",
        "gene therapy crispr editing"
    };
    int nq = sizeof(novel) / sizeof(novel[0]);
    
    double start = get_time_us();
    
    for (int i = 0; i < count; i++) {
        engram_response_t r = engram_cue(e, novel[i % nq]);
        (void)r;
    }
    
    double elapsed = get_time_us() - start;
    double per_op = elapsed / count;
    double ops_per_sec = 1000000.0 / per_op;
    
    printf("COLD   %d times: %.2f ms (%.1f μs/op, %.0f ops/sec)\n", 
           count, elapsed / 1000.0, per_op, ops_per_sec);
}

int main(void) {
    printf("\n╔══════════════════════════════════════════════════════════════╗\n");
    printf("║               ENGRAM PERFORMANCE BENCHMARK                    ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    engram_config_t config = engram_config_default();
    engram_t *e = engram_create(&config);
    if (!e) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }
    
    engram_stats_t s = engram_stats(e);
    printf("Device: %s\n", s.device == ENGRAM_DEVICE_VULKAN ? "Vulkan GPU" : "CPU");
    printf("Vector dimensions: 256\n\n");
    
    printf("─── Empty DB ───────────────────────────────────────────────────\n");
    benchmark_insert(e, 1000);
    benchmark_query(e, 1000);
    
    s = engram_stats(e);
    printf("State: %zu neurons, %zu synapses\n\n", s.neuron_count, s.synapse_count);
    
    printf("─── After 1K docs ──────────────────────────────────────────────\n");
    benchmark_insert(e, 1000);
    benchmark_query(e, 1000);
    benchmark_cold_query(e, 1000);
    
    s = engram_stats(e);
    printf("State: %zu neurons, %zu synapses\n\n", s.neuron_count, s.synapse_count);
    
    printf("─── After 2K docs ──────────────────────────────────────────────\n");
    benchmark_insert(e, 5000);
    benchmark_query(e, 1000);
    benchmark_cold_query(e, 1000);
    
    s = engram_stats(e);
    printf("State: %zu neurons, %zu synapses\n\n", s.neuron_count, s.synapse_count);
    
    printf("─── After 7K docs ──────────────────────────────────────────────\n");
    benchmark_insert(e, 10000);
    benchmark_query(e, 1000);
    benchmark_cold_query(e, 1000);
    
    s = engram_stats(e);
    printf("State: %zu neurons, %zu synapses, %.2f MB\n\n", 
           s.neuron_count, s.synapse_count, s.memory_total / (1024.0 * 1024.0));
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("SUMMARY:\n");
    printf("  - Insert includes: tokenize + embed + hash lookup + learn\n");
    printf("  - Query includes: tokenize + embed + activation spread + retrieval\n");
    printf("  - Cold query: novel terms not in training data\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    engram_destroy(e);
    return 0;
}
