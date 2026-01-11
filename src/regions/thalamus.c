#include "../internal.h"
#include "../regions.h"

int thalamus_init(engram_t *eng) {
    eng->thalamus.attention_threshold = 0.3f;
    eng->thalamus.current_gate = 1.0f;
    eng->thalamus.focus_cluster_id = UINT32_MAX;
    return 0;
}

void thalamus_destroy(engram_t *eng) {
    (void)eng;
}

int thalamus_gate_cue(engram_t *eng, const engram_cue_t *cue) {
    engram_arousal_t state = atomic_load(&eng->brainstem.arousal_state);

    switch (state) {
        case ENGRAM_AROUSAL_WAKE:
            eng->thalamus.current_gate = 1.0f;
            break;
        case ENGRAM_AROUSAL_DROWSY:
            eng->thalamus.current_gate = 0.5f;
            break;
        case ENGRAM_AROUSAL_SLEEP:
            eng->thalamus.current_gate = 0.1f;
            break;
        case ENGRAM_AROUSAL_REM:
            eng->thalamus.current_gate = 0.0f;
            break;
    }

    if (cue->flags & ENGRAM_CUE_FLAG_URGENT) {
        eng->thalamus.current_gate = 1.0f;
    }

    float effective_intensity = cue->intensity * eng->thalamus.current_gate;
    if (effective_intensity < eng->thalamus.attention_threshold) {
        return 0;
    }

    return 1;
}

void thalamus_update_attention(engram_t *eng, uint32_t focus_cluster) {
    eng->thalamus.focus_cluster_id = focus_cluster;
}

float thalamus_get_attention_boost(engram_t *eng, uint32_t cluster_id) {
    if (eng->thalamus.focus_cluster_id == UINT32_MAX) {
        return 1.0f;
    }
    if (cluster_id == eng->thalamus.focus_cluster_id) {
        return 1.5f;
    }
    return 0.8f;
}
