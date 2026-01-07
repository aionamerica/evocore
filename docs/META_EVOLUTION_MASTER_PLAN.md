# Meta-Evolutionary Framework: 10-Phase Implementation Plan

**Document Version:** 2.0
**Date:** January 6, 2026
**Status:** Master Plan (C-Only Architecture)

---

## Executive Summary

This document outlines a comprehensive 10-phase plan for transitioning the AION-C trading optimizer from a fixed-parameter evolutionary system to a **Meta-Evolutionary Framework**. The new framework will be **domain-agnostic**, enabling evolution not just of trading strategies, but of the evolutionary algorithm itself.

### Vision

```
Level 3: META-EVOLUTION (Evolves: Mutation rates, selection pressure, population dynamics)
Level 2: STRATEGY EVOLUTION (Evolves: Trading parameters, strategy combinations)
Level 1: TRADING NODES (Executes: Backtests, generates signals)
```

### Key Principles

1. **Domain Agnosticism** - The core framework (evocore) must have NO trading-specific logic
2. **Modularity** - Clean separation between framework and domain layers
3. **C Language Only** - Pure C implementation for maximum performance and portability
4. **Extensibility** - Callback-based architecture for any domain problem
5. **Production Ready** - Comprehensive testing, benchmarking, and deployment strategy

---

## 1. Project Structure

### 1.1 Repository Layout

```
/mnt/storage/Projects/Aion/
├── evocore/                     # NEW: Domain-agnostic meta-evolution framework (C only)
│   ├── include/evocore/         # Public headers
│   ├── src/                   # Implementation
│   ├── src/cuda/              # GPU kernels
│   ├── examples/              # Non-trading examples
│   ├── tests/                 # Framework tests
│   └── Makefile
│
├── aion-optimizer/            # Existing: Trading optimizer (uses evocore)
│   ├── include/aion/          # Aion-specific headers
│   ├── src/                   # Trading implementation
│   ├── src/strategies/        # Strategy implementations
│   ├── src/cuda/              # Trading-specific GPU kernels
│   ├── src/ui/                # Raylib GUI
│   └── Makefile
│
└── aion-trader/               # NEW: Live trading execution
    ├── include/trader/        # Trader headers
    ├── src/                   # Execution engine
    ├── src/brokers/           # Broker integrations
    └── Makefile
```

### 1.2 Component Relationships

| Component | Language | Purpose |
|-----------|----------|---------|
| `evocore` | C | Core meta-evolution framework (domain-agnostic) |
| `aion-optimizer` | C | Trading strategy optimizer (uses evocore) |
| `aion-trader` | C | Live trading execution (uses optimized strategies) |

---

## 2. Technology Stack (C-Only)

### 2.1 Core Framework (evocore)

| Component | Technology | Justification |
|-----------|------------|---------------|
| **Language** | C99 | Proven, portable, existing codebase |
| **Build System** | Make + CMake | Existing build system |
| **GPU** | CUDA 11+ | Existing GPU kernels |
| **Configuration** | Custom INI parser | Simple, no dependencies |
| **Logging** | Custom logging | Minimal dependency |
| **Testing** | Custom test framework + assert | No external dependencies |
| **Memory** | Custom arena allocator | Performance, fragmentation control |

### 2.2 Dependencies

```
evocore dependencies:
- CUDA toolkit (optional, for GPU acceleration)
- PostgreSQL libpq (for persistence)
- Standard C library ONLY

aion-optimizer dependencies:
- evocore (linked as library)
- CUDA toolkit
- PostgreSQL libpq
- Raylib (for GUI)
```

---

## 3. Framework Architecture (C)

### 3.1 Three-Level Hierarchy

```
┌─────────────────────────────────────────────────────────────────┐
│                     EVOCORE META-EVOLUTION                        │
│  (Evolves: mutation_rates, selection_pressure, pop_dynamics)    │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    EVOCORE EVOLUTION ENGINE                        │
│  (Evolves: domain_parameters, strategies, topologies)          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│              DOMAIN CALLBACKS (AION or Other)                   │
│  (Evaluates: trading_nodes, tsp_solutions, etc.)                │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Core Abstraction Layer (C)

```c
/* include/evocore/genome.h */

