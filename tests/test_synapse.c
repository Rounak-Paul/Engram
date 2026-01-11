#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("Testing synapse creation and modification...\n");
    
    engram_config_t config = engram_config_default();
    config.neuron_count = 1024;
    config.synapse_pool_size = 8192;
    
    engram_t *eng = engram_create(&config);
    assert(eng != NULL);
    
    engram_cue(eng, "hello world", 11);
    
    assert(engram_synapse_count(eng) > 0);
    
    engram_destroy(eng);
    
    printf("Synapse tests passed.\n");
    return 0;
}
