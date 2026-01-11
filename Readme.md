# Engram

A brain-inspired memory system library in C. Engram models human memory through neural pathways and synaptic connections, enabling pattern-based storage and associative recall with natural forgetting.

## Overview

Engram is not a database. It is a living memory system that learns, adapts, and forgets—mirroring how biological neural networks process and retain information.

### Core Principles

- **Pattern Storage**: Data is encoded as activation patterns across neural populations, not as discrete records
- **Associative Memory**: Recall is cue-driven; partial or related inputs activate connected memory traces
- **Synaptic Plasticity**: Connections strengthen with use (long-term potentiation) and weaken without (decay)
- **Natural Forgetting**: Irrelevant memories fade, keeping the system efficient and relevant
- **Always Running**: Like a biological brain, Engram never stops processing—background activity is continuous and self-regulating
- **Adaptive Resource Usage**: Processing intensity scales with activity and available resources, never overloading the host system
- **Circadian Rhythms**: Alternating high-activity and consolidation phases mirror wake/sleep cycles

## Architecture

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              Engram Instance                                 │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                    Brainstem (Always Running Thread)                   │  │
│  │  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐ │  │
│  │  │ Neural Clock    │  │ Arousal System  │  │ Resource Governor       │ │  │
│  │  │ ─────────────── │  │ ─────────────── │  │ ─────────────────────── │ │  │
│  │  │ Continuous tick │  │ Wake/Sleep/REM  │  │ CPU load monitoring     │ │  │
│  │  │ Drives all      │  │ Activity level  │  │ Memory pressure         │ │  │
│  │  │ neural activity │  │ modulation      │  │ Adaptive throttling     │ │  │
│  │  └─────────────────┘  └─────────────────┘  └─────────────────────────┘ │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                     │                                        │
│         ┌───────────────────────────┼───────────────────────────┐            │
│         ▼                           ▼                           ▼            │
│  ┌─────────────────┐  ┌─────────────────────────┐  ┌─────────────────────┐   │
│  │    Thalamus     │  │      Hippocampus        │  │      Amygdala       │   │
│  │ ─────────────── │  │ ─────────────────────── │  │ ─────────────────── │   │
│  │ Sensory gating  │  │ Rapid encoding          │  │ Salience detection  │   │
│  │ Attention focus │  │ Pattern formation       │  │ Emotional tagging   │   │
│  │ Cue routing     │  │ Working memory          │  │ Priority signaling  │   │
│  └────────┬────────┘  └───────────┬─────────────┘  └──────────┬──────────┘   │
│           │                       │                           │              │
│           └───────────────────────┼───────────────────────────┘              │
│                                   ▼                                          │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                            Neocortex                                   │  │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────────┐  │  │
│  │  │ Sensory Regions  │  │ Association Areas│  │ Prefrontal Cortex    │  │  │
│  │  │ Pattern encoding │  │ Cross-modal links│  │ Working memory mgmt  │  │  │
│  │  │ Feature extract  │  │ Concept formation│  │ Goal-directed recall │  │  │
│  │  └──────────────────┘  └──────────────────┘  └──────────────────────┘  │  │
│  │                                                                        │  │
│  │  Long-term storage • Hierarchical organization • Memory consolidation │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                   │                                          │
│                                   ▼                                          │
│  ┌────────────────────────────────────────────────────────────────────────┐  │
│  │                         Neural Substrate                               │  │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐                    │  │
│  │  │ Cluster │──│ Cluster │──│ Cluster │──│ Cluster │  ...               │  │
│  │  │  ┌───┐  │  │  ┌───┐  │  │  ┌───┐  │  │  ┌───┐  │                    │  │
│  │  │  │ N ├──┼──┼──│ N ├──┼──┼──│ N ├──┼──┼──│ N │  │   Synaptic         │  │
│  │  │  └─┬─┘  │  │  └─┬─┘  │  │  └─┬─┘  │  │  └─┬─┘  │   Connections      │  │
│  │  │    │    │  │    │    │  │    │    │  │    │    │                    │  │
│  │  │  ┌─┴─┐  │  │  ┌─┴─┐  │  │  ┌─┴─┐  │  │  ┌─┴─┐  │                    │  │
│  │  │  │ N ├──┼──┼──│ N ├──┼──┼──│ N ├──┼──┼──│ N │  │                    │  │
│  │  │  └───┘  │  │  └───┘  │  │  └───┘  │  │  └───┘  │                    │  │
│  │  └─────────┘  └─────────┘  └─────────┘  └─────────┘                    │  │
│  └────────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Components