#ifndef EVOCORE_GENOME_H
#define EVOCORE_GENOME_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Domain-agnostic genome representation
 *
 * The genome is an opaque byte array that the domain interprets.
 * evocore handles storage, mutation, and crossover without knowing
 * the internal structure.
 */
typedef struct {
    void *data;              /* Raw genome data */
    size_t size;             /* Size in bytes */
    size_t capacity;         /* Allocated capacity */
} evocore_genome_t;

/**
 * Genome operations vtable
 *
 * Domains provide these functions to tell evocore how to handle
 * their specific genome type.
 */
typedef struct {
    /* Create a random genome */
    void (*random_init)(evocore_genome_t *genome, void *context);

    /* Mutate a genome in-place */
    void (*mutate)(evocore_genome_t *genome, double rate, void *context);

    /* Crossover two genomes to produce offspring */
    void (*crossover)(const evocore_genome_t *parent1,
                     const evocore_genome_t *parent2,
                     evocore_genome_t *child1,
                     evocore_genome_t *child2,
                     void *context);

    /* Clone a genome */
    void (*clone)(const evocore_genome_t *src, evocore_genome_t *dst);

    /* Free genome resources */
    void (*destroy)(evocore_genome_t *genome);

    /* Calculate genome diversity (0.0 to 1.0) */
    double (*diversity)(const evocore_genome_t *a, const evocore_genome_t *b);
} genome_ops_t;

#endif /* EVOCORE_GENOME_H */
```

```c
/* include/evocore/fitness.h */

#ifndef EVOCORE_FITNESS_H
#define EVOCORE_FITNESS_H

#include "evocore/genome.h"

/**
 * Fitness evaluation callback
 *
 * Domains implement this to evaluate how good a genome is.
 * Higher fitness = better. Returns negative on error.
 */
typedef double (*fitness_func_t)(const evocore_genome_t *genome, void *context);

/**
 * Batch fitness evaluation (optional, for GPU acceleration)
 *
 * If provided, evocore will use this for parallel evaluation.
 * Returns number of successfully evaluated genomes.
 */
typedef size_t (*batch_fitness_func_t)(
    const evocore_genome_t *genomes,
    double *fitnesses,
    size_t count,
    void *context
);

#endif /* EVOCORE_FITNESS_H */
```

```c
/* include/evocore/meta.h */

#ifndef EVOCORE_META_H
#define EVOCORE_META_H

/**
 * Meta-evolution parameters
 *
 * These parameters control HOW evolution happens.
 * The meta-evolution layer evolves these values.
 */
typedef struct {
    /* Mutation rates (adaptive) */
    double optimization_mutation_rate;     /* Default: 0.05 */
    double variance_mutation_rate;         /* Default: 0.15 */
    double experimentation_rate;           /* Default: 0.05 */

    /* Selection pressures */
    double elite_protection_ratio;         /* Default: 0.10 */
    double culling_ratio;                  /* Default: 0.25 */
    double fitness_threshold_for_breeding; /* Default: 0.0 */

    /* Population dynamics */
    int target_population_size;
    int min_population_size;
    int max_population_size;

    /* Learning parameters */
    double learning_rate;                  /* 5D bucket updates */
    double exploration_factor;             /* Random vs learned */
    double confidence_threshold;

    /* Breeding ratios (adaptive based on performance) */
    double profitable_optimization_ratio;  /* Default: 0.80 */
    double profitable_random_ratio;        /* Default: 0.05 */
    double losing_optimization_ratio;      /* Default: 0.50 */
    double losing_random_ratio;            /* Default: 0.25 */

    /* Meta-meta parameters */
    double meta_mutation_rate;             /* How fast meta-params evolve */
    double meta_learning_rate;             /* Meta-level learning speed */
    double meta_convergence_threshold;     /* Meta convergence detection */
} evocore_meta_params_t;

