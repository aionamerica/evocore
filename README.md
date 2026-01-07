# Evocore - Meta-Evolutionary Framework

**Version:** 0.4.0
**Language:** C99

A domain-agnostic meta-evolutionary framework for evolutionary computation research and optimization.

## Overview

Evocore provides a flexible foundation for building evolutionary algorithms that can evolve not just solutions, but the evolutionary parameters themselves. The framework is designed to be:

- **Domain-agnostic** - Works with any problem through callback-based fitness evaluation
- **Pure C** - No external dependencies, maximum portability
- **Extensible** - Clean separation between framework and domain logic
- **High Performance** - Multi-threaded CPU evaluation and GPU acceleration via CUDA
- **Production Ready** - Checkpointing, monitoring, persistence, and deployment automation

## Features

- **Meta-Evolution**: Evolves evolutionary parameters (mutation rates, selection pressure, population size)
- **GPU Acceleration**: CUDA kernels for parallel fitness evaluation
- **Persistence**: JSON serialization for genomes, populations, and meta-populations
- **Checkpointing**: Auto-save with compression and resume capability
- **Monitoring**: Real-time statistics, convergence detection, stagnation alerts
- **Memory Optimization**: Arena allocators and memory pools for efficient bulk allocation
- **Production Deployment**: Docker support, installation scripts, health checks

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
│   ├── meta.h               # Meta-evolution layer
│   ├── gpu.h                # GPU acceleration
│   ├── persist.h            # Serialization & checkpointing
│   ├── stats.h              # Statistics & monitoring
│   ├── memory.h             # Memory management API
│   ├── arena.h              # Arena allocator
│   ├── config.h             # INI configuration
│   └── error.h              # Error handling
├── src/                     # Implementation
│   ├── cuda/                # CUDA kernels
│   ├── arena.c              # Arena allocator
│   ├── memory.c             # Memory tracking
│   └── ...
├── examples/                # Example programs
│   ├── sphere_function.c    # Sphere function optimization
│   ├── trading_domain.c     # Trading strategy domain
│   ├── tsp_domain.c         # Traveling salesman domain
│   ├── meta_demo.c          # Meta-evolution demo
│   ├── gpu_benchmark.c      # GPU performance benchmark
│   ├── checkpoint_demo.c    # Checkpoint/save-resume demo
│   └── monitoring_demo.c    # Statistics monitoring demo
├── tests/                   # Test programs
├── deployment/              # Production deployment
│   ├── scripts/            # install, uninstall, health_check
│   ├── docker/             # Dockerfile, docker-compose.yml
│   └── configs/            # production.conf, logging.conf
├── docs/                    # Documentation
│   └── DEPLOYMENT.md        # Deployment guide
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
./build/sphere_function
./build/trading_domain
./build/meta_demo
./build/gpu_benchmark
./build/checkpoint_demo
./build/monitoring_demo
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

See `docs/DEPLOYMENT.md` for complete deployment documentation.

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

| Phase | Status |
|-------|--------|
| Phase 1: Foundation | ✅ Complete |
| Phase 2: Domain System | ✅ Complete |
| Phase 3: Meta-Evolution | ✅ Complete |
| Phase 4: GPU Acceleration | ✅ Complete |
| Phase 6: Memory Optimization | ✅ Complete |
| Phase 7: Persistence | ✅ Complete |
| Phase 8: Testing | ✅ Complete (102 tests passing) |
| Phase 9: Monitoring | ✅ Complete |
| Phase 10: Production | ✅ Complete |

## License

MIT License - Copyright (c) 2026 AION América S.A.S.
