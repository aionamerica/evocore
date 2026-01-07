/**
 * Performance Optimization Module
 *
 * Provides memory pools, SIMD operations, and parallel evaluation
 * for high-performance evolutionary computing.
 */

#ifndef EVOCORE_OPTIMIZE_H
#define EVOCORE_OPTIMIZE_H

#include "evocore/genome.h"
#include "evocore/population.h"
#include "evocore/fitness.h"
#include "evocore/error.h"
#include <stddef.h>
#include <stdbool.h>

/*========================================================================
 * Memory Pool for Genome Allocation
 *========================================================================*/

/**
 * Memory pool for efficient genome allocation
 *
 * Reduces allocation overhead by pre-allocating blocks
 * of genome-sized memory chunks.
 */
typedef struct evocore_mempool_t evocore_mempool_t;

/**
 * Create a memory pool
 *
 * @param genome_size    Size of each genome in bytes
 * @param block_size     Number of genomes per block
 * @return New memory pool, or NULL on failure
 */
evocore_mempool_t* evocore_mempool_create(size_t genome_size, size_t block_size);

/**
 * Destroy a memory pool
 *
 * @param pool    Memory pool to destroy
 */
void evocore_mempool_destroy(evocore_mempool_t *pool);

/**
 * Allocate a genome from the pool
 *
 * Faster than malloc as it uses pre-allocated blocks.
 *
 * @param pool    Memory pool
 * @param genome  Genome to initialize (must have data=NULL)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_mempool_alloc(evocore_mempool_t *pool,
                                  evocore_genome_t *genome);

/**
 * Free a genome back to the pool
 *
 * @param pool    Memory pool
 * @param genome  Genome to free
 */
void evocore_mempool_free(evocore_mempool_t *pool,
                        evocore_genome_t *genome);

/**
 * Get memory pool statistics
 *
 * @param pool    Memory pool
 * @param total_allocations   Output: total allocations
 * @param current_allocations Output: currently allocated
 * @param total_blocks        Output: total blocks allocated
 * @param free_blocks         Output: free blocks
 */
void evocore_mempool_get_stats(const evocore_mempool_t *pool,
                              size_t *total_allocations,
                              size_t *current_allocations,
                              size_t *total_blocks,
                              size_t *free_blocks);

/*========================================================================
 * Parallel Fitness Evaluation
 *======================================================================== */

/**
 * Parallel batch evaluation context
 *
 * Manages thread pool for parallel fitness evaluation.
 */
typedef struct evocore_parallel_ctx_t evocore_parallel_ctx_t;

/**
 * Create a parallel evaluation context
 *
 * @param num_threads    Number of threads (0 = auto-detect)
 * @return New context, or NULL on failure
 */
evocore_parallel_ctx_t* evocore_parallel_create(int num_threads);

/**
 * Destroy parallel evaluation context
 *
 * @param ctx    Context to destroy
 */
void evocore_parallel_destroy(evocore_parallel_ctx_t *ctx);

/**
 * Evaluate population in parallel
 *
 * Splits the population across threads for parallel fitness evaluation.
 *
 * @param ctx            Parallel context
 * @param pop            Population to evaluate
 * @param fitness_func   Fitness function
 * @param user_context   User context for fitness function
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_parallel_evaluate_population(evocore_parallel_ctx_t *ctx,
                                                evocore_population_t *pop,
                                                evocore_fitness_func_t fitness_func,
                                                void *user_context);

/**
 * Evaluate batch of genomes in parallel
 *
 * @param ctx            Parallel context
 * @param genomes        Array of genomes
 * @param fitnesses      Output array for fitness values
 * @param count          Number of genomes
 * @param fitness_func   Fitness function
 * @param user_context   User context for fitness function
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_parallel_evaluate_batch(evocore_parallel_ctx_t *ctx,
                                            const evocore_genome_t **genomes,
                                            double *fitnesses,
                                            size_t count,
                                            evocore_fitness_func_t fitness_func,
                                            void *user_context);

/**
 * Get number of threads in context
 *
 * @param ctx    Parallel context
 * @return Number of threads
 */
int evocore_parallel_get_num_threads(const evocore_parallel_ctx_t *ctx);

/*========================================================================
 * SIMD Genome Operations
 *======================================================================== */

/**
 * SIMD-optimized genome mutation
 *
 * Uses SIMD instructions for faster mutation of large genomes.
 * Falls back to scalar implementation if SIMD unavailable.
 *
 * @param genome    Genome to mutate
 * @param rate      Mutation rate (0.0 to 1.0)
 * @param seed      Random seed pointer
 */
void evocore_simd_mutate_genome(evocore_genome_t *genome,
                              double rate,
                              unsigned int *seed);

/**
 * SIMD-optimized genome comparison
 *
 * Quickly calculates Hamming distance between two genomes.
 *
 * @param a    First genome
 * @param b    Second genome
 * @return Number of differing bytes
 */
size_t evocore_simd_genome_hamming_distance(const evocore_genome_t *a,
                                          const evocore_genome_t *b);

/**
 * Check if SIMD optimizations are available
 *
 * @return true if SIMD is available, false otherwise
 */
bool evocore_simd_available(void);

/*========================================================================
 * Cache-Friendly Population Layout
 *======================================================================== */

/**
 * Reorganize population for cache efficiency
 *
 * Structures-of-Arrays layout improves cache hit rates for
 * fitness evaluation and selection operations.
 *
 * @param pop    Population to reorganize
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_optimize_layout(evocore_population_t *pop);

/*========================================================================
 * Performance Monitoring
 *========================================================================*/

/**
 * Performance counter
 */
typedef struct {
    const char *name;
    long long count;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
} evocore_perf_counter_t;

/**
 * Maximum number of performance counters
 */
#define EVOCORE_MAX_PERF_COUNTERS 32

/**
 * Performance monitoring context
 */
typedef struct {
    evocore_perf_counter_t counters[EVOCORE_MAX_PERF_COUNTERS];
    int count;
    bool enabled;
} evocore_perf_monitor_t;

/**
 * Get global performance monitor
 *
 * @return Performance monitor pointer
 */
evocore_perf_monitor_t* evocore_perf_monitor_get(void);

/**
 * Reset all performance counters
 */
void evocore_perf_reset(void);

/**
 * Enable/disable performance monitoring
 *
 * @param enabled    true to enable, false to disable
 */
void evocore_perf_set_enabled(bool enabled);

/**
 * Start a performance measurement
 *
 * @param name    Counter name
 * @return Counter index
 */
int evocore_perf_start(const char *name);

/**
 * End a performance measurement
 *
 * @param index    Counter index from perf_start
 * @return Time elapsed in milliseconds
 */
double evocore_perf_end(int index);

/**
 * Print all performance counters
 */
void evocore_perf_print(void);

/**
 * Get counter by name
 *
 * @param name    Counter name
 * @return Counter pointer, or NULL if not found
 */
const evocore_perf_counter_t* evocore_perf_get(const char *name);

#endif /* EVOCORE_OPTIMIZE_H */
