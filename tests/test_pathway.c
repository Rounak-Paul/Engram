#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

int main(void) {
    printf("Testing pathway formation...\n");
    
    engram_config_t config = engram_config_default();
    config.neuron_count = 1024;
    config.synapse_pool_size = 8192;
    config.hippocampus_tick_ms = 50;
    
    engram_t *eng = engram_create(&config);
    assert(eng != NULL);
    
    engram_cue(eng, "the quick brown fox jumps over the lazy dog", 43);
    engram_cue(eng, "the quick brown fox runs through the forest", 43);
    
    usleep(200000);
    
    assert(engram_pathway_count(eng) > 0);
    
    engram_destroy(eng);
    
    printf("Pathway tests passed.\n");
    return 0;
}
