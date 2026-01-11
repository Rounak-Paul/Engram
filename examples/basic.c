#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    engram_config_t config = engram_config_default();
    config.neuron_count = 16384;
    config.synapse_pool_size = 262144;
    
    engram_t *eng = engram_create(&config);
    if (!eng) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }
    
    printf("Teaching the brain some facts...\n\n");
    
    const char *facts[] = {
        "The sky is blue during the day",
        "The sun rises in the east",
        "Water freezes at zero degrees",
        "Fire is hot and produces light",
        "Trees produce oxygen through photosynthesis",
        "The moon orbits the Earth",
        "Stars are distant suns",
        "Rain falls from clouds"
    };
    
    for (size_t i = 0; i < sizeof(facts) / sizeof(facts[0]); i++) {
        printf("Learning: %s\n", facts[i]);
        engram_cue(eng, facts[i], strlen(facts[i]));
        usleep(200000);
    }
    
    printf("\nWaiting for consolidation...\n");
    sleep(3);
    
    printf("\nAsking the brain questions...\n\n");
    
    const char *questions[] = {
        "sky",
        "sun",
        "water",
        "fire",
        "moon",
        "stars"
    };
    
    for (size_t i = 0; i < sizeof(questions) / sizeof(questions[0]); i++) {
        engram_response_t resp = engram_cue(eng, questions[i], strlen(questions[i]));
        printf("Cue: '%s' -> %s (confidence: %.2f)\n", 
               questions[i], resp.text, resp.confidence);
        engram_response_free(&resp);
    }
    
    printf("\nBrain stats:\n");
    printf("  Neurons: %llu\n", (unsigned long long)engram_neuron_count(eng));
    printf("  Synapses: %llu\n", (unsigned long long)engram_synapse_count(eng));
    printf("  Pathways: %llu\n", (unsigned long long)engram_pathway_count(eng));
    
    engram_destroy(eng);
    return 0;
}
