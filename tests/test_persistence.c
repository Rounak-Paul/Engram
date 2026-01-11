#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

int main(void) {
    printf("Testing persistence...\n");
    
    const char *path = "/tmp/test_brain.engram";
    
    {
        engram_config_t config = engram_config_default();
        config.neuron_count = 2048;
        config.synapse_pool_size = 16384;
        
        engram_t *eng = engram_create(&config);
        assert(eng != NULL);
        
        engram_cue(eng, "memory persistence test", 23);
        engram_cue(eng, "data should survive", 19);
        
        usleep(200000);
        
        uint64_t synapses_before = engram_synapse_count(eng);
        
        int result = engram_save(eng, path);
        assert(result == 0);
        
        engram_destroy(eng);
        
        engram_t *loaded = engram_load(path);
        assert(loaded != NULL);
        
        uint64_t synapses_after = engram_synapse_count(loaded);
        assert(synapses_after == synapses_before);
        
        engram_destroy(loaded);
    }
    
    unlink(path);
    
    printf("Persistence tests passed.\n");
    return 0;
}
