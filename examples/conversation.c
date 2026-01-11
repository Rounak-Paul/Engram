#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#define sleep_ms(ms) Sleep(ms)
static int get_cpu_count(void) {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}
static size_t get_available_memory_mb(void) {
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return (size_t)(status.ullAvailPhys / (1024 * 1024));
}
#else
#include <unistd.h>
#define sleep_ms(ms) usleep((ms) * 1000)
static int get_cpu_count(void) {
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    return count > 0 ? (int)count : 1;
}
static size_t get_available_memory_mb(void) {
#ifdef __APPLE__
    return 8192;
#else
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page_size > 0) {
        return (size_t)((pages * page_size) / (1024 * 1024));
    }
    return 4096;
#endif
}
#endif

static engram_t *brain = NULL;

static void teach(const char *text) {
    engram_cue_t cue = {
        .data = text,
        .size = strlen(text) + 1,
        .intensity = 1.0f,
        .salience = 1.0f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_FLAG_LEARN
    };
    engram_stimulate(brain, &cue);
}

static const char *recall_memory(const char *query) {
    static char buffer[1024];
    
    engram_cue_t cue = {
        .data = query,
        .size = strlen(query) + 1,
        .intensity = 1.0f,
        .salience = 0.5f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_FLAG_RECALL
    };
    
    engram_recall_t result;
    if (engram_recall(brain, &cue, &result) == 0 && result.confidence > 0.01f) {
        if (result.data && result.data_size > 0) {
            size_t len = result.data_size < sizeof(buffer) - 1 ? result.data_size : sizeof(buffer) - 1;
            memcpy(buffer, result.data, len);
            buffer[len] = '\0';
            return buffer;
        }
    }
    return NULL;
}

static void print_stats(void) {
    engram_stats_t stats;
    engram_stats(brain, &stats);
    
    printf("\n─── Brain Statistics ───\n");
    printf("  Neurons: %u (active: %u)\n", stats.neuron_count, stats.active_neuron_count);
    printf("  Synapses: %u\n", stats.synapse_count);
    printf("  Pathways: %u\n", stats.pathway_count);
    printf("  Memory: %.2f MB\n", stats.memory_usage_bytes / (1024.0 * 1024.0));
    printf("  Tick rate: %.1f/s\n", stats.current_tick_rate);
    printf("────────────────────────\n\n");
}

static int is_question(const char *input, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '?') return 1;
    }
    
    if (len >= 5 && strncasecmp(input, "what ", 5) == 0) return 1;
    if (len >= 4 && strncasecmp(input, "who ", 4) == 0) return 1;
    if (len >= 6 && strncasecmp(input, "where ", 6) == 0) return 1;
    if (len >= 5 && strncasecmp(input, "when ", 5) == 0) return 1;
    if (len >= 4 && strncasecmp(input, "why ", 4) == 0) return 1;
    if (len >= 4 && strncasecmp(input, "how ", 4) == 0) return 1;
    if (len >= 7 && strncasecmp(input, "tell me", 7) == 0) return 1;
    if (len >= 6 && strncasecmp(input, "recall", 6) == 0) return 1;
    if (len >= 8 && strncasecmp(input, "remember", 8) == 0) return 1;
    
    int has_space = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == ' ') has_space = 1;
    }
    if (!has_space && len > 0 && len < 30) return 1;
    
    return 0;
}

int main(void) {
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║  Engram - Brain-Inspired Conversational Memory        ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    srand((unsigned int)time(NULL));
    
    int cpu_count = get_cpu_count();
    size_t mem_mb = get_available_memory_mb();
    
    printf("System: %d CPU cores, %zu MB available\n", cpu_count, mem_mb);
    
    size_t usable_mem = mem_mb / 2;
    if (usable_mem > 4096) usable_mem = 4096;
    if (usable_mem < 512) usable_mem = 512;
    
    uint32_t neuron_count = (uint32_t)(usable_mem * 500);
    if (neuron_count > 2000000) neuron_count = 2000000;
    if (neuron_count < 100000) neuron_count = 100000;
    
    printf("Allocating %u neurons, using 95%% CPU...\n\n", neuron_count);
    
    engram_config_t config = engram_config_performance();
    config.neuron_count = neuron_count;
    config.resource_limits.max_cpu_percent = 95.0f;
    config.resource_limits.max_memory_bytes = usable_mem * 1024 * 1024;
    config.hippocampus_capacity = 8192;
    config.auto_arousal = 0;
    
    brain = engram_create(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create brain\n");
        return 1;
    }
    
    printf("Brain ready.\n\n");
    printf("This brain learns naturally through exposure.\n");
    printf("Type statements to teach, questions to recall.\n\n");
    printf("Commands: /stats, /sleep, /quit\n\n");
    printf("────────────────────────────────────────────────────────\n\n");
    
    char input[1024];
    
    while (1) {
        printf("You: ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) break;
        
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r')) {
            input[--len] = '\0';
        }
        
        if (len == 0) continue;
        
        if (strcmp(input, "/quit") == 0 || strcmp(input, "/exit") == 0) {
            printf("\nGoodbye!\n");
            break;
        }
        
        if (strcmp(input, "/stats") == 0) {
            print_stats();
            continue;
        }
        
        if (strcmp(input, "/sleep") == 0) {
            printf("Consolidating...\n");
            engram_set_arousal(brain, ENGRAM_AROUSAL_SLEEP);
            sleep_ms(500);
            engram_set_arousal(brain, ENGRAM_AROUSAL_REM);
            sleep_ms(500);
            engram_set_arousal(brain, ENGRAM_AROUSAL_WAKE);
            printf("Done.\n\n");
            continue;
        }
        
        if (is_question(input, len)) {
            const char *memory = recall_memory(input);
            if (memory) {
                printf("Brain: %s\n\n", memory);
            } else {
                printf("Brain: I don't recall anything about that.\n\n");
            }
        } else {
            teach(input);
            printf("Brain: Learned.\n\n");
        }
    }
    
    engram_destroy(brain);
    return 0;
}
