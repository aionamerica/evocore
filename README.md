# Evocore - Meta-Evolutionary Framework

**Version:** 1.0.0
**Language:** C99

A domain-agnostic meta-evolutionary framework for evolutionary computation research and optimization with organic learning capabilities.

## Overview

Evocore provides a flexible foundation for building evolutionary algorithms that can evolve not just solutions, but the evolutionary parameters themselves. The framework is designed to be:

- **Domain-agnostic** - Works with any problem through callback-based fitness evaluation
- **Pure C** - No external dependencies, maximum portability
- **Extensible** - Clean separation between framework and domain logic
- **High Performance** - Multi-threaded CPU evaluation and GPU acceleration via CUDA
- **Full Featured** - Checkpointing, monitoring, persistence, and deployment automation
- **Organic Learning** - Context-aware, temporal, and adaptive optimization strategies

## Features

### Core Evolution
- **Meta-Evolution**: Evolves evolutionary parameters (mutation rates, selection pressure, population size)
- **GPU Acceleration**: CUDA kernels for parallel fitness evaluation
- **Persistence**: JSON serialization for genomes, populations, and meta-populations
- **Checkpointing**: Auto-save with compression and resume capability
- **Monitoring**: Real-time statistics, convergence detection, stagnation alerts
- **Memory Optimization**: Arena allocators and memory pools for efficient bulk allocation

### Organic Learning (New in 1.0.0)
- **Weighted Statistics**: Fitness-weighted statistical learning with variance tracking
- **Context Learning**: Multi-dimensional context awareness for domain adaptation
- **Temporal Learning**: Time-bucketed learning with regime change detection
- **Exploration Control**: Adaptive strategies (Fixed, Decay, Adaptive, UCB1, Boltzmann)
- **Parameter Synthesis**: Cross-context knowledge transfer (Average, Weighted, Trend, Regime, Ensemble)

## Building

### Prerequisites

- C99-compatible compiler (GCC 4.8+, Clang 3.4+)
- GNU Make 3.81+
- CUDA Toolkit 11.0+ (optional, for GPU support)
- pthread library (included with glibc)

### Quick Start

```bash
# Build library and examples
make

# Run the sphere function example
make run

# Build and run tests
make test

# Clean build artifacts
make clean
```

### Build Options

```bash
make                    # Build without CUDA
make CUDA=yes           # Build with CUDA support
make OMP=yes            # Build with OpenMP (future)
make debug              # Debug build with symbols
make valgrind           # Run with memory leak detection
make benchmark          # Run performance benchmarks
```

## Project Structure

```
evocore/
├── include/evocore/          # Public headers
│   ├── evocore.h            # Main umbrella header
│   ├── genome.h             # Genome abstraction
│   ├── population.h         # Population management
│   ├── domain.h             # Domain interface
│   ├── meta.h               # Meta-evolution layer
│   ├── gpu.h                # GPU acceleration
│   ├── persist.h            # Serialization & checkpointing
│   ├── stats.h              # Statistics & monitoring
│   ├── memory.h             # Memory management API
│   ├── arena.h              # Arena allocator
│   ├── config.h             # INI configuration
│   ├── error.h              # Error handling
│   ├── weighted.h           # Weighted statistics (NEW)
│   ├── context.h            # Context learning (NEW)
│   ├── temporal.h           # Temporal learning (NEW)
│   ├── exploration.h        # Exploration control (NEW)
│   ├── synthesis.h          # Parameter synthesis (NEW)
│   ├── fitness.h            # Fitness interface
│   ├── optimize.h           # Optimization utilities
│   └── log.h                # Logging system
├── src/                     # Implementation
│   ├── cuda/                # CUDA kernels
│   ├── arena.c              # Arena allocator
│   ├── memory.c             # Memory tracking
│   ├── weighted.c           # Weighted statistics (NEW)
│   ├── context.c            # Context learning (NEW)
│   ├── temporal.c           # Temporal learning (NEW)
│   ├── exploration.c        # Exploration control (NEW)
│   ├── synthesis.c          # Parameter synthesis (NEW)
│   └── ...
├── examples/                # Example programs
│   ├── sphere_function.c    # Sphere function optimization
│   ├── trading_domain.c     # Trading strategy domain
│   ├── tsp_domain.c         # Traveling salesman domain
│   ├── meta_demo.c          # Meta-evolution demo
│   ├── gpu_benchmark.c      # GPU performance benchmark
│   ├── checkpoint_demo.c    # Checkpoint/save-resume demo
│   ├── monitoring_demo.c    # Statistics monitoring demo
│   └── organic_learning_demo.c  # Organic learning demo (NEW)
├── tests/                   # Test programs
│   ├── test_genome.c        # Genome tests (15)
│   ├── test_population.c    # Population tests (10)
│   ├── test_config.c        # Configuration tests (11)
│   ├── test_domain.c        # Domain tests (16)
│   ├── test_meta.c          # Meta-evolution tests (19)
│   ├── test_gpu.c           # GPU tests (18)
│   ├── test_persist.c       # Persistence tests (8)
│   ├── test_stats.c         # Statistics tests (11)
│   ├── test_weighted.c      # Weighted stats tests (15)
│   ├── test_context.c       # Context learning tests (10)
│   ├── test_temporal.c      # Temporal learning tests (11)
│   ├── test_exploration.c   # Exploration tests (16)
│   ├── test_synthesis.c     # Synthesis tests (15)
│   └── test_integration.c   # Integration tests (7)
├── deployment/              # Production deployment
│   ├── scripts/            # install, uninstall, health_check
│   ├── docker/             # Dockerfile, docker-compose.yml
│   └── configs/            # production.conf, logging.conf
├── shell.nix                # Nix shell with CUDA dependencies
└── Makefile
```

