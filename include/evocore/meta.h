#ifndef EVOCORE_META_H
#define EVOCORE_META_H

#include "evocore/genome.h"
#include "evocore/domain.h"
#include "evocore/error.h"
#include <stddef.h>

/*========================================================================
 * Meta-Evolution Layer
 * ======================================================================
 *
 * The meta-evolution layer evolves the evolutionary parameters themselves.
 * Instead of fixed mutation rates, selection pressures, etc., these
 * parameters become part of a meta-genome that co-evolves with the
 * solution population.
 *
 * Level 3: META-EVOLUTION (Evolves: mutation rates, selection pressure)
 * Level 2: STRATEGY EVOLUTION (Evolves: domain parameters)
 * Level 1: DOMAIN (Executes: fitness evaluation)
 */

/*========================================================================
 * Meta-Parameters Structure
 *========================================================================*/

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

    /* ========================================================================
     * NEGATIVE LEARNING (NEW)
     * ======================================================================== */

    /** Enable negative learning (true/false) */
    bool negative_learning_enabled;

    /** Influence of negative learning on selection (0.0 - 1.0) */
    double negative_penalty_weight;

    /** How fast penalties decay per generation (0.0 - 0.2) */
    double negative_decay_rate;

    /** Maximum failures stored per context (100 - 5000) */
    size_t negative_capacity;

    /** Genome similarity threshold for matching (0.5 - 0.95) */
    double negative_similarity_threshold;

    /** Minimum penalty before forbidding sampling (0.3 - 0.8) */
    double negative_forbidden_threshold;

} evocore_meta_params_t;

/*========================================================================
 * Meta-Individual Structure
 *========================================================================*/

/**
 * Meta-individual: A set of meta-parameters being evolved
 *
 * Each meta-individual represents a complete configuration for
 * evolutionary parameters. The meta-evolution maintains a population
 * of these configurations.
 */
typedef struct {
    evocore_meta_params_t params;
    double meta_fitness;
    int generation;
    double *fitness_history;
    size_t history_size;
    size_t history_capacity;
} evocore_meta_individual_t;

/*========================================================================
 * Meta-Population Structure
 *========================================================================*/

/**
 * Maximum number of meta-individuals in meta-population
 */
#define EVOCORE_MAX_META_INDIVIDUALS 20

/**
 * Meta-population state
 *
 * Manages the population of meta-parameter configurations.
 */
typedef struct {
    evocore_meta_individual_t individuals[EVOCORE_MAX_META_INDIVIDUALS];
    int count;
    int current_generation;
    evocore_meta_params_t best_params;
    double best_meta_fitness;
    bool initialized;
} evocore_meta_population_t;

/*========================================================================
 * Meta-Parameter Management
 *========================================================================*/

/**
 * Initialize meta-parameters with well-tested defaults
 *
 * @param params    Meta-parameters structure to initialize
 */
void evocore_meta_params_init(evocore_meta_params_t *params);

/**
 * Validate meta-parameters
 *
 * Checks that all parameters are within acceptable ranges.
 *
 * @param params    Meta-parameters to validate
 * @return EVOCORE_OK if valid, error code otherwise
 */
evocore_error_t evocore_meta_params_validate(const evocore_meta_params_t *params);

/**
 * Mutate meta-parameters
 *
 * Applies random mutations to the meta-parameters based on
 * the meta_mutation_rate field.
 *
 * @param params    Meta-parameters to mutate
 * @param seed      Random seed pointer (updated each call)
 */
void evocore_meta_params_mutate(evocore_meta_params_t *params,
                               unsigned int *seed);

/**
 * Clone meta-parameters
 *
 * @param src       Source parameters
 * @param dst       Destination parameters
 */
void evocore_meta_params_clone(const evocore_meta_params_t *src,
                              evocore_meta_params_t *dst);

/*========================================================================
 * Meta-Individual Management
 *========================================================================*/

/**
 * Initialize a meta-individual
 *
 * @param individual    Meta-individual to initialize
 * @param params        Initial parameters (use NULL for defaults)
 * @param history_capacity   Maximum fitness history to store
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_individual_init(evocore_meta_individual_t *individual,
                                          const evocore_meta_params_t *params,
                                          size_t history_capacity);

/**
 * Cleanup a meta-individual
 *
 * @param individual    Meta-individual to cleanup
 */
void evocore_meta_individual_cleanup(evocore_meta_individual_t *individual);

/**
 * Record fitness for a meta-individual
 *
 * @param individual    Meta-individual
 * @param fitness       Fitness value to record
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_individual_record_fitness(evocore_meta_individual_t *individual,
                                                    double fitness);

/**
 * Get average fitness for a meta-individual
 *
 * @param individual    Meta-individual
 * @return Average fitness over history, or 0 if no history
 */
double evocore_meta_individual_average_fitness(const evocore_meta_individual_t *individual);

/**
 * Get improvement trend for a meta-individual
 *
 * Positive values indicate improving fitness over time.
 *
 * @param individual    Meta-individual
 * @return Improvement rate (positive = improving, negative = declining)
 */
double evocore_meta_individual_improvement_trend(const evocore_meta_individual_t *individual);

/*========================================================================
 * Meta-Population Management
 *========================================================================*/

/**
 * Initialize meta-population
 *
 * Creates an initial population of meta-individuals with
 * varied parameter settings.
 *
 * @param meta_pop      Meta-population to initialize
 * @param size          Number of meta-individuals to create
 * @param seed          Random seed (NULL to use current time)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_population_init(evocore_meta_population_t *meta_pop,
                                          int size,
                                          unsigned int *seed);

/**
 * Cleanup meta-population
 *
 * @param meta_pop      Meta-population to cleanup
 */
