/**
 * Evocore Context Learning Module
 *
 * Multi-dimensional context learning system for domain-specific knowledge.
 *
 * Replaces rigid 5D bucket systems with a flexible framework for learning
 * parameter distributions across any number of dimensions.
 *
 * Example: Trading system contexts
 * - Dimensions: Strategy, Asset, Leverage, Timeframe, Volatility
 * - Context: "MA_CROSSOVER:BTC:LOW:1h:NORMAL"
 * - Learning: Tracks parameter statistics per context
 */

#ifndef EVOCORE_CONTEXT_H
#define EVOCORE_CONTEXT_H

#include "evocore/weighted.h"
#include "evocore/genome.h"
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/*========================================================================
 * Forward Declarations
 *========================================================================*/
/* Include negative.h for negative learning types and enums
 * Note: negative.h only depends on genome.h and error.h, which are already included
 */
#include "evocore/negative.h"

/*========================================================================
 * Data Structures
 *========================================================================*/

/**
 * Context dimension definition
 *
 * Defines one dimension of the context space (e.g., "Asset" with values
 * BTC, ETH, SOL, MATIC)
 */
typedef struct {
    char *name;              /* Dimension name (allocated) */
    size_t value_count;      /* Number of possible values */
    char **values;           /* Array of value strings (allocated) */
} evocore_context_dimension_t;


/**
 * Context statistics
 *
 * Tracks learned parameter distributions for a specific context.
 * Uses weighted statistics for fitness-weighted learning.
 */
typedef struct {
    char *key;                       /* Context key (e.g., "BTC:1h:NORMAL") */
    evocore_weighted_array_t *stats; /* Per-parameter statistics */
    size_t param_count;              /* Number of parameters tracked */
    double confidence;               /* Overall confidence 0-1 */
    time_t first_update;             /* First learning timestamp */
    time_t last_update;              /* Last learning timestamp */
    size_t total_experiences;        /* Total number of updates */
    double avg_fitness;              /* Average fitness of all updates */
    double best_fitness;             /* Best fitness seen */

    /* Negative Learning */
    evocore_negative_learning_t *negative;  /* Per-context negative learning */
    size_t failure_count;                   /* Failures in this context */
    double avg_failure_fitness;             /* Average failure fitness */
} evocore_context_stats_t;

/**
 * Context system
 *
 * Manages multi-dimensional learning buckets.
 */
typedef struct {
    evocore_context_dimension_t *dimensions;  /* Dimension definitions */
    size_t dimension_count;                   /* Number of dimensions */
    void *internal;                           /* Internal hash table */
    size_t param_count;                       /* Parameters per context */
    size_t total_contexts;                    /* Total contexts stored */
} evocore_context_system_t;

/**
 * Context query result
 */
typedef struct {
    evocore_context_stats_t **contexts;  /* Array of context pointers */
    size_t count;                         /* Number of results */
    size_t capacity;                      /* Allocated capacity */
} evocore_context_query_t;

/*========================================================================
 * System Management
 *========================================================================*/

/**
 * Create a context system
 *
 * @param dimensions Array of dimension definitions
 * @param dimension_count Number of dimensions
 * @param param_count Number of parameters to track per context
 * @return New context system, or NULL on error
 */
evocore_context_system_t* evocore_context_system_create(
    const evocore_context_dimension_t *dimensions,
    size_t dimension_count,
    size_t param_count
);

/**
 * Free context system
 *
 * @param system System to free
 */
void evocore_context_system_free(evocore_context_system_t *system);

/**
 * Add a dimension to existing system
 *
 * @param system Context system
 * @param name Dimension name
 * @param values Array of possible values
 * @param value_count Number of values
 * @return true on success
 */
bool evocore_context_add_dimension(
    evocore_context_system_t *system,
    const char *name,
    const char **values,
    size_t value_count
);

/*========================================================================
 * Context Keys
 *========================================================================*/

/**
 * Build a context key from dimension values
 *
 * Creates a string like "MA_CROSSOVER:BTC:LOW:1h:NORMAL"
 *
 * @param system Context system
 * @param dimension_values Array of dimension values (one per dimension)
 * @param out_key Output buffer for key
 * @param key_size Size of output buffer
 * @return true on success
 */
bool evocore_context_build_key(
    const evocore_context_system_t *system,
    const char **dimension_values,
    char *out_key,
    size_t key_size
);

/**
 * Parse a context key into dimension values
 *
 * @param system Context system
 * @param key Context key to parse
 * @param out_values Output array (must have dimension_count entries)
 * @return true on success
 */