#### Brainstem

The always-running core that drives continuous neural activity. Runs as a background thread.

| Component | Description |
|-----------|-------------|
| **Neural Clock** | Generates continuous ticks driving all neural dynamics; frequency adapts to load |
| **Arousal System** | Modulates global activity level; cycles through wake, sleep, and REM-like states |
| **Resource Governor** | Monitors CPU/memory usage; throttles processing to stay within resource budget |

#### Thalamus

Sensory gateway and attention router.

- Gates incoming cues based on current arousal and attention state
- Routes stimuli to appropriate processing regions
- Maintains focus by suppressing irrelevant activations
- Coordinates communication between brain regions

#### Hippocampus

Rapid learning and pattern formation system.

- Encodes incoming cues into sparse distributed representations
- Creates initial bindings between pattern elements
- Maintains working memory for recent activations
- Triggers consolidation to neocortex based on repetition and significance
- Replays recent experiences during low-arousal states

#### Amygdala

Salience and emotional processing.

- Tags memories with importance/priority scores
- Detects novelty and unexpected patterns
- Modulates learning rate based on salience
- Influences memory persistence through emotional weighting

#### Neocortex

Long-term storage and hierarchical organization.

| Region | Function |
|--------|----------|
| **Sensory Areas** | Encode modality-specific patterns; extract features |
| **Association Areas** | Form cross-modal links; build abstract concepts |
| **Prefrontal Cortex** | Manage working memory; direct goal-oriented recall |

- Consolidates patterns from hippocampus during sleep-like states
- Builds hierarchical representations through pattern abstraction
- Optimizes storage by merging similar patterns
- Manages memory decay and garbage collection

#### Neural Substrate

The foundation layer managing neurons and their synaptic connections.

| Component | Description |
|-----------|-------------|
| **Neuron** | Processing unit with activation state, threshold, refractory period, and fatigue |
| **Synapse** | Weighted connection with plasticity, eligibility traces, and last-active timestamp |
| **Cluster** | Spatially grouped neurons for cache-efficient processing and lateral inhibition |
| **Pathway** | Frequently co-activated neuron chains representing consolidated memory traces |

## Data Structures

### Neuron

```c
typedef struct engram_neuron {
    uint32_t id;
    float activation;
    float threshold;
    float fatigue;
    uint32_t last_fired_tick;
    uint32_t fire_count;
    uint16_t cluster_id;
    uint8_t refractory_remaining;
    uint8_t flags;
} engram_neuron_t;
```

### Synapse

```c
typedef struct engram_synapse {
    uint32_t pre_neuron_id;
    uint32_t post_neuron_id;
    float weight;
    float eligibility_trace;
    uint32_t last_active_tick;
    uint16_t potentiation_count;
    uint16_t flags;
} engram_synapse_t;
```

### Cue

```c
typedef struct engram_cue {
    const void *data;
    size_t size;
    float intensity;
    float salience;
    uint32_t modality;
    uint32_t flags;
} engram_cue_t;
```

### Recall Result

```c
typedef struct engram_recall {
    float *pattern;
    size_t pattern_size;
    float confidence;
    float familiarity;
    uint32_t pathway_id;
    uint32_t age_ticks;
} engram_recall_t;
```

## API

### Lifecycle

```c
engram_t *engram_create(const engram_config_t *config);
void engram_destroy(engram_t *eng);
```

`engram_create` spawns the brainstem thread which runs continuously until `engram_destroy` is called. The brain is always processing.

### Memory Operations

```c
int engram_stimulate(engram_t *eng, const engram_cue_t *cue);
int engram_recall(engram_t *eng, const engram_cue_t *cue, engram_recall_t *result);
int engram_associate(engram_t *eng, const engram_cue_t *cues, size_t count);
```

**Note**: `engram_recall` uses internal buffers managed by the library. No cleanup is required - just use the result and move on. The data remains valid until the next `engram_recall` call on the same engram.

### Arousal Control