/**
 * Meta-individual: A set of meta-parameters being evolved
 */
typedef struct {
    evocore_meta_params_t params;
    double meta_fitness;
    int generation;
    double *fitness_history;
    size_t history_size;
    size_t history_capacity;
} evocore_meta_individual_t;

#endif /* EVOCORE_META_H */
```

### 3.3 Domain Registration (C)

```c
/* include/evocore/domain.h */

#ifndef EVOCORE_DOMAIN_H
#define EVOCORE_DOMAIN_H

#include "evocore/genome.h"
#include "evocore/fitness.h"
#include "evocore/meta.h"

/**
 * Domain descriptor
 *
 * Domains register with evocore to provide problem-specific logic.
 * This replaces the plugin system with a simpler callback approach.
 */
typedef struct {
    const char *name;              /* Domain name (e.g., "trading", "tsp") */
    const char *version;           /* Version string */

    size_t genome_size;            /* Expected genome size */

    genome_ops_t genome_ops;       /* Genome operations */
    fitness_func_t fitness_func;   /* Fitness evaluation */
    batch_fitness_func_t batch_fitness;  /* Optional batch/GPU evaluation */

    void *user_context;            /* User-provided context pointer */

    /* Optional: serialization */
    int (*serialize_genome)(const evocore_genome_t *genome, char *buffer, size_t size);
    int (*deserialize_genome)(const char *buffer, evocore_genome_t *genome);

    /* Optional: statistics */
    void (*get_statistics)(const evocore_genome_t *genome, char *buffer, size_t size);
} evocore_domain_t;

/**
 * Register a domain with evocore
 *
 * Returns 0 on success, negative on error.
 */
int evocore_register_domain(evocore_domain_t *domain);

/**
 * Unregister a domain
 */
void evocore_unregister_domain(const char *name);

/**
 * Get domain by name
 */
evocore_domain_t* evocore_get_domain(const char *name);

#endif /* EVOCORE_DOMAIN_H */
```

---

## 4. The 10 Phases

### Phase 1: Foundation - Core Framework Skeleton
**Primary Objective:** Establish the domain-agnostic framework structure

**Technical Components:**
- Core header files (`genome.h`, `fitness.h`, `population.h`)
- Basic population management
- Configuration system (INI-based)
- Logging infrastructure
- Error handling with `evocore_error_t`

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   ├── evocore.h                 # Main header
│   ├── genome.h                # Genome abstraction
│   ├── fitness.h               # Fitness callbacks
│   ├── population.h            # Population management
│   ├── config.h                # Configuration
│   └── error.h                 # Error codes
├── src/
│   ├── genome.c
│   ├── population.c
│   ├── config.c
│   ├── error.c
│   └── log.c
├── examples/
│   └── sphere_function.c       # Simple optimization example
└── Makefile
```

**Dependencies:** None

**Success Criteria:**
- [ ] Compiles cleanly with `make`
- [ ] Can run sphere function optimization
- [ ] Configuration loaded from INI file
- [ ] Basic logging to stdout/file
- [ ] Population survives 100 generations

**Complexity:** Low

---

### Phase 2: Domain Registration System
**Primary Objective:** Implement callback-based domain system

**Technical Components:**
- Domain descriptor structure
- Domain registration/unregistration
- Domain lookup by name
- Context management
- Domain validation

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   └── domain.h                # Domain registration
├── src/
│   └── domain.c
└── examples/
    ├── trading_domain.c        # Stub trading domain
    └── tsp_domain.c            # TSP domain example