void evocore_meta_population_cleanup(evocore_meta_population_t *meta_pop);

/**
 * Get best meta-individual from population
 *
 * @param meta_pop      Meta-population
 * @return Pointer to best individual, or NULL if empty
 */
evocore_meta_individual_t* evocore_meta_population_best(evocore_meta_population_t *meta_pop);

/**
 * Evolve meta-population to next generation
 *
 * Applies selection, mutation, and crossover to meta-individuals.
 *
 * @param meta_pop      Meta-population to evolve
 * @param seed          Random seed pointer (updated each call)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_population_evolve(evocore_meta_population_t *meta_pop,
                                            unsigned int *seed);

/**
 * Sort meta-population by fitness (best first)
 *
 * @param meta_pop      Meta-population to sort
 */
void evocore_meta_population_sort(evocore_meta_population_t *meta_pop);

/**
 * Check if meta-population has converged
 *
 * Convergence is detected when the best fitness hasn't improved
 * significantly for several generations.
 *
 * @param meta_pop      Meta-population to check
 * @param threshold     Improvement threshold (below this = converged)
 * @param generations   Number of generations to check
 * @return true if converged, false otherwise
 */
bool evocore_meta_population_converged(const evocore_meta_population_t *meta_pop,
                                      double threshold,
                                      int generations);

/*========================================================================
 * Adaptive Parameter Adjustment
 *========================================================================*/

/**
 * Adjust parameters based on recent performance
 *
 * Performs online adjustment of meta-parameters based on
 * the population's recent fitness history.
 *
 * @param params        Meta-parameters to adjust
 * @param recent_fitness    Array of recent fitness values
 * @param count         Number of fitness values
 * @param improvement   True if fitness is improving, false otherwise
 */
void evocore_meta_adapt(evocore_meta_params_t *params,
                       const double *recent_fitness,
                       size_t count,
                       bool improvement);

/**
 * Suggest mutation rate based on population diversity
 *
 * Higher diversity -> lower mutation rate (exploitation)
 * Lower diversity -> higher mutation rate (exploration)
 *
 * @param diversity     Population diversity (0.0 to 1.0)
 * @param params        Meta-parameters to receive suggestion
 */
void evocore_meta_suggest_mutation_rate(double diversity,
                                       evocore_meta_params_t *params);

/**
 * Suggest selection pressure based on fitness distribution
 *
 * Tight distribution -> higher pressure (fine-tuning)
 * Wide distribution -> lower pressure (exploration)
 *
 * @param fitness_stddev   Standard deviation of fitness values
 * @param params        Meta-parameters to receive suggestion
 */
void evocore_meta_suggest_selection_pressure(double fitness_stddev,
                                            evocore_meta_params_t *params);

/*========================================================================
 * Meta-Evaluation
 *========================================================================*/

/**
 * Evaluate meta-fitness for a parameter set
 *
 * Meta-fitness is calculated based on:
 * 1. Best fitness achieved with these parameters
 * 2. Rate of improvement
 * 3. Population diversity maintained
 *
 * @param params        Meta-parameters to evaluate
 * @param best_fitness  Best fitness achieved
 * @param avg_fitness   Average fitness achieved
 * @param diversity     Population diversity
 * @param generations   Number of generations run
 * @return Meta-fitness score (higher is better)
 */
double evocore_meta_evaluate(const evocore_meta_params_t *params,
                            double best_fitness,
                            double avg_fitness,
                            double diversity,
                            int generations);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Print meta-parameters
 *
 * @param params    Meta-parameters to print
 */
void evocore_meta_params_print(const evocore_meta_params_t *params);

/**
 * Get parameter value by name
 *
 * @param params    Meta-parameters
 * @param name      Parameter name (e.g., "optimization_mutation_rate")
 * @return Parameter value, or 0 if not found
 */
double evocore_meta_params_get(const evocore_meta_params_t *params,
                              const char *name);

/**
 * Set parameter value by name
 *
 * @param params    Meta-parameters
 * @param name      Parameter name
 * @param value     New value
 * @return EVOCORE_OK if found and set, error code otherwise
 */
evocore_error_t evocore_meta_params_set(evocore_meta_params_t *params,
                                      const char *name,
                                      double value);

/*========================================================================
 * Online Learning (Adaptive)
 *========================================================================*/

/**
 * Record fitness outcome for online learning
 *
 * This function should be called after each generation to
 * build up knowledge about which parameter settings work best.
 *
 * @param mutation_rate      Mutation rate that was used
 * @param exploration_factor Exploration factor that was used
 * @param fitness            Fitness achieved
 * @param learning_rate      How much to weight this new observation
 */
void evocore_meta_learn_outcome(double mutation_rate,
                               double exploration_factor,
                               double fitness,
                               double learning_rate);

/**
 * Get recommended parameters based on learning history
 *
 * @param mutation_rate_out     Output: recommended mutation rate
 * @param exploration_out       Output: recommended exploration factor
 * @param min_samples           Minimum samples per bucket to trust it
 * @return true if recommendations available, false otherwise
 */
bool evocore_meta_get_learned_params(double *mutation_rate_out,
                                    double *exploration_out,
                                    int min_samples);

/**
 * Reset learning history
 */
void evocore_meta_reset_learning(void);

#endif /* EVOCORE_META_H */