```c
typedef enum {
    ENGRAM_AROUSAL_WAKE,
    ENGRAM_AROUSAL_DROWSY,
    ENGRAM_AROUSAL_SLEEP,
    ENGRAM_AROUSAL_REM
} engram_arousal_t;

int engram_set_arousal(engram_t *eng, engram_arousal_t state);
engram_arousal_t engram_get_arousal(engram_t *eng);
int engram_set_arousal_auto(engram_t *eng, int enable);
```

Arousal states affect processing:

| State | External Cues | Internal Processing | Consolidation | Resource Usage |
|-------|---------------|---------------------|---------------|----------------|
| Wake | Full | Active | Minimal | High |
| Drowsy | Reduced | Declining | Starting | Medium |
| Sleep | Gated | Replay only | Maximum | Low |
| REM | Blocked | Pattern mixing | Creative links | Medium |

### Resource Management

```c
typedef struct engram_resource_limits {
    float max_cpu_percent;
    size_t max_memory_bytes;
    uint32_t max_ticks_per_second;
    uint32_t min_ticks_per_second;
} engram_resource_limits_t;

int engram_set_resource_limits(engram_t *eng, const engram_resource_limits_t *limits);
void engram_get_resource_usage(engram_t *eng, engram_resource_usage_t *usage);
```

The resource governor continuously monitors system load and adjusts tick rate and batch sizes to stay within limits. Processing never stops but intensity scales.

### Persistence

```c
int engram_save(engram_t *eng, const char *path);
engram_t *engram_load(const char *path, const engram_config_t *config);
int engram_checkpoint(engram_t *eng);
```

### Introspection

```c
void engram_stats(engram_t *eng, engram_stats_t *stats);
size_t engram_pathway_count(engram_t *eng);
float engram_memory_pressure(engram_t *eng);
uint64_t engram_tick_count(engram_t *eng);
float engram_current_tick_rate(engram_t *eng);
```

## Configuration

```c
typedef struct engram_config {
    uint32_t neuron_count;
    uint32_t cluster_size;
    
    float initial_threshold;
    float decay_rate;
    float learning_rate;
    float fatigue_rate;
    float recovery_rate;
    
    uint32_t hippocampus_capacity;
    uint32_t working_memory_slots;
    
    float consolidation_threshold;
    uint32_t replay_batch_size;
    
    engram_resource_limits_t resource_limits;
    
    uint32_t arousal_cycle_ticks;
    int auto_arousal;
    
    engram_allocator_t *allocator;
} engram_config_t;

engram_config_t engram_config_default(void);
```

### Configuration Profiles

| Profile | Use Case | Neurons | Resources |
|---------|----------|---------|------------|
| Minimal | Embedded systems | 10K | <1% CPU, 1MB |
| Standard | Desktop applications | 100K | <5% CPU, 50MB |
| Performance | Dedicated servers | 1M+ | <25% CPU, 500MB |

```c
engram_config_t engram_config_minimal(void);
engram_config_t engram_config_standard(void);
engram_config_t engram_config_performance(void);
```

## Algorithms

### Encoding

Incoming data is hashed into a sparse distributed representation (SDR) using locality-sensitive hashing. This produces a pattern of neuron activations that:

- Is consistent for identical inputs
- Shows overlap proportional to input similarity
- Distributes information across the neural population
- Sparsity target: 2-5% of neurons active per pattern

### Learning (Spike-Timing Dependent Plasticity)

Synaptic weights update based on correlated activity with temporal asymmetry:

```
If pre fires before post (causal):     Δw = +A₊ × exp(-Δt/τ₊) × eligibility
If post fires before pre (acausal):    Δw = -A₋ × exp(+Δt/τ₋) × eligibility
```

Eligibility traces allow credit assignment over time, enabling learning from delayed outcomes.

### Continuous Processing Loop

The brainstem runs this loop continuously:

```
while (running) {
    t_start = now()
    
    if (resource_governor_permits()) {
        switch (arousal_state) {
            case WAKE:
                process_pending_cues()
                propagate_activations()
                apply_plasticity()
                update_fatigue()
                break
            case DROWSY:
                reduce_sensitivity()
                begin_consolidation()
                break
            case SLEEP:
                replay_hippocampus_traces()
                consolidate_to_neocortex()
                prune_weak_synapses()
                break
            case REM:
                random_activation_mixing()
                novel_association_formation()
                break
        }
        
        decay_inactive_synapses()
        update_arousal_if_auto()
    }
    
    adaptive_sleep(t_start, resource_limits)
}
```