```

**Dependencies:** Phase 1

**Success Criteria:**
- [ ] Can register multiple domains
- [ ] Domain callbacks are invoked correctly
- [ ] Each domain has isolated context
- [ ] At least 2 example domains working

**Complexity:** Medium

---

### Phase 3: Meta-Evolution Layer
**Primary Objective:** Implement Level 3 meta-evolution

**Technical Components:**
- `evocore_meta_params_t` structure
- Meta-fitness evaluation
- Meta-population management
- Adaptive parameter adjustment
- Meta-convergence detection

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   └── meta.h                  # Meta-evolution
├── src/
│   ├── meta.c                 # Meta-evolution logic
│   ├── meta_fitness.c         # Meta-parameter evaluation
│   └── adaptive.c             # Online adaptation
└── examples/
    └── meta_demo.c            # Demonstrate meta-evolution
```

**Dependencies:** Phase 1, Phase 2

**Success Criteria:**
- [ ] Meta-population evolves distinct parameter sets
- [ ] Meta-parameters adapt based on performance
- [ ] Demonstrates adaptive mutation rates
- [ ] Meta-convergence detection working

**Complexity:** High

---

### Phase 4: GPU Acceleration Framework
**Primary Objective:** Enable GPU fitness evaluations

**Technical Components:**
- Generic GPU kernel interface
- Batch evaluation system
- CPU-GPU data transfer optimization
- Work queue management
- Multi-GPU support

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   └── gpu.h                  # GPU interface
├── src/
│   ├── gpu.c                  # GPU context management
│   └── batch.c                # Batch evaluation
├── src/cuda/
│   ├── evaluate.cu            # Generic evaluation kernel
│   └── reduce.cu              # Reduction kernels
└── examples/
    └── gpu_benchmark.c        # Performance benchmark
```

**Dependencies:** Phase 1

**Success Criteria:**
- [ ] Evaluates 1000+ genomes in parallel on GPU
- [ ] CPU-GPU transfer overhead < 10%
- [ ] Graceful fallback to CPU
- [ ] Benchmark shows speedup vs CPU

**Complexity:** High

---

### Phase 5: Integration with AION-Optimizer
**Primary Objective:** Replace 5D learning with evocore meta-evolution

**Technical Components:**
- AION domain implementation for evocore
- Trading genome (TradingDNA) adaptation
- Trading fitness function (Sharpe, return, drawdown)
- Strategy registry integration
- Remove old 5D learning code

**Files/Modules to Modify/Create:**
```
aion-optimizer/
├── include/aion/
│   ├── evocore_domain.h         # AION as evocore domain
│   └── trading_genome.h       # Trading genome wrapper
├── src/
│   ├── evocore_domain.c         # Domain implementation
│   ├── trading_fitness.c      # Fitness calculation
│   └── evolution.c            # Replace with evocore calls
├── src/strategies/
│   └── (all strategies adapted to evocore genome format)
└── Makefile
│   └── (link against evocore)
```

**Dependencies:** Phase 2, Phase 4

**Success Criteria:**
- [ ] All 6 strategies work with evocore
- [ ] Meta-evolution replaces 5D learning
- [ ] GPU acceleration for backtests
- [ ] Performance matches or exceeds current system

**Complexity:** High

---

### Phase 6: Memory and Performance Optimization
**Primary Objective:** Optimize memory usage and performance

**Technical Components:**
- Arena allocator for genomes
- Memory pool for meta-individuals
- SIMD operations where applicable
- Cache-friendly data structures
- Performance profiling

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   └── memory.h               # Memory management
├── src/
│   ├── arena.c                # Arena allocator
│   └── pool.c                 # Object pools
└── tests/
    └── performance_bench.c    # Performance benchmarks
```

**Dependencies:** Phase 1, Phase 4, Phase 5

**Success Criteria:**
- [ ] Memory usage reduced by 30%+
- [ ] No memory leaks (valgrind clean)
- [ ] Performance improves by 20%+
- [ ] Benchmark suite established

**Complexity:** Medium

---

### Phase 7: Persistence and State Management
**Primary Objective:** Implement robust state persistence

**Technical Components:**
- Database schema for meta-evolution
- State checkpoint/resume
- Migration from current database
- Real-time state synchronization

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   └── persist.h              # Persistence interface
├── src/
│   ├── persist.c              # Persistence implementation
│   ├── checkpoint.c           # Checkpoint system
│   └── migration.c            # Migrate old data
└── sql/
    └── 001_evocore_schema.sql   # Database schema
