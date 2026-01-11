#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE_LEN 4096
#define MAX_PATH_LEN 512

static char *read_file_content(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(size + 1);
    if (!content) {
        fclose(f);
        return NULL;
    }
    
    size_t read_count = fread(content, 1, size, f);
    content[read_count] = '\0';
    fclose(f);
    
    if (out_len) *out_len = read_count;
    return content;
}

static void teach(engram_t *brain, const char *text) {
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

static void ingest_text(engram_t *brain, const char *text, const char *source) {
    char sentence[MAX_LINE_LEN];
    int sentence_count = 0;
    const char *p = text;
    char *s = sentence;
    
    while (*p) {
        if (*p == '.' || *p == '!' || *p == '?') {
            *s++ = *p++;
            *s = '\0';
            
            if (strlen(sentence) > 10) {
                teach(brain, sentence);
                sentence_count++;
            }
            s = sentence;
            
            while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
                p++;
        } else if (*p == '\n' || *p == '\r') {
            *s++ = ' ';
            p++;
        } else if ((s - sentence) < MAX_LINE_LEN - 2) {
            *s++ = *p++;
        } else {
            p++;
        }
    }
    
    printf("  Ingested %d sentences from %s\n", sentence_count, source);
}

static void ingest_directory(engram_t *brain, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory %s\n", dir_path);
        return;
    }
    
    struct dirent *entry;
    int file_count = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".txt") == 0) {
            size_t content_len;
            char *content = read_file_content(path, &content_len);
            if (content) {
                ingest_text(brain, content, entry->d_name);
                free(content);
                file_count++;
            }
        }
    }
    
    closedir(dir);
    printf("\nProcessed %d text files from %s\n", file_count, dir_path);
}

static const char *recall_memory(engram_t *brain, const char *query) {
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

int main(int argc, char *argv[]) {
    const char *knowledge_dir = "../knowledge";
    const char *output_file = "physics.engram";
    
    if (argc >= 2) knowledge_dir = argv[1];
    if (argc >= 3) output_file = argv[2];
    
    printf("Engram Knowledge Ingestion Tool\n");
    printf("================================\n\n");
    
    engram_config_t config = engram_config_default();
    config.neuron_count = 100000;
    config.hippocampus_capacity = 50000;
    
    printf("Initializing brain with %u neuron capacity...\n", config.neuron_count);
    engram_t *brain = engram_create(&config);
    if (!brain) {
        fprintf(stderr, "Failed to create engram\n");
        return 1;
    }
    
    printf("Ingesting knowledge from: %s\n\n", knowledge_dir);
    ingest_directory(brain, knowledge_dir);
    
    printf("\nSaving brain to: %s\n", output_file);
    if (engram_save(brain, output_file) == 0) {
        printf("Successfully saved brain state!\n");
        
        struct stat st;
        if (stat(output_file, &st) == 0) {
            if (st.st_size < 1024)
                printf("File size: %lld bytes\n", (long long)st.st_size);
            else if (st.st_size < 1024 * 1024)
                printf("File size: %.1f KB\n", st.st_size / 1024.0);
            else
                printf("File size: %.1f MB\n", st.st_size / (1024.0 * 1024.0));
        }
    } else {
        fprintf(stderr, "Failed to save brain state\n");
    }
    
    printf("\nTesting recall:\n");
    printf("--------------\n");
    
    const char *queries[] = {
        "radar",
        "black hole",
        "quantum",
        "universe",
        "SAR",
        "doppler",
        "einstein",
        "hawking"
    };
    
    int num_queries = sizeof(queries) / sizeof(queries[0]);
    
    for (int i = 0; i < num_queries; i++) {
        printf("\nQuery: \"%s\"\n", queries[i]);
        const char *result = recall_memory(brain, queries[i]);
        if (result && strlen(result) > 0) {
            printf("  -> %s\n", result);
        } else {
            printf("  -> (no associations)\n");
        }
    }
    
    engram_destroy(brain);
    printf("\nDone!\n");
    
    return 0;
}
