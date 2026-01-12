#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT 4096

static void print_stats(engram_t *e) {
    printf("\n[Stats] Neurons: %zu | Synapses: %zu | GPU: %s\n",
           engram_neuron_count(e), engram_synapse_count(e),
           engram_gpu_available(e) ? "enabled" : "disabled");
}

static int load_knowledge_file(engram_t *e, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path);
        return -1;
    }
    
    printf("Loading: %s\n", path);
    
    char line[1024];
    int count = 0;
    char paragraph[MAX_INPUT] = {0};
    size_t para_len = 0;
    
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        
        if (len <= 1 && para_len > 0) {
            engram_response_t resp = engram_cue(e, paragraph);
            engram_response_free(&resp);
            count++;
            paragraph[0] = '\0';
            para_len = 0;
            continue;
        }
        
        if (para_len + len < MAX_INPUT - 1) {
            strcat(paragraph, line);
            para_len += len;
        } else if (para_len > 0) {
            engram_response_t resp = engram_cue(e, paragraph);
            engram_response_free(&resp);
            count++;
            strcpy(paragraph, line);
            para_len = len;
        }
    }
    
    if (para_len > 0) {
        engram_response_t resp = engram_cue(e, paragraph);
        engram_response_free(&resp);
        count++;
    }
    
    fclose(f);
    printf("Loaded %d segments from %s\n", count, path);
    return count;
}

static void interactive_mode(engram_t *e) {
    char input[MAX_INPUT];
    
    printf("\nEngram Interactive Mode\n");
    printf("Commands: :load <file>, :stats, :quit\n");
    printf("Type anything else to cue the engram\n\n");
    
    while (1) {
        printf("> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) break;
        
        size_t len = strlen(input);
        if (len > 0 && input[len-1] == '\n') input[len-1] = '\0';
        
        if (input[0] == '\0') continue;
        
        if (strcmp(input, ":quit") == 0 || strcmp(input, ":q") == 0) {
            break;
        }
        
        if (strcmp(input, ":stats") == 0) {
            print_stats(e);
            continue;
        }
        
        if (strncmp(input, ":load ", 6) == 0) {
            load_knowledge_file(e, input + 6);
            print_stats(e);
            continue;
        }
        
        engram_response_t resp = engram_cue(e, input);
        
        if (resp.count > 0) {
            printf("\nActivated %zu memories:\n", resp.count);
            for (size_t i = 0; i < resp.count && i < 10; i++) {
                printf("  [%zu] id=%lu relevance=%.3f\n", 
                       i + 1, (unsigned long)resp.ids[i], resp.relevance[i]);
            }
        } else {
            printf("\nNew memory formed.\n");
        }
        
        engram_response_free(&resp);
    }
}

int main(int argc, char *argv[]) {
    engram_config_t config = engram_config_default();
    engram_t *e = engram_create(&config);
    
    if (!e) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }
    
    printf("Engram initialized\n");
    print_stats(e);
    
    for (int i = 1; i < argc; i++) {
        load_knowledge_file(e, argv[i]);
    }
    
    if (argc > 1) {
        print_stats(e);
    }
    
    interactive_mode(e);
    
    printf("\nShutting down...\n");
    engram_destroy(e);
    return 0;
}