```

**Dependencies:** Phase 3, Phase 5

**Success Criteria:**
- [ ] Can save/restore full evolution state
- [ ] Migration from old database working
- [ ] Resume from checkpoint reproduces results
- [ ] Database handles 1M+ genomes

**Complexity:** Medium

---

### Phase 8: Testing and Benchmarking
**Primary Objective:** Comprehensive test coverage

**Technical Components:**
- Unit tests for all modules
- Integration tests for full cycles
- Regression test suite
- Performance benchmarks
- Memory leak detection

**Files/Modules to Create:**
```
evocore/
├── tests/
│   ├── test_genome.c
│   ├── test_population.c
│   ├── test_meta.c
│   ├── test_integration.c
│   └── test_regression.c
└── benches/
│   ├── bench_evolution.c
│   └── bench_gpu.c

aion-optimizer/
└── tests/
    ├── test_strategies.c
    └── test_evocore_integration.c
```

**Dependencies:** All previous phases

**Success Criteria:**
- [ ] >80% code coverage
- [ ] All tests passing
- [ ] Benchmarks establish baseline
- [ ] CI/CD pipeline running tests

**Complexity:** Medium

---

### Phase 9: Monitoring and Visualization
**Primary Objective:** Real-time monitoring

**Technical Components:**
- Metrics collection system
- Statistics generation
- Real-time parameter display
- Alert system for stagnation
- GUI integration

**Files/Modules to Create:**
```
evocore/
├── include/evocore/
│   └── monitor.h              # Monitoring interface
├── src/
│   ├── monitor.c              # Metrics collection
│   └── statistics.c           # Statistics generation

aion-optimizer/
└── src/ui/
    └── tab_evocore.c            # evocore visualization tab
```

**Dependencies:** Phase 7

**Success Criteria:**
- [ ] Real-time evolution state in GUI
- [ ] Can visualize meta-parameter evolution
- [ ] Alert system detects stagnation
- [ ] Export metrics to file

**Complexity:** Medium

---

### Phase 10: Production Deployment
**Primary Objective:** Production-ready system

**Technical Components:**
- Deployment automation
- Configuration management
- Monitoring and alerting
- Documentation completion
- Performance tuning

**Files/Modules to Create:**
```
evocore/
├── deployment/
│   └── production.conf        # Production config
└── docs/
    ├── ARCHITECTURE.md
    ├── API.md
    ├── MIGRATION.md
    └── PERFORMANCE.md

aion-optimizer/
└── deployment/
    └── production.conf
```

**Dependencies:** All previous phases

**Success Criteria:**
- [ ] One-command build and deploy
- [ ] Production instance stable
- [ ] Documentation complete
- [ ] 99.9% uptime achieved

**Complexity:** High

---

## 5. Migration Strategy

### 5.1 Incremental Transition

```
┌─────────────────────────────────────────────────────────────────┐
│                    CURRENT STATE (Phase 0)                       │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │           aion-optimizer (with 5D learning)             │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐                 │    │
│  │  │Strategies│ │ 5D Learn│ │   GPU   │                 │    │
│  │  └─────────┘  └─────────┘  └─────────┘                 │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    INTERMEDIATE (Phase 5-6)                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                     evocore (framework)                   │    │
│  │  ┌─────────┐  ┌─────────┐  ┌─────────┐                 │    │
│  │  │ Meta-Evo│ │ Evolution│ │   GPU   │                 │    │
│  │  └─────────┘  └─────────┘  └─────────┘                 │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                  │
│                              ▼                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │         aion-optimizer (trading domain)                 │    │
│  │  ┌─────────┐  ┌─────────┐                               │    │
│  │  │Strategies│ │Fitness  │                               │    │
│  │  └─────────┘  └─────────┘                               │    │
│  └─────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                       FINAL (Phase 10)                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                     evocore (framework)                   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                  │
│        ┌─────────────────────┴─────────────────────┐           │
│        ▼                                           ▼           │
│  ┌──────────────────┐                    ┌──────────────────┐  │
│  │ aion-optimizer   │                    │   aion-trader    │  │
│  │ (optimization)   │────────────────────▶│ (live trading)  │  │
│  └──────────────────┘  optimized params   └──────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

