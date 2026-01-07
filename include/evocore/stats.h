/**
 * Statistics and Monitoring Module
 *
 * Provides collection and reporting of evolutionary statistics,
 * including convergence detection, diversity metrics, and progress tracking.
 */

#ifndef EVOCORE_STATS_H
#define EVOCORE_STATS_H

#include "evocore/population.h"
#include "evocore/error.h"
#include "evocore/optimize.h"
#include "evocore/memory.h"
#include <stddef.h>
#include <stdbool.h>

/*========================================================================
 * Evolution Statistics
 *========================================================================*/

/**
 * Evolution run statistics
 *
 * Tracks progress and metrics across generations.
 */
typedef struct {
    /* Generation info */
    size_t current_generation;
    size_t total_generations;

    /* Fitness tracking */
    double best_fitness_ever;
    double worst_fitness_ever;
    double best_fitness_current;
    double avg_fitness_current;
    double worst_fitness_current;

    /* Convergence metrics */
    double fitness_improvement_rate;    /* Recent improvement per gen */
    double fitness_variance;            /* Population diversity */
    int stagnant_generations;           /* Generations without improvement */
    int convergence_streak;            /* Generations since last best */

    /* Timing */
    double total_time_ms;
    double generation_time_ms;
    double eval_time_ms;

    /* Operation counts */
    long long total_evaluations;
    long long mutations_performed;
    long long crossovers_performed;

    /* Memory usage */
    size_t memory_usage_bytes;

    /* Tracking options */
    bool track_memory;
    bool track_timing;

    /* Status flags */
    bool converged;
    bool stagnant;
    bool diverse;
} evocore_stats_t;

/**
 * Statistics tracking configuration
 */
typedef struct {
    double improvement_threshold;       /* Minimum improvement to reset stagnation */
    int stagnation_generations;         /* Generations without improvement = stagnant */
    double diversity_threshold;         /* Minimum variance for "diverse" population */
    bool track_memory;
    bool track_timing;
} evocore_stats_config_t;

/**
 * Default statistics configuration
 */
#define EVOCORE_STATS_CONFIG_DEFAULTS { \
    .improvement_threshold = 0.001, \
    .stagnation_generations = 50, \
    .diversity_threshold = 1.0, \
    .track_memory = true, \
    .track_timing = true \
}

/*========================================================================
 * Statistics API
 *========================================================================*/

/**
 * Initialize statistics tracker
 *
 * @param config    Configuration (NULL for defaults)
 * @return Allocated stats structure, or NULL on failure
 */
evocore_stats_t* evocore_stats_create(const evocore_stats_config_t *config);

/**
 * Free statistics tracker
 *
 * @param stats    Statistics to free
 */
void evocore_stats_free(evocore_stats_t *stats);

/**
 * Update statistics from current population
 *
 * @param stats    Statistics to update
 * @param pop      Current population
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_stats_update(evocore_stats_t *stats,
                                const evocore_population_t *pop);

/**
 * Update statistics after operations
 *
 * @param stats        Statistics to update
 * @param eval_count   Number of evaluations performed
 * @param mutations    Number of mutations performed
 * @param crossovers   Number of crossovers performed
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_stats_record_operations(evocore_stats_t *stats,
                                          long long eval_count,
                                          long long mutations,
                                          long long crossovers);

/**
 * Check if population has converged
 *
 * @param stats    Statistics to check
 * @return true if converged, false otherwise
 */
bool evocore_stats_is_converged(const evocore_stats_t *stats);

/**
 * Check if population is stagnant
 *
 * @param stats    Statistics to check
 * @return true if stagnant, false otherwise
 */
bool evocore_stats_is_stagnant(const evocore_stats_t *stats);

/**
 * Get diversity metric (0 to 1, higher = more diverse)
 *
 * @param pop    Population to analyze
 * @return Diversity score, or 0 on error
 */
double evocore_stats_diversity(const evocore_population_t *pop);

/**
 * Get fitness statistics for population
 *
 * @param pop            Population to analyze
 * @param out_min        Output: minimum fitness (can be NULL)
 * @param out_max        Output: maximum fitness (can be NULL)
 * @param out_mean       Output: mean fitness (can be NULL)
 * @param out_stddev     Output: standard deviation (can be NULL)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_stats_fitness_distribution(const evocore_population_t *pop,
                                             double *out_min,
                                             double *out_max,
                                             double *out_mean,
                                             double *out_stddev);

/*========================================================================
 * Progress Reporting
 *========================================================================*/

/**
 * Progress report callback
 *
 * Called periodically during evolution to report progress.
 *
 * @param stats       Current statistics
 * @param user_data   User-provided context
 */
typedef void (*evocore_progress_callback_t)(const evocore_stats_t *stats,
                                          void *user_data);

/**
 * Progress report format
 */
typedef struct {
    evocore_progress_callback_t callback;
    void *user_data;
    int report_every_n_generations;
    bool verbose;
} evocore_progress_reporter_t;

/**
 * Create progress reporter
 *
 * @param reporter    Reporter structure to initialize
 * @param callback    Progress callback function
 * @param user_data   User data for callback
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_progress_reporter_init(evocore_progress_reporter_t *reporter,
                                         evocore_progress_callback_t callback,
                                         void *user_data);

/**
 * Report progress (calls callback if conditions met)
 *
 * @param reporter    Progress reporter
 * @param stats      Current statistics
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_progress_report(const evocore_progress_reporter_t *reporter,
                                  const evocore_stats_t *stats);

/**
 * Default console progress callback
 *
 * Prints statistics to stdout in a formatted table.
 *
 * @param stats       Current statistics
 * @param user_data   User data (unused)
 */
void evocore_progress_print_console(const evocore_stats_t *stats,
                                  void *user_data);

/*========================================================================
 * Diagnostic Info
 *========================================================================*/

/**
 * Diagnostic report
 *
 * Comprehensive diagnostic information for debugging.
 */
typedef struct {
    char version[32];
    char build_timestamp[64];

    /* System info */
    int num_cores;
    bool simd_available;
    bool openmp_available;

    /* Runtime stats */
    evocore_memory_stats_t memory;
    evocore_perf_monitor_t perf;

    /* Current evolution state */
    size_t population_size;
    size_t population_capacity;
    size_t generation;
    double best_fitness;

    /* Health indicators */
    bool memory_healthy;
    bool performance_healthy;
    bool population_healthy;

} evocore_diagnostic_report_t;

/**
 * Generate diagnostic report
 *
 * @param pop        Current population (can be NULL)
 * @param report     Output: diagnostic report
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_diagnostic_generate(const evocore_population_t *pop,
                                       evocore_diagnostic_report_t *report);

/**
 * Print diagnostic report to stdout
 *
 * @param report    Report to print
 */
void evocore_diagnostic_print(const evocore_diagnostic_report_t *report);

/**
 * Log diagnostic report (using evocore_log system)
 *
 * @param report    Report to log
 */
void evocore_diagnostic_log(const evocore_diagnostic_report_t *report);

#endif /* EVOCORE_STATS_H */
