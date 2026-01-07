/**
 * Evocore Parameter Synthesis Module
 *
 * Cross-context knowledge transfer and parameter hybridization.
 *
 * This module enables combining learned knowledge from multiple contexts
 * to generate new parameter configurations. It supports various synthesis
 * strategies including weighted averaging, trend projection, and ensemble methods.
 *
 * Use cases:
 * - Transfer learning between related contexts (e.g., BTC -> ETH)
 * - Hybrid parameter generation from multiple sources
 * - Meta-learning across similar domains
 */

#ifndef EVOCORE_SYNTHESIS_H
#define EVOCORE_SYNTHESIS_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/*========================================================================
 * Data Structures
 *========================================================================*/

/**
 * Synthesis strategies
 */
typedef enum {
    EVOCORE_SYNTHESIS_AVERAGE,      /* Simple average of sources */
    EVOCORE_SYNTHESIS_WEIGHTED,     /* Weighted average by confidence */
    EVOCORE_SYNTHESIS_TREND,        /* Project based on trend */
    EVOCORE_SYNTHESIS_REGIME,       /* Select based on regime detection */
    EVOCORE_SYNTHESIS_ENSEMBLE,     /* Ensemble of multiple strategies */
    EVOCORE_SYNTHESIS_NEAREST       /* Nearest neighbor in parameter space */
} evocore_synthesis_strategy_t;

/**
 * Parameter source
 *
 * Represents a single source of parameter knowledge.
 */
typedef struct {
    double *parameters;            /* Parameter values */
    size_t param_count;            /* Number of parameters */
    double confidence;             /* Confidence score (0-1) */
    double fitness;                /* Associated fitness */
    time_t timestamp;              /* When learned */
    char *context_id;              /* Origin context */
} evocore_param_source_t;

/**
 * Synthesis request
 *
 * Describes what parameters to synthesize and from what sources.
 */
typedef struct {
    evocore_synthesis_strategy_t strategy;
    size_t target_param_count;     /* Number of parameters to generate */
    size_t source_count;           /* Number of sources */
    evocore_param_source_t *sources; /* Source array */

    /* Strategy options */
    double exploration_factor;     /* 0=pure synthesis, 1=random */
    double trend_strength;         /* How much to follow trends (0-1) */
    size_t ensemble_count;         /* For ENSEMBLE: number of methods */

    /* Output */
    double *result;                /* Synthesized parameters */
    double synthesis_confidence;   /* Confidence in result */
} evocore_synthesis_request_t;

/**
 * Synthesis cache
 *
 * Caches synthesis results for faster repeated access.
 */
typedef struct {
    evocore_synthesis_request_t *last_request;
    double *cached_result;
    time_t cache_time;
    size_t cache_hits;
    size_t cache_misses;
} evocore_synthesis_cache_t;

/**
 * Context similarity matrix
 *
 * Tracks similarity between contexts for transfer learning.
 */
typedef struct {
    size_t context_count;
    char **context_ids;
    double **similarity_matrix;    /* context_count x context_count */
    time_t last_update;
} evocore_similarity_matrix_t;

/*========================================================================
 * Synthesis Operations
 *========================================================================*/

/**
 * Create synthesis request
 *
 * @param strategy Synthesis strategy
 * @param param_count Number of parameters
 * @param source_count Number of sources
 * @return New synthesis request, or NULL on error
 */
evocore_synthesis_request_t* evocore_synthesis_request_create(
    evocore_synthesis_strategy_t strategy,
    size_t param_count,
    size_t source_count
);

/**
 * Free synthesis request
 *
 * @param request Request to free
 */
void evocore_synthesis_request_free(evocore_synthesis_request_t *request);

/**
 * Add source to synthesis request
 *
 * @param request Synthesis request
 * @param index Source index
 * @param parameters Parameter array
 * @param confidence Confidence score (0-1)
 * @param fitness Associated fitness
 * @param context_id Origin context ID
 * @return true on success
 */
bool evocore_synthesis_add_source(
    evocore_synthesis_request_t *request,
    size_t index,
    const double *parameters,
    double confidence,
    double fitness,
    const char *context_id
);

/**
 * Execute synthesis
 *
 * Generates parameters based on the configured strategy.
 *
 * @param request Synthesis request
 * @param out_parameters Output parameter array
 * @param out_confidence Output confidence, or NULL
 * @param seed Random seed for exploration
 * @return true on success
 */
bool evocore_synthesis_execute(
    evocore_synthesis_request_t *request,
    double *out_parameters,
    double *out_confidence,
    unsigned int *seed
);

/*========================================================================
 * Strategy Implementations
 *========================================================================*/

/**
 * Average synthesis
 *
 * Computes simple average of all source parameters.
 *
 * @param request Synthesis request
 * @param out_parameters Output array
 * @return true on success
 */
bool evocore_synthesis_average(
    const evocore_synthesis_request_t *request,
    double *out_parameters
);

/**
 * Weighted synthesis
 *
 * Computes confidence-weighted average.
 *
 * @param request Synthesis request
 * @param out_parameters Output array
 * @return true on success
 */
bool evocore_synthesis_weighted(
    const evocore_synthesis_request_t *request,
    double *out_parameters
);

/**
 * Trend synthesis
 *
 * Projects parameters based on detected trends.
 *
 * @param request Synthesis request
 * @param out_parameters Output array
 * @param trend_strength Trend projection strength (0-1)
 * @return true on success
 */