## Usage Example

```c
#include "evocore.h"

/* Fitness function */
double my_fitness(const evocore_genome_t *genome, void *context) {
    double values[10];
    evocore_genome_read(genome, 0, values, sizeof(values));

    double sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += values[i] * values[i];
    }
    return -sum;  /* Negative because framework maximizes */
}

int main(void) {
    /* Initialize population */
    evocore_population_t pop;
    evocore_population_init(&pop, 100);

    /* Run evolution */
    for (int gen = 0; gen < 100; gen++) {
        evocore_population_evaluate(&pop, my_fitness, NULL);
        evocore_population_sort(&pop);

        /* Selection, breeding, mutation... */
    }

    /* Cleanup */
    evocore_population_cleanup(&pop);
    return 0;
}
```

## GPU Acceleration

### Building with CUDA

```bash
make CUDA=yes
```

### GPU Performance

```bash
./build/gpu_benchmark
```

Typical results:
- **Serial CPU:** ~2M evals/sec
- **Parallel CPU (16 threads):** ~12M evals/sec
- **GPU (CUDA):** ~80M evals/sec

## Checkpointing

```c
/* Save checkpoint */
evocore_checkpoint_t checkpoint;
evocore_checkpoint_create(&checkpoint, &pop, &domain, &meta_pop);
evocore_checkpoint_save(&checkpoint, "checkpoint.json", NULL);
evocore_checkpoint_free(&checkpoint);

/* Load checkpoint */
evocore_checkpoint_load("checkpoint.json", &checkpoint);
evocore_checkpoint_restore(&checkpoint, &pop, &domain, &meta_pop);
evocore_checkpoint_free(&checkpoint);
```

### Auto-Checkpointing

```c
evocore_auto_checkpoint_config_t config = {
    .enabled = true,
    .every_n_generations = 50,
    .directory = "./checkpoints",
    .max_checkpoints = 5
};

evocore_checkpoint_manager_t *mgr = evocore_checkpoint_manager_create(&config);

/* During evolution */
evocore_checkpoint_manager_update(mgr, &pop, &domain, &meta_pop);
```

## Meta-Evolution

```c
/* Initialize meta-population */
evocore_meta_population_t meta_pop;
evocore_meta_population_init(&meta_pop, 10);

/* Evolve parameters */
for (int meta_gen = 0; meta_gen < 100; meta_gen++) {
    evocore_meta_evaluate(&meta_pop, evolve_with_params, NULL);
    evocore_meta_population_evolve(&meta_pop);
}

/* Get best parameters */
evocore_meta_params_t best = evocore_meta_population_best_params(&meta_pop);
```

## Configuration

```ini
[evolution]
population_size = 500
max_generations = 10000

[selection]
tournament_size = 5
elite_count = 10

[mutation]
rate = 0.1

[meta_evolution]
enabled = true
meta_population_size = 10

[checkpointing]
enabled = true
every_n_generations = 100
directory = /var/lib/evocore/checkpoints

[monitoring]
enable_alerts = true
stagnation_threshold = 100
```

## Running Examples

```bash
./build/sphere_function          # Sphere function optimization
./build/trading_domain           # Trading strategy domain
./build/tsp_domain               # Traveling salesman domain
./build/meta_demo                # Meta-evolution demo
./build/gpu_benchmark            # GPU performance benchmark
./build/checkpoint_demo          # Checkpoint/save-resume demo
./build/monitoring_demo          # Statistics monitoring demo
./build/organic_learning_demo    # Organic learning capabilities (NEW)
```

## Deployment

### Using Installation Script

```bash
sudo deployment/scripts/install.sh
```

### Using Docker

```bash
cd deployment/docker
docker-compose up evocore
```

## API Documentation

### Core Types

| Type | Description |
|------|-------------|
| `evocore_genome_t` | Opaque byte array encoding a solution |
| `evocore_individual_t` | Genome + fitness pair |
| `evocore_population_t` | Collection of individuals |
| `evocore_meta_population_t` | Meta-evolution state |
| `evocore_gpu_context_t` | GPU acceleration context |
| `evocore_checkpoint_t` | Checkpoint data |

### Organic Learning Types (New in 1.0.0)

