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
 * Frees all genomes and the population array.
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
 * Frees the genome at that index.
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
 * Preserves existing individuals.
 *
 * @param pop           Population to resize
 * @param new_capacity  New capacity
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_resize(evocore_population_t *pop,
                                      size_t new_capacity);

/**
 * Set population size without changing capacity
 *
 * Useful after bulk operations.
 *
 * @param pop       Population to modify
 * @param size      New size (must be <= capacity)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_set_size(evocore_population_t *pop,
                                        size_t size);

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
 * Assumes stats are up to date (call evocore_population_update_stats first).
 *
 * @param pop       Population
 * @return Pointer to best individual, or NULL if empty
 */
evocore_individual_t* evocore_population_get_best(evocore_population_t *pop);

/**
 * Get population size
 *
 * @param pop       Population
 * @return Current size
 */
size_t evocore_population_size(const evocore_population_t *pop);

/**
 * Get population capacity
 *
 * @param pop       Population
 * @return Maximum capacity
 */
size_t evocore_population_capacity(const evocore_population_t *pop);

/**
 * Get population generation
 *
 * @param pop       Population
 * @return Current generation number
 */
size_t evocore_population_generation(const evocore_population_t *pop);

/**
 * Increment generation counter
 *
 * @param pop       Population
 */
void evocore_population_increment_generation(evocore_population_t *pop);

/*========================================================================
 * Population Statistics
 *========================================================================*/

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
 * Best fitness first.
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
 * @param pop               Population to select from
 * @param tournament_size   Number of individuals in tournament
 * @param seed              Random seed pointer (will be updated)
 * @return Index of selected parent, or SIZE_MAX on error
 */
size_t evocore_population_tournament_select(const evocore_population_t *pop,
                                          size_t tournament_size,
                                          unsigned int *seed);

/**
 * Truncate population to keep top N individuals
 *
 * Frees removed individuals.
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

/**
 * Perform crossover between two parents to create offspring
 *
 * Uses uniform crossover - each byte has 50% chance from each parent.
 *
 * @param parent1   First parent genome
 * @param parent2   Second parent genome
 * @param child1    First child (initialized by this function)
 * @param child2    Second child (initialized by this function)
 * @param seed      Random seed pointer (will be updated)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_crossover(const evocore_genome_t *parent1,
                                      const evocore_genome_t *parent2,
                                      evocore_genome_t *child1,
                                      evocore_genome_t *child2,
                                      unsigned int *seed);

/**
 * Mutate a genome in-place
 *
 * Each byte has a chance to be replaced with a random value.
 *
 * @param genome    Genome to mutate
 * @param rate      Mutation rate (0.0 to 1.0)
 * @param seed      Random seed pointer (will be updated)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_mutate(evocore_genome_t *genome,
                                   double rate,
                                   unsigned int *seed);

#endif /* EVOCORE_POPULATION_H */