bool evocore_synthesis_trend(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    double trend_strength
);

/**
 * Regime-based synthesis
 *
 * Selects sources based on regime similarity.
 *
 * @param request Synthesis request
 * @param out_parameters Output array
 * @param current_regime Current regime identifier
 * @return true on success
 */
bool evocore_synthesis_regime(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    const char *current_regime
);

/**
 * Ensemble synthesis
 *
 * Combines multiple synthesis methods.
 *
 * @param request Synthesis request
 * @param out_parameters Output array
 * @param seed Random seed
 * @return true on success
 */
bool evocore_synthesis_ensemble(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    unsigned int *seed
);

/**
 * Nearest neighbor synthesis
 *
 * Finds most similar source and uses its parameters.
 *
 * @param request Synthesis request
 * @param out_parameters Output array
 * @param target_context Target context to match
 * @return true on success
 */
bool evocore_synthesis_nearest(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    const char *target_context
);

/*========================================================================
 * Similarity and Distance
 *========================================================================*/

/**
 * Create similarity matrix
 *
 * @param context_count Number of contexts
 * @param context_ids Array of context IDs
 * @return New similarity matrix, or NULL on error
 */
evocore_similarity_matrix_t* evocore_similarity_matrix_create(
    size_t context_count,
    char **context_ids
);

/**
 * Free similarity matrix
 *
 * @param matrix Matrix to free
 */
void evocore_similarity_matrix_free(evocore_similarity_matrix_t *matrix);

/**
 * Update similarity between contexts
 *
 * @param matrix Similarity matrix
 * @param context_a First context
 * @param context_b Second context
 * @param similarity Similarity score (0-1)
 * @return true on success
 */
bool evocore_similarity_update(
    evocore_similarity_matrix_t *matrix,
    const char *context_a,
    const char *context_b,
    double similarity
);

/**
 * Get similarity between contexts
 *
 * @param matrix Similarity matrix
 * @param context_a First context
 * @param context_b Second context
 * @return Similarity score (0-1), or 0 if not found
 */
double evocore_similarity_get(
    const evocore_similarity_matrix_t *matrix,
    const char *context_a,
    const char *context_b
);

/**
 * Find most similar context
 *
 * @param matrix Similarity matrix
 * @param target_context Context to match
 * @return Most similar context ID, or NULL
 */
const char* evocore_similarity_find_nearest(
    const evocore_similarity_matrix_t *matrix,
    const char *target_context
);

/**
 * Calculate parameter distance
 *
 * Computes Euclidean distance between two parameter vectors.
 *
 * @param params1 First parameter vector
 * @param params2 Second parameter vector
 * @param count Number of parameters
 * @return Euclidean distance
 */
double evocore_param_distance(
    const double *params1,
    const double *params2,
    size_t count
);

/**
 * Calculate parameter similarity
 *
 * Converts distance to similarity (0-1).
 *
 * @param params1 First parameter vector
 * @param params2 Second parameter vector
 * @param count Number of parameters
 * @param max_distance Maximum expected distance
 * @return Similarity score (0-1)
 */
double evocore_param_similarity(
    const double *params1,
    const double *params2,
    size_t count,
    double max_distance
);

/*========================================================================
 * Transfer Learning
 *========================================================================*/

/**
 * Transfer parameters across contexts
 *
 * Transfers learned parameters from source to target context,
 * adjusted for context differences.
 *
 * @param source_params Source parameters
 * @param source_context Source context ID
 * @param target_context Target context ID
 * @param similarity_matrix Context similarity matrix
 * @param param_count Number of parameters
 * @param out_params Output adjusted parameters
 * @param adjustment_factor How much to adjust (0-1)
 * @return true on success
 */
bool evocore_transfer_params(
    const double *source_params,
    const char *source_context,
    const char *target_context,
    const evocore_similarity_matrix_t *similarity_matrix,
    size_t param_count,
    double *out_params,
    double adjustment_factor
);

/**
 * Find transferable contexts
 *
 * Finds contexts similar enough for parameter transfer.
 *
 * @param target_context Target context
 * @param similarity_matrix Similarity matrix
 * @param min_similarity Minimum similarity threshold
 * @param out_contexts Output array of context IDs
 * @param max_contexts Maximum contexts to return
 * @return Number of contexts found
 */
size_t evocore_find_transferable_contexts(
    const char *target_context,
    const evocore_similarity_matrix_t *similarity_matrix,
    double min_similarity,
    const char **out_contexts,
    size_t max_contexts
);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Validate synthesis request
 *
 * @param request Request to validate
 * @return true if valid
 */
bool evocore_synthesis_validate(const evocore_synthesis_request_t *request);

/**
 * Get synthesis strategy name
 *
 * @param strategy Strategy enum
 * @return String name
 */
const char* evocore_synthesis_strategy_name(evocore_synthesis_strategy_t strategy);

/**
 * Create synthesis cache
 *
 * @return New cache, or NULL on error
 */
evocore_synthesis_cache_t* evocore_synthesis_cache_create(void);

/**
 * Free synthesis cache
 *
 * @param cache Cache to free
 */
void evocore_synthesis_cache_free(evocore_synthesis_cache_t *cache);

/**
 * Clear cache
 *
 * @param cache Cache to clear
 */
void evocore_synthesis_cache_clear(evocore_synthesis_cache_t *cache);

#endif /* EVOCORE_SYNTHESIS_H */
