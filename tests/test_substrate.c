#include <engram/engram.h>
#include <stdio.h>
#include <assert.h>

int main(void) {
    printf("Testing substrate...\n");
    
    engram_config_t config = engram_config_default();
    config.neuron_count = 1024;
    config.synapse_pool_size = 8192;
    
    engram_t *eng = engram_create(&config);
    assert(eng != NULL);
    
    assert(engram_neuron_count(eng) == 1024);
    assert(engram_synapse_count(eng) == 0);
    assert(engram_pathway_count(eng) == 0);
    
    engram_destroy(eng);
    
    printf("Substrate tests passed.\n");
    return 0;
}