| Type | Description |
|------|-------------|
| `evocore_weighted_array_t` | Fitness-weighted statistics array |
| `evocore_context_system_t` | Multi-dimensional context learning |
| `evocore_temporal_system_t` | Time-bucketed temporal learning |
| `evocore_exploration_t` | Exploration controller |
| `evocore_bandit_t` | Multi-armed bandit |
| `evocore_synthesis_request_t` | Parameter synthesis request |

### Key Functions

| Function | Purpose |
|----------|---------|
| `evocore_population_init()` | Create a population |
| `evocore_population_evaluate()` | Evaluate all individuals |
| `evocore_genome_crossover()` | Uniform crossover |
| `evocore_genome_mutate()` | Random mutation |
| `evocore_gpu_init()` | Initialize GPU context |
| `evocore_checkpoint_save()` | Save checkpoint |
| `evocore_meta_population_evolve()` | Evolve parameters |

### Organic Learning Functions (New in 1.0.0)

| Function | Purpose |
|----------|---------|
| `evocore_weighted_array_create()` | Create weighted statistics |
| `evocore_weighted_array_update()` | Update with fitness-weighted value |
| `evocore_weighted_array_get_variance()` | Get weighted variance |
| `evocore_context_system_create()` | Create context learning system |
| `evocore_context_learn()` | Learn from experience |
| `evocore_context_sample()` | Sample from context distribution |
| `evocore_temporal_create()` | Create temporal learning system |
| `evocore_temporal_learn()` | Learn timestamped experience |
| `evocore_temporal_get_organic_mean()` | Get unbiased temporal mean |
| `evocore_temporal_get_trend()` | Detect parameter trends |
| `evocore_temporal_detect_regime_change()` | Detect regime changes |
| `evocore_exploration_create()` | Create exploration controller |
| `evocore_exploration_update()` | Update exploration rate |
| `evocore_exploration_should_explore()` | Decide to explore/exploit |
| `evocore_bandit_create()` | Create multi-armed bandit |
| `evocore_bandit_select_ucb()` | UCB1 selection |
| `evocore_boltzmann_select()` | Boltzmann selection |
| `evocore_synthesis_request_create()` | Create synthesis request |
| `evocore_synthesis_execute()` | Execute parameter synthesis |
| `evocore_param_distance()` | Calculate parameter distance |
| `evocore_param_similarity()` | Calculate parameter similarity |

### Exploration Strategies

| Strategy | Description |
|----------|-------------|
| `EVOCORE_EXPLORE_FIXED` | Constant exploration rate |
| `EVOCORE_EXPLORE_DECAY` | Exponential decay over generations |
| `EVOCORE_EXPLORE_ADAPTIVE` | Adjusts based on recent performance |
| `EVOCORE_EXPLORE_UCB1` | Upper Confidence Bound (bandit) |
| `EVOCORE_EXPLORE_BOLTZMANN` | Simulated annealing |

### Synthesis Strategies

| Strategy | Description |
|----------|-------------|
| `EVOCORE_SYNTHESIS_AVERAGE` | Simple average of sources |
| `EVOCORE_SYNTHESIS_WEIGHTED` | Weighted average by confidence |
| `EVOCORE_SYNTHESIS_TREND` | Project based on detected trend |
| `EVOCORE_SYNTHESIS_REGIME` | Select by fitness regime |
| `EVOCORE_SYNTHESIS_ENSEMBLE` | Combine multiple methods |
| `EVOCORE_SYNTHESIS_NEAREST` | Nearest neighbor in parameter space |

## Performance

### Benchmarks

```bash
make benchmark
```

### Memory

- Genome storage: 8-64 bytes per genome
- Arena allocator: Reduces allocation overhead by 30%
- Memory pools: Reusable fixed-size blocks

## Development Status

| Phase | Status | Tests |
|-------|--------|-------|
| Phase 1: Foundation | ✅ Complete | 15 tests |
| Phase 2: Domain System | ✅ Complete | 10 tests |
| Phase 3: Meta-Evolution | ✅ Complete | 19 tests |
| Phase 4: GPU Acceleration | ✅ Complete | 18 tests |
| Phase 5: Organic Learning | ✅ Complete | 67 tests |
| ├─ Weighted Statistics | ✅ Complete | 15 tests |
| ├─ Context Learning | ✅ Complete | 10 tests |
| ├─ Temporal Learning | ✅ Complete | 11 tests |
| ├─ Exploration Control | ✅ Complete | 16 tests |
| └─ Parameter Synthesis | ✅ Complete | 15 tests |
| Phase 6: Memory Optimization | ✅ Complete | - |
| Phase 7: Persistence | ✅ Complete | 8 tests |
| Phase 8: Testing | ✅ Complete | 7 tests (integration) |
| Phase 9: Monitoring | ✅ Complete | 11 tests |
| **Total** | **✅ Complete** | **169 tests passing** |

## License

MIT License - Copyright (c) 2026 AION América S.A.S.