### 5.2 Data Migration

| Stage | Action | Target |
|-------|--------|--------|
| 1 | Add evocore tables | Phase 5 |
| 2 | Dual-write (old + new) | Phase 6 |
| 3 | Migrate historical data | Phase 7 |
| 4 | Deprecate old 5D tables | Phase 10 |

---

## 6. Meta-Parameter Structure (C)

### 6.1 Complete MetaGenome Definition

```c
/* include/evocore/meta.h */

/**
 * Meta-evolution parameters
 *
 * These parameters control HOW evolution happens.
 * The meta-evolution layer evolves these values.
 */
typedef struct {
    /* ========================================================================
     * MUTATION RATES (Adaptive)
     * ======================================================================== */

    /** How aggressively to mutate parameters (0.01 - 0.50) */
    double optimization_mutation_rate;

    /** How much to vary existing parameters (0.05 - 0.50) */
    double variance_mutation_rate;

    /** Rate of completely random exploration (0.01 - 0.30) */
    double experimentation_rate;

    /* ========================================================================
     * SELECTION PRESSURE
     * ======================================================================== */

    /** Ratio of elite individuals protected from culling (0.05 - 0.30) */
    double elite_protection_ratio;

    /** Ratio of worst individuals to cull (0.10 - 0.50) */
    double culling_ratio;

    /** Minimum fitness required for breeding (0.0 - 1.0) */
    double fitness_threshold_for_breeding;

    /* ========================================================================
     * POPULATION DYNAMICS
     * ======================================================================== */

    /** Target population size (50 - 10000) */
    int target_population_size;

    /** Minimum population (10 - target) */
    int min_population_size;

    /** Maximum population (target - 20000) */
    int max_population_size;

    /* ========================================================================
     * LEARNING PARAMETERS
     * ======================================================================== */

    /** Rate at which learning buckets update (0.01 - 1.0) */
    double learning_rate;

    /** Balance between learned values and exploration (0.0 - 1.0) */
    double exploration_factor;

    /** Minimum confidence before trusting learned values (0.0 - 1.0) */
    double confidence_threshold;

    /* ========================================================================
     * BREEDING RATIOS (Performance-Dependent)
     * ======================================================================== */

    /** For profitable nodes: ratio of optimization mutations (0.5 - 1.0) */
    double profitable_optimization_ratio;

    /** For profitable nodes: ratio of random exploration (0.0 - 0.2) */
    double profitable_random_ratio;

    /** For losing nodes: ratio of optimization mutations (0.2 - 0.8) */
    double losing_optimization_ratio;

    /** For losing nodes: ratio of random exploration (0.1 - 0.5) */
    double losing_random_ratio;

    /* ========================================================================
     * META-META PARAMETERS
     * ======================================================================== */

    /** How fast meta-parameters themselves evolve (0.01 - 0.20) */
    double meta_mutation_rate;

    /** Meta-level learning rate (0.01 - 0.50) */
    double meta_learning_rate;

    /** Meta-level convergence threshold (0.001 - 0.1) */
    double meta_convergence_threshold;
} evocore_meta_params_t;

/**
 * Default meta-parameters
 *
 * These are well-tested starting values that work for most problems.
 */
static const evocore_meta_params_t EVOCORE_META_DEFAULTS = {
    /* Mutation rates */
    .optimization_mutation_rate = 0.05,
    .variance_mutation_rate = 0.15,
    .experimentation_rate = 0.05,

    /* Selection pressure */
    .elite_protection_ratio = 0.10,
    .culling_ratio = 0.25,
    .fitness_threshold_for_breeding = 0.0,

    /* Population dynamics */
    .target_population_size = 500,
    .min_population_size = 50,
    .max_population_size = 2000,

    /* Learning parameters */
    .learning_rate = 0.1,
    .exploration_factor = 0.3,
    .confidence_threshold = 0.7,

    /* Breeding ratios */
    .profitable_optimization_ratio = 0.80,
    .profitable_random_ratio = 0.05,
    .losing_optimization_ratio = 0.50,
    .losing_random_ratio = 0.25,

    /* Meta-meta parameters */
    .meta_mutation_rate = 0.05,
    .meta_learning_rate = 0.1,
    .meta_convergence_threshold = 0.01
};

/**
 * Initialize meta-parameters with defaults
 */
void evocore_meta_params_init(evocore_meta_params_t *params);

/**
 * Validate meta-parameters
 *
 * Returns 0 if valid, negative if any parameter is out of range.
 */
int evocore_meta_params_validate(const evocore_meta_params_t *params);

/**
 * Mutate meta-parameters
 *
 * Applies random mutations to the meta-parameters based on
 * the meta_mutation_rate.
 */
void evocore_meta_params_mutate(evocore_meta_params_t *params,
                               double rate,
                               unsigned int *seed);

#endif /* EVOCORE_META_H */
```