bool evocore_context_parse_key(
    const evocore_context_system_t *system,
    const char *key,
    char **out_values
);

/**
 * Validate dimension values
 *
 * @param system Context system
 * @param dimension_values Array of values to validate
 * @return true if all values are valid for their dimensions
 */
bool evocore_context_validate_values(
    const evocore_context_system_t *system,
    const char **dimension_values
);

/*========================================================================
 * Learning Operations
 *========================================================================*/

/**
 * Learn from experience
 *
 * Updates the context's weighted statistics with new parameter values.
 * Higher fitness values contribute more to the learned distribution.
 *
 * @param system Context system
 * @param dimension_values Dimension values defining the context
 * @param parameters Parameter array (must have param_count entries)
 * @param fitness Fitness value (weights the update)
 * @return true on success
 */
bool evocore_context_learn(
    evocore_context_system_t *system,
    const char **dimension_values,
    const double *parameters,
    size_t param_count,
    double fitness
);

/**
 * Learn with explicit context key
 *
 * Same as evocore_context_learn but uses pre-built key.
 *
 * @param system Context system
 * @param context_key Pre-built context key
 * @param parameters Parameter array
 * @param param_count Number of parameters
 * @param fitness Fitness value
 * @return true on success
 */
bool evocore_context_learn_key(
    evocore_context_system_t *system,
    const char *context_key,
    const double *parameters,
    size_t param_count,
    double fitness
);

/*========================================================================
 * Statistics Retrieval
 *========================================================================*/

/**
 * Get statistics for a context
 *
 * Returns the learned statistics for the specified context.
 * Creates a new context if it doesn't exist.
 *
 * @param system Context system
 * @param dimension_values Dimension values defining the context
 * @param out_stats Output statistics pointer (valid until next call)
 * @return true on success
 */
bool evocore_context_get_stats(
    evocore_context_system_t *system,
    const char **dimension_values,
    evocore_context_stats_t **out_stats
);

/**
 * Get statistics by context key
 *
 * @param system Context system
 * @param context_key Context key
 * @param out_stats Output statistics pointer
 * @return true on success
 */
bool evocore_context_get_stats_key(
    const evocore_context_system_t *system,
    const char *context_key,
    evocore_context_stats_t **out_stats
);

/**
 * Check if context has sufficient data
 *
 * @param stats Context statistics
 * @param min_samples Minimum samples required
 * @return true if context has enough data
 */
bool evocore_context_has_data(
    const evocore_context_stats_t *stats,
    size_t min_samples
);

/*========================================================================
 * Sampling
 *========================================================================*/

/**
 * Sample parameters from a context
 *
 * Generates parameters from the learned distribution for the specified
 * context. If insufficient data, falls back to random sampling.
 *
 * @param system Context system
 * @param dimension_values Dimension values defining the context
 * @param out_parameters Output parameter array
 * @param param_count Number of parameters
 * @param exploration_factor 0=pure exploit, 1=pure explore
 * @param seed Random seed pointer
 * @return true on success
 */
bool evocore_context_sample(
    const evocore_context_system_t *system,
    const char **dimension_values,
    double *out_parameters,
    size_t param_count,
    double exploration_factor,
    unsigned int *seed
);

/**
 * Sample from context key
 *
 * @param system Context system
 * @param context_key Context key
 * @param out_parameters Output parameter array
 * @param param_count Number of parameters
 * @param exploration_factor Exploration vs exploitation balance
 * @param seed Random seed pointer
 * @return true on success
 */
bool evocore_context_sample_key(
    const evocore_context_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double exploration_factor,
    unsigned int *seed
);

/*========================================================================
 * Query Operations
 *========================================================================*/

/**
 * Query best-performing contexts
 *
 * Returns contexts sorted by fitness/confidence.
 *
 * @param system Context system
 * @param partial_match Partial key to match (e.g., "BTC:1h")
 * @param min_samples Minimum samples required
 * @param out_results Output query results
 * @param max_results Maximum results to return
 * @return true on success
 */
bool evocore_context_query_best(
    const evocore_context_system_t *system,
    const char *partial_match,
    size_t min_samples,
    evocore_context_query_t *out_results,
    size_t max_results
);

/**
 * Free query results
 *
 * @param results Query results to free
 */
void evocore_context_query_free(evocore_context_query_t *results);

/**
 * Get context count
 *
 * @param system Context system
 * @return Total number of contexts stored
 */
