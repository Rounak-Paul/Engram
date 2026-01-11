#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void teach_fact(engram_t *brain, const char *fact);
static void attempt_recall(engram_t *brain, const char *query);
static void print_stats(engram_t *brain);

int main(void) {
    printf("Engram Basic Example\n");
    printf("====================\n\n");

    engram_config_t config = engram_config_standard();
    config.resource_limits.max_cpu_percent = 10.0f;
    config.auto_arousal = 1;
    config.hippocampus_capacity = 1024;

    printf("Creating brain with %u neurons...\n", config.neuron_count);
    engram_t *brain = engram_create(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }

    printf("Brain created. Background processing started.\n\n");

    printf("=== Phase 1: Teaching facts ===\n");
    teach_fact(brain, "The sky is blue");
    teach_fact(brain, "Water is wet");
    teach_fact(brain, "Fire is hot");
    teach_fact(brain, "The sky has clouds");
    teach_fact(brain, "Rain falls from clouds");
    teach_fact(brain, "Paris is in France");
    teach_fact(brain, "Tokyo is in Japan");
    teach_fact(brain, "Rome is in Italy");

    printf("\n=== Phase 2: Allow initial processing ===\n");
    printf("Processing for 500ms...\n");
    usleep(500000);
    print_stats(brain);

    printf("\n=== Phase 3: Sleep consolidation ===\n");
    printf("Entering sleep state for memory consolidation...\n");
    engram_set_arousal(brain, ENGRAM_AROUSAL_SLEEP);
    sleep(2);
    
    printf("Entering REM for replay...\n");
    engram_set_arousal(brain, ENGRAM_AROUSAL_REM);
    sleep(1);
    
    engram_set_arousal(brain, ENGRAM_AROUSAL_WAKE);
    printf("Awake.\n");
    print_stats(brain);

    printf("\n=== Phase 4: Memory recall tests ===\n");
    
    printf("\nTest 1: Exact match recall\n");
    attempt_recall(brain, "The sky is blue");
    
    printf("\nTest 2: Partial cue recall\n");
    attempt_recall(brain, "sky blue");
    
    printf("\nTest 3: Single word cue\n");
    attempt_recall(brain, "Paris");
    
    printf("\nTest 4: Related concept\n");
    attempt_recall(brain, "clouds rain");
    
    printf("\nTest 5: Non-existent memory\n");
    attempt_recall(brain, "elephants in Antarctica");

    printf("\n=== Phase 5: Reinforcement learning ===\n");
    printf("Teaching the same fact multiple times to strengthen it...\n");
    for (int i = 0; i < 5; i++) {
        teach_fact(brain, "The sky is blue");
        usleep(100000);
    }
    
    engram_set_arousal(brain, ENGRAM_AROUSAL_SLEEP);
    sleep(1);
    engram_set_arousal(brain, ENGRAM_AROUSAL_WAKE);
    
    printf("\nRecall after reinforcement:\n");
    attempt_recall(brain, "sky blue");

    printf("\n=== Final statistics ===\n");
    print_stats(brain);

    printf("\nDestroying brain...\n");
    engram_destroy(brain);
    printf("Done.\n");

    return 0;
}

static void teach_fact(engram_t *brain, const char *fact) {
    engram_cue_t cue = {
        .data = fact,
        .size = strlen(fact),
        .intensity = 1.0f,
        .salience = 0.9f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_FLAG_LEARN
    };

    if (engram_stimulate(brain, &cue) == 0) {
        printf("  Learned: \"%s\"\n", fact);
    } else {
        printf("  Failed: \"%s\"\n", fact);
    }
}

static void attempt_recall(engram_t *brain, const char *query) {
    engram_cue_t cue = {
        .data = query,
        .size = strlen(query),
        .intensity = 0.8f,
        .salience = 1.0f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_FLAG_RECALL
    };

    engram_recall_t result;
    int status = engram_recall(brain, &cue, &result);

    printf("  Query: \"%s\"\n", query);
    
    if (status == 0) {
        if (result.data && result.data_size > 0) {
            printf("  Result: \"%.*s\"\n", (int)result.data_size, (char *)result.data);
        } else {
            printf("  Result: (pattern match, no stored data)\n");
        }
        printf("  Confidence: %.2f\n", result.confidence);
        printf("  Familiarity: %.2f\n", result.familiarity);
        printf("  Age: %u ticks\n", result.age_ticks);
    } else if (status == 1) {
        printf("  Result: No matching memory found\n");
    } else {
        printf("  Result: Error during recall\n");
    }
}

static void print_stats(engram_t *brain) {
    engram_stats_t stats;
    engram_stats(brain, &stats);
    
    const char *arousal_names[] = {"WAKE", "DROWSY", "SLEEP", "REM"};
    
    printf("Statistics:\n");
    printf("  Ticks: %llu @ %.1f/s\n", stats.tick_count, stats.current_tick_rate);
    printf("  Neurons: %u (active: %u)\n", stats.neuron_count, stats.active_neuron_count);
    printf("  Synapses: %u\n", stats.synapse_count);
    printf("  Pathways: %u\n", stats.pathway_count);
    printf("  Memory: %.2f KB\n", stats.memory_usage_bytes / 1024.0f);
    printf("  Arousal: %s\n", arousal_names[stats.arousal_state]);
}
