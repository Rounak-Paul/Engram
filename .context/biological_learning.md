# Biological Learning Model

Engram's learning was refactored from a Hebbian/associative model into a
spiking, neuromodulated model that matches real cortical-hippocampal dynamics.

## Mechanisms implemented

1. Integrate-and-fire neuron (`propagate.c`)
   - `neuron.potential` accumulates charge; fires only when `>= threshold`.
   - On fire: `activation` set, potential reset, `refractory_until = tick + ENGRAM_DEFAULT_REFRACTORY`.
   - `propagate_activation` returns the list of neurons that actually fired.

2. STDP (`propagate_stdp` in `propagate.c`)
   - Weight change depends on pre/post `last_fire_tick` order.
   - Pre-before-post (dt>0) potentiates; post-before-pre (dt<0) depresses.
   - Magnitude `exp(-|dt| / ENGRAM_DEFAULT_STDP_WINDOW)`, gated by dopamine and per-synapse plasticity.

3. Inhibitory synapses (`synapse.sign`: +1 excitatory, -1 inhibitory)
   - Lateral inhibition added in `engram_cue_internal` for mid-similarity competitors (0.3–0.5).
   - Propagation multiplies charge by `sign`; potential floored at 0.

4. Neuromodulation (`substrate.dopamine`, `substrate.acetylcholine`)
   - `substrate_modulate(novelty)` updates both each cue.
   - Dopamine gates STDP learning rate; decays in `substrate_decay`.
   - Acetylcholine drives cortical pattern completion strength.

5. Hippocampal replay (`hippocampus_imprint` / `hippocampus_replay`)
   - Co-firing patterns stored in a ring buffer of `replay_trace_t` (cap `REPLAY_TRACE_CAPACITY`).
   - Replay runs during the periodic decay tick, re-applying STDP offline (consolidation).

6. Cortical pattern completion (`cortex_complete`)
   - Top-down: seed neurons drive strongly-connected excitatory targets over potential threshold.
   - Recruited neurons activate and gain importance. Walks synapse graph only (no full scan).

7. Per-synapse plasticity (`synapse.plasticity`)
   - Scales each weight update; recovers toward 1.0 in decay.

## Config (types.h `engram_config_t`)
`fire_threshold`, `stdp_window`, `inhibition_strength`, `refractory_period`, `replay_passes`.

## Persistence
File magic bumped to `...0003`. Neuron format adds `threshold`, `last_fire_tick`;
synapse format adds `plasticity`, `sign`. Old `.dat` files are rejected (no back-compat by design).

## Bug fixed during refactor
`find_or_create_neuron` held a `neuron_t*` (`best_match`) across `substrate_alloc_neuron`,
which can realloc the neuron array -> heap-use-after-free (latent; only triggered when
neuron capacity grows). Now captures stable `engram_id_t` and re-resolves pointers by id.

## Verification
- ASan + UBSan clean over 3000-cue stress with capacity growth + save/load roundtrip.
- Benchmark on par with baseline: INSERT ~50us/op, query latency <90us at 200K neurons.
- Bulk insert path unchanged (does not run the learning machinery).
