#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    printf("Engram Associative Memory Example\n");
    printf("==================================\n\n");

    engram_config_t config = engram_config_standard();
    engram_t *brain = engram_create(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }

    printf("Creating associations between concepts...\n\n");

    engram_cue_t apple_cues[3] = {
        {.data = "apple", .size = 5, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0},
        {.data = "red", .size = 3, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0},
        {.data = "fruit", .size = 5, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0}
    };
    engram_associate(brain, apple_cues, 3);
    printf("Associated: apple <-> red <-> fruit\n");

    engram_cue_t banana_cues[3] = {
        {.data = "banana", .size = 6, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0},
        {.data = "yellow", .size = 6, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0},
        {.data = "fruit", .size = 5, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0}
    };
    engram_associate(brain, banana_cues, 3);
    printf("Associated: banana <-> yellow <-> fruit\n");

    engram_cue_t sky_cues[3] = {
        {.data = "sky", .size = 3, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0},
        {.data = "blue", .size = 4, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0},
        {.data = "clouds", .size = 6, .intensity = 1.0f, .salience = 0.9f, .modality = ENGRAM_MODALITY_TEXT, .flags = 0}
    };
    engram_associate(brain, sky_cues, 3);
    printf("Associated: sky <-> blue <-> clouds\n");

    printf("\nLet the brain process for a moment...\n");
    usleep(500000);

    printf("\nTesting recall:\n");

    const char *queries[] = {"red", "fruit", "blue"};
    for (int i = 0; i < 3; i++) {
        engram_cue_t query = {
            .data = queries[i],
            .size = strlen(queries[i]),
            .intensity = 0.8f,
            .salience = 1.0f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = ENGRAM_CUE_FLAG_RECALL
        };

        engram_recall_t result;
        int status = engram_recall(brain, &query, &result);

        printf("  Query '%s': ", queries[i]);
        if (status == 0) {
            printf("found pathway (confidence: %.2f)\n", result.confidence);
            engram_recall_free(&result);
        } else {
            printf("no match found\n");
        }
    }

    engram_stats_t stats;
    engram_stats(brain, &stats);
    printf("\nFinal statistics:\n");
    printf("  Pathways formed: %u\n", stats.pathway_count);
    printf("  Synapses created: %u\n", stats.synapse_count);

    engram_destroy(brain);
    printf("\nDone.\n");

    return 0;
}
