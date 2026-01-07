# Phase 1: Foundation - Core Framework Skeleton
## Detailed Implementation Plan

**Document Version:** 1.0
**Date:** January 6, 2026
**Status:** Implementation Ready
**Duration:** 2 weeks

---

## Table of Contents

1. [Overview](#1-overview)
2. [Objectives](#2-objectives)
3. [Directory Structure](#3-directory-structure)
4. [File Specifications](#4-file-specifications)
5. [Implementation Order](#5-implementation-order)
6. [Testing Strategy](#6-testing-strategy)
7. [Success Criteria](#7-success-criteria)
8. [Dependencies](#8-dependencies)
9. [Risks and Mitigations](#9-risks-and-mitigations)

---

## 1. Overview

Phase 1 establishes the foundational architecture of the **evocore** meta-evolutionary framework. This phase focuses on building the core data structures, abstractions, and infrastructure that all subsequent phases will depend upon.

### 1.1 Scope

The scope of Phase 1 is intentionally limited to the minimal viable framework:
- Core data structures (genome, population)
- Configuration file handling
- Logging infrastructure
- Error handling
- A simple working example

### 1.2 Out of Scope

The following are explicitly **out of scope** for Phase 1:
- Meta-evolution (Phase 3)
- GPU acceleration (Phase 4)
- Domain registration system (Phase 2)
- Database persistence (Phase 7)
- Trading-specific logic (Phase 5)

---

## 2. Objectives

### 2.1 Primary Objective

Establish a domain-agnostic framework structure that can:
1. Represent and manipulate evolutionary genomes
2. Manage populations of individuals
3. Load configuration from INI files
4. Log events to console and file
5. Handle errors consistently

### 2.2 Secondary Objectives

- Create a working example (sphere function optimization)
- Establish coding standards for the project
- Set up build system (Makefile)
- Create initial test infrastructure

---

## 3. Directory Structure

```
/mnt/storage/Projects/Aion/evocore/
├── include/evocore/              # Public headers
│   ├── evocore.h                 # Main umbrella header
│   ├── genome.h                # Genome abstraction
│   ├── fitness.h               # Fitness callback type
│   ├── population.h            # Population management
│   ├── config.h                # Configuration loading
│   ├── error.h                 # Error codes and handling
│   └── log.h                   # Logging interface
├── src/                        # Implementation
│   ├── genome.c
│   ├── population.c
│   ├── config.c
│   ├── error.c
│   ├── log.c
│   └── internal.h              # Internal shared header
├── examples/                   # Example programs
│   ├── sphere_function.c       # Simple optimization demo
│   └── sphere_config.ini       # Config for demo
├── tests/                      # Tests
│   ├── test_genome.c
│   ├── test_population.c
│   └── test_config.c
├── Makefile                    # Build configuration
├── README.md                   # Basic documentation
└── PHASE_1_DETAILED_PLAN.md    # This document
```

---

## 4. File Specifications

### 4.1 `include/evocore/evocore.h`

**Purpose:** Main umbrella header that includes all public evocore headers.

```c
#ifndef EVOCORE_H
#define EVOCORE_H

/**
 * evocore - Meta-Evolutionary Framework
 *
 * A domain-agnostic framework for evolutionary and meta-evolutionary
 * optimization.
 */

/* Version information */
#define EVOCORE_VERSION_MAJOR 0
#define EVOCORE_VERSION_MINOR 1
#define EVOCORE_VERSION_PATCH 0
#define EVOCORE_VERSION_STRING "0.1.0"

/* Core abstractions */
#include "evocore/genome.h"
#include "evocore/fitness.h"
#include "evocore/population.h"

/* Infrastructure */
#include "evocore/config.h"
#include "evocore/error.h"
#include "evocore/log.h"

#endif /* EVOCORE_H */
```

---

### 4.2 `include/evocore/error.h`

**Purpose:** Error code definitions and error handling utilities.

```c
#ifndef EVOCORE_ERROR_H
#define EVOCORE_ERROR_H

#include <stddef.h>

/**
 * Error codes returned by evocore functions
 *
 * Positive values indicate success (with info).
 * Zero indicates success with no special info.
 * Negative values indicate errors.
 */
typedef enum {
    /* Success codes */
    EVOCORE_OK = 0,
    EVOCORE_SUCCESS_CONVERGED = 1,    /* Optimization converged */
    EVOCORE_SUCCESS_MAX_GEN = 2,      /* Reached max generations */

    /* General errors (negative) */
    EVOCORE_ERR_UNKNOWN = -1,
    EVOCORE_ERR_NULL_PTR = -2,        /* Null pointer argument */
    EVOCORE_ERR_OUT_OF_MEMORY = -3,   /* Memory allocation failed */
    EVOCORE_ERR_INVALID_ARG = -4,     /* Invalid argument value */

    /* Genome errors */
    EVOCORE_ERR_GENOME_EMPTY = -10,
    EVOCORE_ERR_GENOME_TOO_LARGE = -11,
    EVOCORE_ERR_GENOME_INVALID = -12,

    /* Population errors */
    EVOCORE_ERR_POP_EMPTY = -20,
    EVOCORE_ERR_POP_FULL = -21,
    EVOCORE_ERR_POP_SIZE = -22,

    /* Config errors */
    EVOCORE_ERR_CONFIG_NOT_FOUND = -30,
    EVOCORE_ERR_CONFIG_PARSE = -31,
    EVOCORE_ERR_CONFIG_INVALID = -32,

    /* File I/O errors */
    EVOCORE_ERR_FILE_NOT_FOUND = -40,
    EVOCORE_ERR_FILE_READ = -41,
    EVOCORE_ERR_FILE_WRITE = -42
} evocore_error_t;

/**
 * Get human-readable error message
 */
const char* evocore_error_string(evocore_error_t err);

/**
 * Macro for checking and returning errors
 */
#define EVOCORE_CHECK(expr) \
    do { \
        evocore_error_t _err = (expr); \
        if (_err != EVOCORE_OK) \
            return _err; \
    } while(0)

/**
 * Macro for logging and returning errors
 */
#define EVOCORE_CHECK_LOG(expr, msg) \
    do { \
        evocore_error_t _err = (expr); \
        if (_err != EVOCORE_OK) { \
            evocore_log_error("%s: %s", msg, evocore_error_string(_err)); \
            return _err; \
        } \
    } while(0)

#endif /* EVOCORE_ERROR_H */
```

---

### 4.3 `include/evocore/genome.h`

**Purpose:** Genome abstraction for encoding candidate solutions.

```c
#ifndef EVOCORE_GENOME_H
#define EVOCORE_GENOME_H

#include <stddef.h>
#include <stdbool.h>
#include "evocore/error.h"

/**
 * Genome structure
 *
 * A genome is an opaque byte array representing a candidate solution.
 * The framework treats genomes as opaque - only the domain knows
 * the internal structure.
 */
typedef struct {
    void *data;              /* Raw genome data */
    size_t size;             /* Current size in bytes */
    size_t capacity;         /* Allocated capacity */
    bool owns_memory;        /* Whether we allocated the memory */
} evocore_genome_t;

/**
 * Result structure for fitness + genome pair
 */
typedef struct {
    evocore_genome_t *genome;
    double fitness;
} evocore_individual_t;

/*========================================================================
 * Genome Lifecycle
 *========================================================================*/

/**
 * Create a new genome with specified capacity
 *
 * @param genome    Pointer to genome structure to initialize
 * @param capacity  Initial capacity in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_init(evocore_genome_t *genome, size_t capacity);

/**
 * Create a genome by copying existing data
 *
 * @param genome    Pointer to genome structure to initialize
 * @param data      Data to copy (must be valid)
 * @param size      Size of data in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_from_data(evocore_genome_t *genome,
                                     const void *data,
                                     size_t size);

/**
 * Create a view (wrapper) over existing data without copying
 *
 * The genome does NOT own the memory and will not free it.
 *
 * @param genome    Pointer to genome structure to initialize
 * @param data      Data to wrap (must remain valid)
 * @param size      Size of data in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_view(evocore_genome_t *genome,
                                const void *data,
                                size_t size);

/**
 * Free genome resources
 *
 * @param genome    Genome to clean up
 */
void evocore_genome_cleanup(evocore_genome_t *genome);

/**
 * Clone a genome
 *
 * @param src       Source genome to copy from
 * @param dst       Destination genome (must be uninitialized)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_clone(const evocore_genome_t *src,
                                 evocore_genome_t *dst);

/*========================================================================
 * Genome Manipulation
 *========================================================================*/

/**
 * Resize genome capacity
 *
 * Preserves existing data if growing. May reallocate.
 *
 * @param genome    Genome to resize
 * @param new_capacity  New capacity in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_resize(evocore_genome_t *genome,
                                  size_t new_capacity);

/**
 * Set genome size
 *
 * Sets the logical size without changing capacity.
 * New size must be <= capacity.
 *
 * @param genome    Genome to modify
 * @param size      New size in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_set_size(evocore_genome_t *genome, size_t size);

/**
 * Copy data into genome at specified offset
 *
 * @param genome    Destination genome
 * @param offset    Byte offset to write to
 * @param data      Source data
 * @param size      Size of data to copy
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_write(evocore_genome_t *genome,
                                  size_t offset,
                                  const void *data,
                                  size_t size);

/**
 * Read data from genome at specified offset
 *
 * @param genome    Source genome
 * @param offset    Byte offset to read from
 * @param data      Destination buffer
 * @param size      Size of data to read
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_read(const evocore_genome_t *genome,
                                size_t offset,
                                void *data,
                                size_t size);

/*========================================================================
 * Genome Utilities
 *========================================================================*/

/**
 * Calculate Hamming distance between two genomes
 *
 * For genomes of equal size, counts differing bytes.
 *
 * @param a         First genome
 * @param b         Second genome
 * @param distance  Output: calculated distance
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_distance(const evocore_genome_t *a,
                                    const evocore_genome_t *b,
                                    size_t *distance);

/**
 * Zero out genome contents
 *
 * @param genome    Genome to clear
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_zero(evocore_genome_t *genome);

/**
 * Fill genome with random bytes
 *
 * @param genome    Genome to fill
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_randomize(evocore_genome_t *genome);

#endif /* EVOCORE_GENOME_H */
```

---

### 4.4 `include/evocore/fitness.h`

**Purpose:** Fitness function callback type definition.

```c
#ifndef EVOCORE_FITNESS_H
#define EVOCORE_FITNESS_H

#include "evocore/genome.h"

/**
 * Fitness evaluation callback
 *
 * Domains implement this to evaluate how good a genome is.
 * Higher fitness = better. Can return NaN to indicate invalid.
 *
 * @param genome    Genome to evaluate
 * @param context   User-provided context pointer
 * @return Fitness value (higher is better, or NaN for invalid)
 */
typedef double (*evocore_fitness_func_t)(const evocore_genome_t *genome,
                                       void *context);

#endif /* EVOCORE_FITNESS_H */
```

---

### 4.5 `include/evocore/population.h`

**Purpose:** Population management for evolutionary algorithms.

```c
#ifndef EVOCORE_POPULATION_H
#define EVOCORE_POPULATION_H

#include <stddef.h>
#include <stdbool.h>
#include "evocore/genome.h"
#include "evocore/fitness.h"
#include "evocore/error.h"

/**
 * Population structure
 *
 * Manages a collection of individuals (genome + fitness pairs).
 */
typedef struct {
    evocore_individual_t *individuals;  /* Array of individuals */
    size_t size;                      /* Current population size */
    size_t capacity;                  /* Maximum capacity */
    size_t generation;                /* Current generation number */
    double best_fitness;              /* Best fitness seen */
    double avg_fitness;               /* Average fitness */
    double worst_fitness;             /* Worst fitness */
    size_t best_index;                /* Index of best individual */
} evocore_population_t;

/*========================================================================
 * Population Lifecycle
 *========================================================================*/

/**
 * Create a new population
 *
 * @param pop       Population to initialize
 * @param capacity  Maximum capacity
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_init(evocore_population_t *pop,
                                     size_t capacity);

/**
 * Free population resources
 *
 * Does NOT free the genomes themselves - call evocore_population_clear first
 * if you want to clean up genomes.
 *
 * @param pop       Population to clean up
 */
void evocore_population_cleanup(evocore_population_t *pop);

/**
 * Clear all individuals from population
 *
 * Frees all genome memory.
 *
 * @param pop       Population to clear
 */
void evocore_population_clear(evocore_population_t *pop);

/*========================================================================
 * Population Manipulation
 *========================================================================*/

/**
 * Add an individual to the population
 *
 * The genome is cloned - caller retains ownership of the original.
 *
 * @param pop       Population to add to
 * @param genome    Genome to add (will be cloned)
 * @param fitness   Fitness value
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_add(evocore_population_t *pop,
                                    const evocore_genome_t *genome,
                                    double fitness);

/**
 * Remove an individual at specified index
 *
 * @param pop       Population to modify
 * @param index     Index of individual to remove
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_remove(evocore_population_t *pop,
                                       size_t index);

/**
 * Resize population capacity
 *
 * @param pop       Population to resize
 * @param new_capacity  New capacity
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_resize(evocore_population_t *pop,
                                      size_t new_capacity);

/*========================================================================
 * Population Queries
 *========================================================================*/

/**
 * Get individual at index
 *
 * @param pop       Population
 * @param index     Index
 * @return Pointer to individual, or NULL if invalid index
 */
evocore_individual_t* evocore_population_get(evocore_population_t *pop,
                                         size_t index);

/**
 * Get best individual
 *
 * @param pop       Population
 * @return Pointer to best individual, or NULL if empty
 */
evocore_individual_t* evocore_population_get_best(evocore_population_t *pop);

/**
 * Calculate population statistics
 *
 * Updates best_fitness, avg_fitness, worst_fitness, best_index.
 *
 * @param pop       Population
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_update_stats(evocore_population_t *pop);

/**
 * Sort population by fitness (descending)
 *
 * @param pop       Population to sort
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_sort(evocore_population_t *pop);

/*========================================================================
 * Evolution Operations
 *========================================================================*/

/**
 * Select parent using tournament selection
 *
 * @param pop       Population to select from
 * @param tournament_size  Number of individuals in tournament
 * @param seed      Random seed pointer (will be dereferenced and updated)
 * @return Index of selected parent, or SIZE_MAX on error
 */
size_t evocore_population_tournament_select(const evocore_population_t *pop,
                                          size_t tournament_size,
                                          unsigned int *seed);

/**
 * Truncate population to keep top N individuals
 *
 * @param pop       Population to truncate
 * @param n         Number of individuals to keep
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_truncate(evocore_population_t *pop,
                                        size_t n);

/**
 * Evaluate all unevaluated individuals in population
 *
 * Uses provided fitness function to evaluate individuals with NaN fitness.
 *
 * @param pop           Population to evaluate
 * @param fitness_func  Fitness function
 * @param context       Context pointer for fitness function
 * @return Number of individuals evaluated
 */
size_t evocore_population_evaluate(evocore_population_t *pop,
                                  evocore_fitness_func_t fitness_func,
                                  void *context);

#endif /* EVOCORE_POPULATION_H */
```

---

### 4.6 `include/evocore/config.h`

**Purpose:** Configuration file loading (INI format).

```c
#ifndef EVOCORE_CONFIG_H
#define EVOCORE_CONFIG_H

#include "evocore/error.h"

/**
 * Opaque configuration structure
 */
typedef struct evocore_config_s evocore_config_t;

/**
 * Configuration value type
 */
typedef enum {
    EVOCORE_CONFIG_TYPE_STRING,
    EVOCORE_CONFIG_TYPE_INT,
    EVOCORE_CONFIG_TYPE_DOUBLE,
    EVOCORE_CONFIG_TYPE_BOOL
} evocore_config_type_t;

/**
 * Configuration entry
 */
typedef struct {
    const char *key;
    const char *value;
    evocore_config_type_t type;
} evocore_config_entry_t;

/*========================================================================
 * Config Lifecycle
 *========================================================================*/

/**
 * Load configuration from file
 *
 * @param path      Path to INI file
 * @param config    Output: configuration object
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_config_load(const char *path,
                                 evocore_config_t **config);

/**
 * Free configuration
 *
 * @param config    Configuration to free (can be NULL)
 */
void evocore_config_free(evocore_config_t *config);

/*========================================================================
 * Config Accessors
 *========================================================================*/

/**
 * Get string value
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param key       Key name
 * @param default_value  Default if not found
 * @return String value or default
 */
const char* evocore_config_get_string(const evocore_config_t *config,
                                     const char *section,
                                     const char *key,
                                     const char *default_value);

/**
 * Get integer value
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param key       Key name
 * @param default_value  Default if not found
 * @return Integer value or default
 */
int evocore_config_get_int(const evocore_config_t *config,
                          const char *section,
                          const char *key,
                          int default_value);

/**
 * Get double value
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param key       Key name
 * @param default_value  Default if not found
 * @return Double value or default
 */
double evocore_config_get_double(const evocore_config_t *config,
                                const char *section,
                                const char *key,
                                double default_value);

/**
 * Get boolean value
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param key       Key name
 * @param default_value  Default if not found
 * @return Boolean value or default
 */
bool evocore_config_get_bool(const evocore_config_t *config,
                            const char *section,
                            const char *key,
                            bool default_value);

/**
 * Check if key exists
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param key       Key name
 * @return true if key exists, false otherwise
 */
bool evocore_config_has_key(const evocore_config_t *config,
                           const char *section,
                           const char *key);

#endif /* EVOCORE_CONFIG_H */
```

---

### 4.7 `include/evocore/log.h`

**Purpose:** Logging infrastructure.

```c
#ifndef EVOCORE_LOG_H
#define EVOCORE_LOG_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Log levels
 */
typedef enum {
    EVOCORE_LOG_TRACE = 0,
    EVOCORE_LOG_DEBUG,
    EVOCORE_LOG_INFO,
    EVOCORE_LOG_WARN,
    EVOCORE_LOG_ERROR,
    EVOCORE_LOG_FATAL
} evocore_log_level_t;

/**
 * Set current log level
 *
 * Messages below this level will be ignored.
 *
 * @param level     Log level
 */
void evocore_log_set_level(evocore_log_level_t level);

/**
 * Enable/disable file logging
 *
 * @param enabled   true to enable, false to disable
 * @param path      Path to log file (ignored if disabled)
 * @return true on success, false otherwise
 */
bool evocore_log_set_file(bool enabled, const char *path);

/**
 * Enable/disable colored console output
 *
 * @param enabled   true to enable colors, false to disable
 */
void evocore_log_set_color(bool enabled);

/*========================================================================
 * Logging Macros
 *========================================================================*/

/**
 * Log a trace message
 */
#define evocore_log_trace(...) \
    evocore_log_message(EVOCORE_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a debug message
 */
#define evocore_log_debug(...) \
    evocore_log_message(EVOCORE_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log an info message
 */
#define evocore_log_info(...) \
    evocore_log_message(EVOCORE_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a warning message
 */
#define evocore_log_warn(...) \
    evocore_log_message(EVOCORE_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log an error message
 */
#define evocore_log_error(...) \
    evocore_log_message(EVOCORE_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a fatal message and exit
 */
#define evocore_log_fatal(...) \
    do { \
        evocore_log_message(EVOCORE_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__); \
        exit(1); \
    } while(0)

/**
 * Core logging function
 *
 * @param level     Log level
 * @param file      Source file name
 * @param line      Line number
 * @param fmt       Format string
 * @param ...       Format arguments
 */
void evocore_log_message(evocore_log_level_t level,
                       const char *file,
                       int line,
                       const char *fmt,
                       ...);

#endif /* EVOCORE_LOG_H */
```

---

### 4.8 Implementation Files Summary

| File | Purpose | Key Functions |
|------|---------|---------------|
| `src/error.c` | Error string lookup | `evocore_error_string()` |
| `src/genome.c` | Genome operations | `evocore_genome_init()`, `evocore_genome_clone()`, etc. |
| `src/population.c` | Population management | `evocore_population_init()`, `evocore_population_add()`, etc. |
| `src/config.c` | INI file parser | `evocore_config_load()`, accessors |
| `src/log.c` | Logging implementation | `evocore_log_message()`, level filtering |

---

## 5. Implementation Order

### Week 1: Core Infrastructure

#### Day 1-2: Error & Logging
1. Implement `src/error.c` - error string lookup
2. Implement `src/log.c` - full logging infrastructure
3. Test: compile and run basic logging test

#### Day 3-4: Configuration
1. Implement `src/config.c` - INI parser
2. Create test config file
3. Test: load and read various config values

#### Day 5: Genome
1. Implement `src/genome.c` - all genome operations
2. Unit tests for genome functions
3. Test: create, manipulate, clone genomes

### Week 2: Population & Example

#### Day 6-7: Population
1. Implement `src/population.c` - core population functions
2. Unit tests for population operations
3. Test: create population, add individuals, sort

#### Day 8-9: Selection & Statistics
1. Implement tournament selection
2. Implement statistics calculation
3. Test: selection tournaments, verify stats

#### Day 10: Example & Integration
1. Implement `examples/sphere_function.c`
2. Create `examples/sphere_config.ini`
3. Full integration test
4. Documentation

---

## 6. Testing Strategy

### 6.1 Unit Tests

Each module has corresponding test file:

```c
/* tests/test_genome.c */
void test_genome_init(void);
void test_genome_clone(void);
void test_genome_resize(void);
void test_genome_distance(void);
```

### 6.2 Integration Test

The sphere function example serves as integration test:
- Creates initial population
- Evolves for N generations
- Tracks best fitness over time
- Verifies convergence

### 6.3 Test Execution

```bash
# Run all tests
make test

# Run specific test
./tests/test_genome
```

---

## 7. Success Criteria

### 7.1 Must Have (Blockers)

- [ ] Compiles cleanly with `make` (no warnings)
- [ ] Can run sphere function optimization
- [ ] Configuration loaded from INI file
- [ ] Basic logging to stdout/file
- [ ] Population survives 100 generations

### 7.2 Should Have (Important)

- [ ] All unit tests passing
- [ ] No memory leaks (valgrind clean)
- [ ] Example produces reasonable optimization results
- [ ] README with build/run instructions

### 7.3 Nice to Have

- [ ] Performance benchmarks
- [ ] Additional examples (Rastrigin, Ackley)
- [ ] Doxygen documentation comments

---

## 8. Dependencies

### 8.1 External Dependencies

**None** - Phase 1 uses only standard C99 library.

### 8.2 Build Tools

- GCC or Clang (C99 compatible)
- GNU Make
- Valgrind (for testing, optional)

---

## 9. Risks and Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Memory management bugs | High | Medium | Use valgrind from day 1 |
| INI parser complexity | Medium | Low | Keep parser simple, skip advanced features |
| Population performance | Low | Low | Use efficient data structures, measure early |
| Integration issues | Medium | Medium | Test integration early (Day 8) |

---

## Appendix A: Example Config File

```ini
; examples/sphere_config.ini
; Sphere function optimization configuration

[evolution]
population_size = 100
max_generations = 100
genome_size = 64

[selection]
tournament_size = 3
elite_count = 5

[mutation]
rate = 0.1

[logging]
level = INFO
file = sphere_evolution.log
```

---

## Appendix B: Example Code Structure

```c
/* examples/sphere_function.c */

#include "evocore.h"
#include <math.h>
#include <stdio.h>

#define DIMENSIONS 10

typedef struct {
    double mins[DIMENSIONS];
    double maxs[DIMENSIONS];
} sphere_context_t;

double sphere_fitness(const evocore_genome_t *genome, void *context) {
    sphere_context_t *ctx = (sphere_context_t *)context;

    double values[DIMENSIONS];
    evocore_genome_read(genome, 0, values, sizeof(values));

    double sum = 0.0;
    for (int i = 0; i < DIMENSIONS; i++) {
        sum += values[i] * values[i];
    }

    return -sum;  /* Negative because we minimize, framework maximizes */
}

int main(int argc, char **argv) {
    /* Load config */
    evocore_config_t *config = NULL;
    evocore_config_load("sphere_config.ini", &config);

    /* Set up logging */
    evocore_log_set_level(EVOCORE_LOG_INFO);
    evocore_log_set_file(true, "sphere_evolution.log");

    /* Initialize population */
    int pop_size = evocore_config_get_int(config, NULL, "population_size", 100);
    evocore_population_t pop;
    evocore_population_init(&pop, pop_size);

    /* Run evolution */
    for (int gen = 0; gen < 100; gen++) {
        /* Selection, breeding, mutation, evaluation... */
    }

    /* Cleanup */
    evocore_population_cleanup(&pop);
    evocore_config_free(config);

    return 0;
}
```

---

**End of Phase 1 Detailed Plan**