### Recall (Pattern Completion)

Given a partial cue:

1. Thalamus gates cue based on arousal/attention
2. Encode cue to SDR pattern
3. Activate matching neurons in hippocampus
4. Propagate activation through weighted synapses
5. Apply lateral inhibition within clusters (winner-take-all)
6. Check neocortex for consolidated matches
7. Iterate until stable attractor or timeout
8. Return strongest pathway as recall result

### Decay and Forgetting

Multiple decay mechanisms mirror biological memory:

```
Synaptic decay:    w(t+1) = w(t) × (1 - λ_synapse × Δt)
Activation decay:  a(t+1) = a(t) × (1 - λ_activation × Δt)
Fatigue recovery:  f(t+1) = f(t) × (1 - λ_recovery × Δt)
```

Synapses below threshold are pruned. Disconnected neurons return to the free pool. Garbage collection runs during sleep states.

### Consolidation (Memory Replay)

During sleep-like states, recent hippocampal patterns replay:

1. Select traces with high activation count or salience
2. Reactivate pattern at reduced intensity
3. Strengthen hippocampus → neocortex pathways
4. Create compressed neocortical representation
5. Gradually reduce hippocampal trace strength
6. Form novel associations during REM-like mixing

### Adaptive Resource Management

The resource governor implements a feedback loop:

```
cpu_usage = measure_cpu_percent()
mem_usage = measure_memory()

if (cpu_usage > limits.max_cpu_percent) {
    tick_rate = tick_rate × 0.9
    batch_size = batch_size × 0.8
}
else if (cpu_usage < limits.max_cpu_percent × 0.5) {
    tick_rate = min(tick_rate × 1.1, limits.max_ticks_per_second)
    batch_size = min(batch_size × 1.2, max_batch)
}

if (mem_usage > limits.max_memory_bytes × 0.9) {
    trigger_aggressive_pruning()
    reduce_working_memory()
}
```

## Memory Layout

Designed for cache efficiency:

```
┌─────────────────────────────────────────┐
│ Cluster 0                               │
│ ┌─────┬─────┬─────┬─────┬─────┬─────┐   │
│ │ N0  │ N1  │ N2  │ N3  │ ... │ Nn  │   │  Contiguous neuron array
│ └─────┴─────┴─────┴─────┴─────┴─────┘   │
│ ┌───────────────────────────────────┐   │
│ │ Synapse array for cluster         │   │  Sorted by pre_neuron_id
│ └───────────────────────────────────┘   │
├─────────────────────────────────────────┤
│ Cluster 1                               │
│ ...                                     │
└─────────────────────────────────────────┘
```

## Build

### Requirements

- C11 compiler (GCC, Clang, MSVC)
- CMake 3.15+
- No external dependencies

### Compile

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `ENGRAM_BUILD_TESTS` | ON | Build test suite |
| `ENGRAM_BUILD_EXAMPLES` | ON | Build example programs |
| `ENGRAM_ENABLE_SIMD` | ON | Enable SIMD optimizations |
| `ENGRAM_THREAD_SAFE` | OFF | Enable thread-safe operations |

### Install

```bash
cmake --install . --prefix /usr/local
```

## Usage Example

```c
#include <engram.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    engram_config_t config = engram_config_standard();
    config.resource_limits.max_cpu_percent = 5.0f;
    config.auto_arousal = 1;

    engram_t *brain = engram_create(&config);
    
    const char *facts[] = {
        "The sky is blue",
        "Water is wet",
        "Fire is hot",
        "The sky has clouds",
        "Rain falls from clouds"
    };
    
    for (size_t i = 0; i < 5; i++) {
        engram_cue_t cue = {
            .data = facts[i],
            .size = strlen(facts[i]),
            .intensity = 1.0f,
            .salience = 0.8f,
            .modality = ENGRAM_MODALITY_TEXT,
            .flags = 0
        };
        engram_stimulate(brain, &cue);
        usleep(100000);
    }
    
    engram_set_arousal(brain, ENGRAM_AROUSAL_SLEEP);
    sleep(2);
    engram_set_arousal(brain, ENGRAM_AROUSAL_WAKE);

    const char *query = "sky";
    engram_cue_t query_cue = {
        .data = query,
        .size = strlen(query),
        .intensity = 0.8f,
        .salience = 1.0f,
        .modality = ENGRAM_MODALITY_TEXT,
        .flags = ENGRAM_CUE_RECALL
    };

    engram_recall_t result;
    if (engram_recall(brain, &query_cue, &result) == 0) {
        if (result.data) {
            printf("Recalled: %.*s\n", (int)result.data_size, (char *)result.data);
        }
        printf("Confidence: %.2f, Familiarity: %.2f\n", 
               result.confidence, result.familiarity);
    }

    engram_stats_t stats;
    engram_stats(brain, &stats);
    printf("Ticks: %lu, Pathways: %u, Tick rate: %.1f/s\n",
           engram_tick_count(brain),
           stats.pathway_count,
           engram_current_tick_rate(brain));

    engram_destroy(brain);
    return 0;
}
```

