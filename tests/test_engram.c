#include <engram/engram.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

int main(void) {
    printf("Testing engram cue and recall...\n");
    
    engram_config_t config = engram_config_default();
    config.neuron_count = 4096;
    config.synapse_pool_size = 32768;
    
    engram_t *eng = engram_create(&config);
    assert(eng != NULL);
    
    engram_cue(eng, "the sun is bright", 17);
    engram_cue(eng, "the moon is pale", 16);
    engram_cue(eng, "stars shine at night", 20);
    
    usleep(300000);
    
    engram_response_t resp = engram_cue(eng, "sun", 3);
    
    assert(resp.text != NULL);
    printf("Response: %s (confidence: %.2f)\n", resp.text, resp.confidence);
    
    engram_response_free(&resp);
    engram_destroy(eng);
    
    printf("Engram cue/recall tests passed.\n");
    return 0;
}
