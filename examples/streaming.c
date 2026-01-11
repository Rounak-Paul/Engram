#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

int main(void) {
    printf("Engram Streaming Example\n");
    printf("========================\n\n");

    engram_config_t config = engram_config_standard();
    config.auto_arousal = 0;

    engram_t *brain = engram_create(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }

    printf("Simulating continuous data stream...\n\n");

    const char *events[] = {
        "user logged in",
        "page view home",
        "page view products",
        "add to cart item_123",
        "page view checkout",
        "user logged in",
        "page view home",
        "page view about",
        "user logged in",
        "page view home",
        "page view products",
        "add to cart item_456",
        "page view products",
        "add to cart item_789",
        "page view checkout",
        "purchase completed"
    };
    size_t event_count = sizeof(events) / sizeof(events[0]);

    for (size_t i = 0; i < event_count; i++) {
        engram_cue_t cue = {
            .data = events[i],
            .size = strlen(events[i]),
            .intensity = 1.0f,
            .salience = 0.7f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = 0
        };

        engram_stimulate(brain, &cue);
        printf("  [%zu] %s\n", i + 1, events[i]);

        usleep(50000);
    }

    printf("\nStream complete. Letting brain settle...\n");
    usleep(200000);

    printf("\nTriggering sleep cycle for pattern consolidation...\n");
    engram_set_arousal(brain, ENGRAM_AROUSAL_SLEEP);
    sleep(1);

    printf("Entering REM for creative associations...\n");
    engram_set_arousal(brain, ENGRAM_AROUSAL_REM);
    usleep(500000);

    printf("Waking up...\n");
    engram_set_arousal(brain, ENGRAM_AROUSAL_WAKE);
    usleep(100000);

    printf("\nQuerying learned patterns:\n");

    const char *queries[] = {"user logged", "products", "checkout"};
    for (int i = 0; i < 3; i++) {
        engram_cue_t query = {
            .data = queries[i],
            .size = strlen(queries[i]),
            .intensity = 0.9f,
            .salience = 1.0f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = ENGRAM_CUE_FLAG_RECALL
        };

        engram_recall_t result;
        int status = engram_recall(brain, &query, &result);

        printf("  '%s': ", queries[i]);
        if (status == 0) {
            printf("confidence=%.2f, familiarity=%.2f\n",
                   result.confidence, result.familiarity);
        } else {
            printf("no pattern found\n");
        }
    }

    engram_stats_t stats;
    engram_stats(brain, &stats);
    printf("\nSession statistics:\n");
    printf("  Total ticks processed: %llu\n", stats.tick_count);
    printf("  Pathways formed: %u\n", stats.pathway_count);
    printf("  Synapses: %u\n", stats.synapse_count);
    printf("  Current tick rate: %.1f/s\n", stats.current_tick_rate);

    engram_destroy(brain);
    printf("\nDone.\n");

    return 0;
}