size_t evocore_context_count(const evocore_context_system_t *system);

/**
 * Get all context keys
 *
 * @param system Context system
 * @param out_keys Output array of keys (caller allocates)
 * @param max_keys Maximum keys to return
 * @return Number of keys returned
 */
size_t evocore_context_get_keys(
    const evocore_context_system_t *system,
    char **out_keys,
    size_t max_keys
);

/*========================================================================
 * Persistence
 *========================================================================*/

/**
 * Save context system to JSON
 *
 * @param system Context system
 * @param filepath Output file path
 * @return true on success
 */
bool evocore_context_save_json(
    const evocore_context_system_t *system,
    const char *filepath
);

/**
 * Load context system from JSON
 *
 * @param filepath Input file path
 * @param out_system Output context system
 * @return true on success
 */
bool evocore_context_load_json(
    const char *filepath,
    evocore_context_system_t **out_system
);

/**
 * Save context system to binary file
 *
 * Binary format is ~5-10x more efficient than JSON.
 *
 * @param system Context system to save
 * @param filepath Output file path
 * @return true on success
 */
bool evocore_context_save_binary(
    const evocore_context_system_t *system,
    const char *filepath
);

/**
 * Load context system from binary file
 *
 * @param filepath Input file path
 * @param out_system Output context system (caller must free with evocore_context_system_free)
 * @return true on success
 */
bool evocore_context_load_binary(
    const char *filepath,
    evocore_context_system_t **out_system
);

/**
 * Export context statistics to CSV
 *
 * @param system Context system
 * @param filepath Output file path
 * @return true on success
 */
bool evocore_context_export_csv(
    const evocore_context_system_t *system,
    const char *filepath
);

/*========================================================================
 * Negative Learning Integration
 *========================================================================*/

/**
 * Record failure in context
 *
 * Records a failed outcome for the given context.
 *
 * @param system Context system
 * @param context_key Pre-built context key
 * @param genome Failed genome
 * @param fitness Fitness value (typically negative)
 * @param severity Failure severity
 * @param generation Current generation
 * @return true on success
 */
bool evocore_context_record_failure(
    evocore_context_system_t *system,
    const char *context_key,
    const evocore_genome_t *genome,
    double fitness,
    evocore_failure_severity_t severity,
    int generation
);

/**
 * Check penalty for genome in context
 *
 * Returns the penalty score for a genome in the given context.
 *
 * @param system Context system
 * @param context_key Context key
 * @param genome Genome to check
 * @param penalty_out Output: penalty score (0.0-1.0)
 * @return true on success
 */
bool evocore_context_check_penalty(
    const evocore_context_system_t *system,
    const char *context_key,
    const evocore_genome_t *genome,
    double *penalty_out
);

/**
 * Check if genome should be forbidden in context
 *
 * @param system Context system
 * @param context_key Context key
 * @param genome Genome to check
 * @param threshold Penalty threshold
 * @return true if genome should be forbidden
 */
bool evocore_context_is_forbidden(
    const evocore_context_system_t *system,
    const char *context_key,
    const evocore_genome_t *genome,
    double threshold
);

/**
 * Get negative learning stats for context
 *
 * @param system Context system
 * @param context_key Context key
 * @param stats_out Output: negative learning statistics
 * @return true on success
 */
bool evocore_context_get_negative_stats(
    const evocore_context_system_t *system,
    const char *context_key,
    evocore_negative_stats_t *stats_out
);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Reset a context
 *
 * Clears all learning for a specific context.
 *
 * @param system Context system
 * @param dimension_values Dimension values defining the context
 * @return true on success
 */
bool evocore_context_reset(
    evocore_context_system_t *system,
    const char **dimension_values
);

/**
 * Reset all contexts
 *
 * Clears all learning data.
 *
 * @param system Context system
 */
void evocore_context_reset_all(evocore_context_system_t *system);

/**
 * Get context confidence
 *
 * Returns the learning confidence for a context.
 *
 * @param stats Context statistics
 * @return Confidence score 0-1
 */
double evocore_context_confidence(const evocore_context_stats_t *stats);

/**
 * Merge two contexts
 *
 * Combines learning from two contexts.
 *
 * @param system Context system
 * @param target_key Target context key (receives merged data)
 * @param source_key Source context key
 * @return true on success
 */
bool evocore_context_merge(
    evocore_context_system_t *system,
    const char *target_key,
    const char *source_key
);

#endif /* EVOCORE_CONTEXT_H */
