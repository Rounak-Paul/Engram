# Engram

A brain-inspired memory system in C with optional Vulkan GPU acceleration. Engram models human memory through neural pathways and synaptic plasticity, enabling associative learning and cue-based recall.

## Overview

Engram is not a database. It stores patterns, builds associations, and recalls through neural activation—like biological memory. Data is encoded as distributed neural activity, not discrete records.

### Key Features

- **Associative Memory**: Recall by cue, not by key. Partial matches activate related memories.
- **Continuous Learning**: Every interaction strengthens or creates neural pathways.
- **Natural Forgetting**: Unused pathways decay. Frequently accessed memories strengthen.
- **Always Running**: Background threads continuously process, consolidate, and adapt.
- **Simple API**: Three functions: `engram_create`, `engram_cue`, `engram_destroy`.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Engram Instance                        │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ Hippocampus  │  │    Cortex    │  │    Wernicke     │   │
│  │   Thread     │  │    Thread    │  │   (Language)    │   │
│  │ ──────────── │  │ ──────────── │  │ ────────────────│   │
│  │ Encoding     │  │ Consolidation│  │ Tokenization    │   │
│  │ Association  │  │ Decay/Prune  │  │ Pattern Embed   │   │
│  │ Working Mem  │  │ Strengthen   │  │ Vocabulary      │   │
│  └──────────────┘  └──────────────┘  └──────────────────┘   │
│                           │                                  │
│                           ▼                                  │
│  ┌──────────────────────────────────────────────────────────┐│
│  │                   Neural Substrate                       ││
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐                   ││
│  │  │ Neurons │──│Synapses │──│Pathways │                   ││
│  │  └─────────┘  └─────────┘  └─────────┘                   ││
│  │  Activation patterns propagate through weighted          ││
│  │  connections. Pathways represent consolidated memory     ││
│  └──────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

## API

```c
#include <engram/engram.h>

// Create brain with default config
engram_t *brain = engram_create(NULL);

// Cue the brain - learns and responds
engram_response_t resp = engram_cue(brain, "the sky is blue", 15);
printf("Response: %s (confidence: %.2f)\n", resp.text, resp.confidence);
engram_response_free(&resp);

// Later, cue with related input
resp = engram_cue(brain, "sky", 3);
// May recall associations with "blue", "sky", etc.

// Save and load
engram_save(brain, "brain.engram");
engram_t *loaded = engram_load("brain.engram");

// Cleanup
engram_destroy(brain);
```

### Configuration

```c
engram_config_t config = engram_config_default();
config.neuron_count = 65536;          // Neural population size
config.synapse_pool_size = 1048576;   // Maximum synaptic connections
config.learning_rate = 0.01f;         // Synaptic plasticity rate
config.decay_rate = 0.0001f;          // Forgetting rate
config.hippocampus_tick_ms = 50;      // Encoding thread interval
config.consolidation_tick_ms = 1000;  // Consolidation thread interval
config.use_vulkan = 1;                // Enable GPU acceleration

engram_t *brain = engram_create(&config);
```

## Build

### Requirements

- C11 compiler (GCC, Clang, MSVC)
- CMake 3.15+
- Vulkan SDK (optional, for GPU acceleration)

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
| `ENGRAM_ENABLE_VULKAN` | ON | Enable Vulkan compute |

### Run Tests

```bash
cd build
ctest --output-on-failure
```

## Examples

### Conversation

Interactive conversation where the brain learns from everything you say:

```bash
./examples/conversation
```

### Learn from Files

Teach the brain from text files:

```bash
./examples/learn_file knowledge/*.txt -o brain.engram
./examples/conversation brain.engram
```

### Basic Usage

```bash
./examples/basic
```

## How It Works

### Encoding

Input text is tokenized and converted to sparse distributed patterns across the neural population. These patterns activate neurons based on hash-derived indices.

### Learning

When patterns co-activate, synapses between active neurons strengthen (Hebbian learning). Repeated patterns form pathways—consolidated memory traces.

### Recall

A cue pattern propagates through the synaptic network. Matching pathways activate, and the strongest activations determine the response. Lateral inhibition ensures sparse, distinct recalls.

### Forgetting

Synapses decay without use. Weak synapses are pruned. Pathways lose strength over time unless reinforced. This keeps the memory bounded and relevant.

### Background Processing

- **Hippocampus Thread**: Encodes working memory, forms associations
- **Cortex Thread**: Consolidates pathways, applies decay, prunes weak synapses

## Design Philosophy

**Why not exact storage?**
Human memory doesn't store exact data. It stores patterns and associations. Engram mirrors this—recall is approximate, associative, and context-dependent.

**Why continuous threads?**
Biological brains never stop processing. Even during idle time, memories consolidate and connections adjust. Background threads enable this continuous adaptation.

**Why forgetting?**
Unbounded memory is noise. Forgetting keeps the system efficient, relevant, and focused on frequently accessed information.

## License

MIT