## Project Structure

```
engram/
├── CMakeLists.txt
├── include/
│   └── engram/
│       ├── engram.h
│       ├── types.h
│       ├── config.h
│       └── allocator.h
├── src/
│   ├── core/
│   │   ├── engram.c
│   │   ├── neuron.c
│   │   ├── synapse.c
│   │   ├── cluster.c
│   │   └── pathway.c
│   ├── regions/
│   │   ├── brainstem.c
│   │   ├── thalamus.c
│   │   ├── hippocampus.c
│   │   ├── amygdala.c
│   │   └── neocortex.c
│   ├── processes/
│   │   ├── encoding.c
│   │   ├── plasticity.c
│   │   ├── decay.c
│   │   ├── consolidation.c
│   │   └── replay.c
│   ├── system/
│   │   ├── clock.c
│   │   ├── arousal.c
│   │   ├── governor.c
│   │   └── persistence.c
│   └── platform/
│       ├── thread_posix.c
│       ├── thread_win32.c
│       ├── time_posix.c
│       └── time_win32.c
├── tests/
│   ├── test_neuron.c
│   ├── test_synapse.c
│   ├── test_encoding.c
│   ├── test_recall.c
│   ├── test_consolidation.c
│   ├── test_arousal.c
│   └── test_governor.c
└── examples/
    ├── basic.c
    ├── associative.c
    ├── streaming.c
    └── resource_limited.c
```

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Stimulate | O(k) | k = active neurons in pattern |
| Recall | O(k × d × i) | d = avg synapses per neuron, i = iterations |
| Tick (wake) | O(a) | a = active neuron count |
| Tick (sleep) | O(r) | r = replay batch size |
| Decay pass | O(s) | s = total synapse count (amortized) |
| Consolidate | O(p × b) | p = pending traces, b = batch size |

### Resource Usage Targets

| Profile | Idle CPU | Peak CPU | Memory | Tick Rate |
|---------|----------|----------|--------|------------|
| Minimal | <0.1% | <1% | <1MB | 1-10/s |
| Standard | <0.5% | <5% | <50MB | 10-100/s |
| Performance | <2% | <25% | <500MB | 100-1000/s |

## Design Decisions

**Why not use a graph database?**
Graph databases store explicit relationships. Engram stores distributed patterns where meaning emerges from activation dynamics, enabling fuzzy matching and graceful degradation.

**Why lossy storage?**
Perfect recall isn't the goal. Relevant information strengthens; irrelevant fades. This keeps the system bounded and focused, like biological memory.

**Why always running?**
Biological brains never stop. Even during sleep, neurons fire, memories consolidate, and synapses adjust. A static memory is a dead memory. Continuous background processing enables consolidation, creative association, and natural forgetting without explicit triggers.

**Why adaptive resource management?**
A brain that consumes 100% CPU is useless to its host. The resource governor ensures Engram behaves like a well-mannered background process—active but not intrusive, scaling its intensity to available resources.

**Why arousal states?**
Different cognitive modes serve different purposes. Wake state handles external stimuli; sleep consolidates memories; REM enables creative recombination. Cycling through states automatically optimizes the balance between learning and consolidation.

**Why clusters?**
Cache locality. Neurons that connect frequently should reside near each other in memory. Clusters provide spatial organization for predictable memory access patterns and enable efficient lateral inhibition.

## License

MIT 