---

## 7. Timeline and Resource Estimates

### 7.1 Phase Timeline

| Phase | Duration | Complexity | Critical Path |
|-------|----------|------------|---------------|
| 1. Foundation | 2 weeks | Low | Yes |
| 2. Domain System | 2 weeks | Medium | Yes |
| 3. Meta-Evolution | 3 weeks | High | Yes |
| 4. GPU Framework | 3 weeks | High | Yes |
| 5. AION Integration | 4 weeks | High | Yes |
| 6. Optimization | 2 weeks | Medium | No |
| 7. Persistence | 2 weeks | Medium | No |
| 8. Testing | 2 weeks | Medium | No |
| 9. Monitoring | 2 weeks | Medium | No |
| 10. Production | 2 weeks | High | No |

**Total Duration:** ~24 weeks (6 months) on critical path

---

## 8. Risk Assessment and Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Performance regression | High | Medium | Benchmark suite from Phase 1 |
| Meta-evolution instability | High | Low | Extensive testing in Phase 3 |
| Data migration issues | High | Low | Dual-write period in Phase 6 |
| Memory leaks | Medium | Medium | Valgrind testing in Phase 6 |
| GPU compatibility | Medium | Low | CPU fallback in Phase 4 |

---

## 9. Success Metrics

### 9.1 Technical Metrics

| Metric | Target | Current |
|--------|--------|---------|
| Genomes/second (GPU) | >100,000 | ~10,000 |
| Meta-parameter convergence | <100 generations | N/A |
| Memory per genome | <1KB | ~5KB |
| Build time | <30 seconds | ~45 seconds |

### 9.2 Trading Metrics

| Metric | Target | Current |
|--------|--------|---------|
| Sharpe ratio | >2.0 | ~1.5 |
| Max drawdown | <20% | ~30% |
| Win rate | >55% | ~52% |
| Optimization time | <1 hour | ~4 hours |

---

## 10. Next Steps

1. **Immediate (Week 1-2):** Set up `evocore` directory structure
2. **Short-term (Month 1):** Complete Phases 1-3
3. **Medium-term (Month 2-3):** Complete Phases 4-5
4. **Long-term (Month 4-6):** Complete Phases 6-10

### Kickoff Checklist

- [ ] Create `/mnt/storage/Projects/Aion/evocore` directory (framework code)
- [ ] Set up basic Makefile
- [ ] Create include/evocore/ directory structure
- [ ] Set up logging infrastructure
- [ ] Create Phase 1 detailed specification

---

**End of Master Plan**

This document serves as the authoritative guide for transitioning AION-C to the evocore meta-evolutionary framework. All architectural decisions should reference this document as the source of truth.
