#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static volatile int running = 1;

static void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

static void print_stats(engram_t *eng) {
    printf("\n[neurons: %llu | synapses: %llu | pathways: %llu]\n",
           (unsigned long long)engram_neuron_count(eng),
           (unsigned long long)engram_synapse_count(eng),
           (unsigned long long)engram_pathway_count(eng));
}

int main(int argc, char **argv) {
    signal(SIGINT, signal_handler);
    
    const char *save_path = "brain.engram";
    engram_t *eng = NULL;
    
    if (argc > 1) {
        save_path = argv[1];
        eng = engram_load(save_path);
        if (eng) {
            printf("Loaded brain from %s\n", save_path);
        }
    }
    
    if (!eng) {
        engram_config_t config = engram_config_default();
        config.neuron_count = 32768;
        config.synapse_pool_size = 524288;
        config.hippocampus_tick_ms = 100;
        config.consolidation_tick_ms = 2000;
        eng = engram_create(&config);
        printf("Created new brain\n");
    }
    
    if (!eng) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }
    
    print_stats(eng);
    printf("\nConversation started. Type 'quit' to exit, 'stats' for brain stats.\n");
    printf("The brain learns from everything you say.\n\n");
    
    char input[1024];
    
    while (running) {
        printf("You: ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) break;
        
        size_t len = strlen(input);
        while (len > 0 && (input[len-1] == '\n' || input[len-1] == '\r')) {
            input[--len] = '\0';
        }
        
        if (len == 0) continue;
        
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            break;
        }
        
        if (strcmp(input, "stats") == 0) {
            print_stats(eng);
            continue;
        }
        
        if (strcmp(input, "save") == 0) {
            if (engram_save(eng, save_path) == 0) {
                printf("Brain saved to %s\n", save_path);
            } else {
                printf("Failed to save brain\n");
            }
            continue;
        }
        
        engram_response_t resp = engram_cue(eng, input, len);
        
        printf("Brain: %s\n", resp.text ? resp.text : "(no response)");
        printf("       [confidence: %.2f | novelty: %.2f]\n", 
               resp.confidence, resp.novelty);
        
        engram_response_free(&resp);
    }
    
    printf("\nSaving brain state...\n");
    engram_save(eng, save_path);
    print_stats(eng);
    
    engram_destroy(eng);
    printf("Goodbye.\n");
    
    return 0;
}
