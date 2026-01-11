#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    
    *len = fread(buf, 1, size, f);
    buf[*len] = '\0';
    fclose(f);
    return buf;
}

static void learn_by_lines(engram_t *eng, const char *text, size_t len) {
    const char *start = text;
    const char *end = text + len;
    
    while (start < end) {
        const char *line_end = start;
        while (line_end < end && *line_end != '\n') line_end++;
        
        size_t line_len = line_end - start;
        if (line_len > 0) {
            engram_cue(eng, start, line_len);
            printf(".");
            fflush(stdout);
        }
        
        start = line_end + 1;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file1> [file2] ... [-o brain.engram]\n", argv[0]);
        return 1;
    }
    
    const char *output_path = "brain.engram";
    engram_t *eng = NULL;
    
    if (access(output_path, F_OK) == 0) {
        eng = engram_load(output_path);
        if (eng) {
            printf("Loaded existing brain from %s\n", output_path);
        }
    }
    
    if (!eng) {
        engram_config_t config = engram_config_default();
        config.neuron_count = 65536;
        config.synapse_pool_size = 1048576;
        eng = engram_create(&config);
    }
    
    if (!eng) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_path = argv[++i];
            continue;
        }
        
        printf("Learning from %s ", argv[i]);
        fflush(stdout);
        
        size_t len;
        char *content = read_file(argv[i], &len);
        if (!content) {
            printf(" (failed to read)\n");
            continue;
        }
        
        learn_by_lines(eng, content, len);
        free(content);
        
        printf(" done\n");
    }
    
    printf("\nSaving brain to %s\n", output_path);
    engram_save(eng, output_path);
    
    printf("\nBrain stats:\n");
    printf("  Neurons: %llu\n", (unsigned long long)engram_neuron_count(eng));
    printf("  Synapses: %llu\n", (unsigned long long)engram_synapse_count(eng));
    printf("  Pathways: %llu\n", (unsigned long long)engram_pathway_count(eng));
    
    engram_destroy(eng);
    return 0;
}
