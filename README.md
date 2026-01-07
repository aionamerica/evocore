# evocore - Meta-Evolutionary Framework

**Version:** 0.1.0
**Language:** C99
**Status:** Phase 1 (Foundation)

A domain-agnostic meta-evolutionary framework for evolutionary computation research and optimization.

## Overview

evocore provides a flexible foundation for building evolutionary algorithms that can evolve not just solutions, but the evolutionary parameters themselves. The framework is designed to be:

- **Domain-agnostic** - Works with any problem through callback-based fitness evaluation
- **Pure C** - No external dependencies, maximum portability
- **Extensible** - Clean separation between framework and domain logic
- **Efficient** - Optimized memory management and data structures

## Project Structure

```
evocore/
├── include/evocore/          # Public headers
│   ├── evocore.h            # Main umbrella header
│   ├── genome.h           # Genome abstraction
│   ├── fitness.h          # Fitness callback type
│   ├── population.h       # Population management
│   ├── config.h           # INI configuration
│   ├── error.h            # Error handling
│   └── log.h              # Logging
├── src/                   # Implementation
├── examples/              # Example programs
│   └── sphere_function.c  # Sphere function optimization
├── tests/                 # Test programs
├── docs/                  # Planning documents
└── Makefile
```

## Building

### Prerequisites

- C99-compatible compiler (GCC, Clang)
- GNU Make
- Math library (`-lm`)

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
make debug      # Debug build with symbols
make valgrind   # Run with memory leak detection
make install    # Install to /usr/local
```

## Usage Example

```c
#include "evocore.h"

/* Fitness function */
double my_fitness(const evocore_genome_t *genome, void *context) {
    /* Extract and evaluate solution */
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

## Running the Example

The sphere function example demonstrates basic optimization:

```bash
# Run with default config
./build/sphere_function examples/sphere_config.ini

# Run with custom random seed
./build/sphere_function examples/sphere_config.ini 12345
```

The example minimizes the sphere function f(x) = sum(x_i^2) over 10 dimensions.

## Configuration

evocore uses INI files for configuration:

```ini
[evolution]
population_size = 100
max_generations = 100

[selection]
tournament_size = 3
elite_count = 5

[mutation]
rate = 0.1

[logging]
level = INFO
file = evolution.log
```

## API Documentation

### Core Types

- `evocore_genome_t` - Opaque byte array encoding a solution
- `evocore_individual_t` - Genome + fitness pair
- `evocore_population_t` - Collection of individuals
- `evocore_fitness_func_t` - Fitness evaluation callback

### Key Functions

| Function | Purpose |
|----------|---------|
| `evocore_genome_init()` | Create a new genome |
| `evocore_population_init()` | Create a population |
| `evocore_population_add()` | Add individual to population |
| `evocore_population_evaluate()` | Evaluate all individuals |
| `evocore_population_tournament_select()` | Tournament selection |
| `evocore_genome_crossover()` | Uniform crossover |
| `evocore_genome_mutate()` | Random mutation |

## Development Status

### Phase 1: Foundation (Current)

- [x] Core data structures
- [x] Configuration system
- [x] Logging infrastructure
- [x] Basic population operations
- [x] Sphere function example

### Planned Phases

- **Phase 2:** Domain registration system
- **Phase 3:** Meta-evolution layer
- **Phase 4:** GPU acceleration
- **Phase 5:** AION trading integration

## Documentation

See the `docs/` directory for detailed planning documents:

- `META_EVOLUTION_MASTER_PLAN.md` - 10-phase implementation roadmap
- `PHASE_1_DETAILED_PLAN.md` - Detailed Phase 1 specification

## License

Proprietary - All rights reserved

## Contributing

This is an internal research project. For questions or issues, contact the maintainers.
