#include <engram/engram.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>

#define MAX_INPUT 4096
#define ENGRAM_FILE "engram.dat"
#define PROGRESS_WIDTH 50

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void format_time(double seconds, char *buf, size_t len) {
    if (seconds < 60) {
        snprintf(buf, len, "%.0fs", seconds);
    } else if (seconds < 3600) {
        snprintf(buf, len, "%dm %ds", (int)(seconds / 60), (int)((int)seconds % 60));
    } else {
        snprintf(buf, len, "%dh %dm", (int)(seconds / 3600), (int)(((int)seconds % 3600) / 60));
    }
}

static void print_progress(size_t current, size_t total, double elapsed_ms, 
                           size_t neurons, size_t synapses) {
    float pct = total > 0 ? (float)current / (float)total : 0.0f;
    int filled = (int)(pct * PROGRESS_WIDTH);
    
    double rate_kbs = elapsed_ms > 0 ? (current / 1024.0) / (elapsed_ms / 1000.0) : 0;
    double eta_sec = 0;
    if (rate_kbs > 0 && current > 0) {
        eta_sec = ((total - current) / 1024.0) / rate_kbs;
    }
    
    char eta_str[32];
    format_time(eta_sec, eta_str, sizeof(eta_str));
    
    printf("\r  [");
    for (int i = 0; i < PROGRESS_WIDTH; i++) {
        if (i < filled) printf("█");
        else if (i == filled) printf("▓");
        else printf("░");
    }
    printf("] %5.1f%% │ %6.1f KB/s │ ETA: %-8s │ N:%zu S:%zu", 
           pct * 100, rate_kbs, pct >= 1.0f ? "--" : eta_str, neurons, synapses);
    fflush(stdout);
}

static void print_stats(engram_t *e) {
    printf("\n┌───────────────────────────────────────┐\n");
    printf("│  Neurons: %-10zu  Synapses: %-10zu│\n",
           engram_neuron_count(e), engram_synapse_count(e));
    printf("└───────────────────────────────────────┘\n");
}

static long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return st.st_size;
    return 0;
}

static int load_knowledge_file(engram_t *e, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Cannot open: %s\n", path);
        return -1;
    }
    
    long file_size = get_file_size(path);
    const char *filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    
    printf("\n  ┌─ %s (%ld bytes)\n", filename, file_size);
    
    double start_time = get_time_ms();
    
    char line[1024];
    int count = 0;
    char paragraph[MAX_INPUT] = {0};
    size_t para_len = 0;
    size_t bytes_read = 0;
    
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        bytes_read += len;
        
        if (len <= 1 && para_len > 0) {
            engram_cue(e, paragraph);
            count++;
            paragraph[0] = '\0';
            para_len = 0;
            print_progress(bytes_read, file_size, get_time_ms() - start_time,
                          engram_neuron_count(e), engram_synapse_count(e));
            continue;
        }
        
        if (para_len + len < MAX_INPUT - 1) {
            strcat(paragraph, line);
            para_len += len;
        } else if (para_len > 0) {
            engram_cue(e, paragraph);
            count++;
            strcpy(paragraph, line);
            para_len = len;
            print_progress(bytes_read, file_size, get_time_ms() - start_time,
                          engram_neuron_count(e), engram_synapse_count(e));
        }
    }
    
    if (para_len > 0) {
        engram_cue(e, paragraph);
        count++;
    }
    
    double elapsed = (get_time_ms() - start_time) / 1000.0;
    char time_str[32];
    format_time(elapsed, time_str, sizeof(time_str));
    
    print_progress(file_size, file_size, get_time_ms() - start_time,
                  engram_neuron_count(e), engram_synapse_count(e));
    printf("\n  └─ ✓ %d segments in %s\n", count, time_str);
    
    fclose(f);
    return count;
}

static int load_knowledge_dir(engram_t *e, const char *dirpath) {
    DIR *dir = opendir(dirpath);
    if (!dir) {
        fprintf(stderr, "Cannot open directory: %s\n", dirpath);
        return -1;
    }
    
    struct dirent *entry;
    char *files[256];
    int file_count = 0;
    char filepath[512];
    
    while ((entry = readdir(dir)) != NULL && file_count < 256) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".txt") == 0) {
            snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);
            files[file_count] = strdup(filepath);
            file_count++;
        }
    }
    closedir(dir);
    
    if (file_count == 0) {
        printf("No .txt files found in %s\n", dirpath);
        return 0;
    }
    
    printf("\n╔════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  LEARNING: %-60s (%d files)  ║\n", dirpath, file_count);
    printf("╚════════════════════════════════════════════════════════════════════════════════════════╝");
    
    double start_time = get_time_ms();
    int total_segments = 0;
    
    for (int i = 0; i < file_count; i++) {
        int loaded = load_knowledge_file(e, files[i]);
        if (loaded > 0) total_segments += loaded;
        free(files[i]);
    }
    
    double elapsed = (get_time_ms() - start_time) / 1000.0;
    char time_str[32];
    format_time(elapsed, time_str, sizeof(time_str));
    
    printf("\n╔════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║  COMPLETE: %d segments from %d files in %-42s  ║\n", total_segments, file_count, time_str);
    printf("╚════════════════════════════════════════════════════════════════════════════════════════╝\n");
    
    return total_segments;
}

static void interactive_mode(engram_t *e) {
    char input[MAX_INPUT];
    
    printf("\nEngram Interactive Mode\n");
    printf("Commands: :learn <dir>, :load <file>, :save, :stats, :quit\n");
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
        
        if (strcmp(input, ":save") == 0) {
            if (engram_save(e, ENGRAM_FILE) == ENGRAM_OK) {
                printf("Saved to %s\n", ENGRAM_FILE);
            } else {
                printf("Failed to save\n");
            }
            continue;
        }
        
        if (strncmp(input, ":learn ", 7) == 0) {
            load_knowledge_dir(e, input + 7);
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
            printf("\nThought pattern (%zu activations):\n", resp.count);
            for (size_t i = 0; i < resp.count && i < 10; i++) {
                const char *text = resp.content[i];
                if (text && strlen(text) > 60) {
                    printf("  [%.3f|%.3f] %.57s...\n", 
                           resp.relevance[i], resp.activations[i], text);
                } else if (text) {
                    printf("  [%.3f|%.3f] %s\n", 
                           resp.relevance[i], resp.activations[i], text);
                } else {
                    printf("  [%.3f|%.3f] (neuron %lu)\n", 
                           resp.relevance[i], resp.activations[i], (unsigned long)resp.ids[i]);
                }
            }
            if (resp.count > 10) {
                printf("  ... and %zu more\n", resp.count - 10);
            }
        } else {
            printf("\nNew thought encoded.\n");
        }
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
    
    FILE *f = fopen(ENGRAM_FILE, "rb");
    if (f) {
        fclose(f);
        if (engram_load(e, ENGRAM_FILE) == ENGRAM_OK) {
            printf("Loaded existing memory from %s\n", ENGRAM_FILE);
        }
    }
    
    print_stats(e);
    
    for (int i = 1; i < argc; i++) {
        load_knowledge_dir(e, argv[i]);
    }
    
    if (argc > 1) {
        print_stats(e);
        engram_save(e, ENGRAM_FILE);
    }
    
    interactive_mode(e);
    
    printf("\nSaving and shutting down...\n");
    engram_save(e, ENGRAM_FILE);
    engram_destroy(e);
    return 0;
}
