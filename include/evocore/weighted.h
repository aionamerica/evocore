/**
 * Evocore Weighted Statistics Module
 *
 * Implements quality-weighted learning using West's online algorithm.
 * Higher fitness values contribute more to learned parameters.
 *
 * This is the foundation for organic learning - better solutions have
 * more influence on the gene pool, just like natural selection.
 */

#ifndef EVOCORE_WEIGHTED_H
#define EVOCORE_WEIGHTED_H

#include <stddef.h>
#include <stdbool.h>

/*========================================================================
 * Data Structures
 *========================================================================*/

/**
 * Weighted running statistics
 *
 * Tracks mean and variance using West's online algorithm with
 * fitness-weighted updates.
 */
typedef struct {
    double mean;           /* Weighted mean */
    double variance;       /* Weighted variance */
    double sum_weights;    /* Sum of all weights (for normalization) */
    double m2;             /* Sum of squared deviations (for variance) */
    size_t count;          /* Number of observations */
    double min_value;      /* Minimum observed value */
    double max_value;      /* Maximum observed value */
    double sum_weighted_x; /* Sum of value * weight */
} evocore_weighted_stats_t;

/**
 * Weighted statistics array
 *
 * For tracking multiple parameters (e.g., 64 trading parameters)
 */
typedef struct {
    evocore_weighted_stats_t *stats;  /* Array of stats */
    size_t count;                     /* Number of parameters */
} evocore_weighted_array_t;

/*========================================================================
 * Single Value Statistics
 *========================================================================*/

/**
 * Initialize weighted statistics
 *
 * @param stats Statistics structure to initialize
 */
void evocore_weighted_init(evocore_weighted_stats_t *stats);

/**
 * Update weighted statistics with a new observation
 *
 * Uses West's online algorithm for numerically stable weighted
 * mean and variance calculation.
 *
 * @param stats Statistics to update
 * @param value New observation
 * @param weight Fitness weight (higher = more influence)
 * @return true if update successful, false on error
 */
bool evocore_weighted_update(
    evocore_weighted_stats_t *stats,
    double value,
    double weight
);

/**
 * Get weighted mean
 *
 * @param stats Statistics structure
 * @return Weighted mean, or 0.0 if no observations
 */
double evocore_weighted_mean(const evocore_weighted_stats_t *stats);

/**
 * Get weighted standard deviation
 *
 * @param stats Statistics structure
 * @return Weighted std dev, or 0.0 if insufficient samples
 */
double evocore_weighted_std(const evocore_weighted_stats_t *stats);

/**
 * Get weighted variance
 *
 * @param stats Statistics structure
 * @return Weighted variance
 */
double evocore_weighted_variance(const evocore_weighted_stats_t *stats);

/**
 * Sample from weighted distribution
 *
 * Samples from a Gaussian distribution defined by the weighted
 * mean and standard deviation.
 *
 * @param stats Statistics structure
 * @param seed Random seed pointer (updated in-place)
 * @return Sampled value
 */
double evocore_weighted_sample(
    const evocore_weighted_stats_t *stats,
    unsigned int *seed
);

/**
 * Reset statistics
 *
 * Clears all accumulated data while keeping the structure.
 *
 * @param stats Statistics to reset
 */
void evocore_weighted_reset(evocore_weighted_stats_t *stats);

/**
 * Check if statistics have sufficient data
 *
 * @param stats Statistics structure
 * @param min_samples Minimum required samples
 * @return true if count >= min_samples
 */
bool evocore_weighted_has_data(
    const evocore_weighted_stats_t *stats,
    size_t min_samples
);

/**
 * Get confidence score
 *
 * Confidence increases with sample count, capped at 1.0.
 * Uses sqrt(n) scaling: confidence = min(1.0, sqrt(n / max_samples))
 *
 * @param stats Statistics structure
 * @param max_samples Samples for full confidence (default: 100)
 * @return Confidence score 0.0 to 1.0
 */
double evocore_weighted_confidence(
    const evocore_weighted_stats_t *stats,
    size_t max_samples
);

/*========================================================================
 * Array Statistics
 *========================================================================*/

/**
 * Create weighted statistics array
 *
 * @param count Number of parameters to track
 * @return Allocated array, or NULL on error
 */
evocore_weighted_array_t* evocore_weighted_array_create(size_t count);

/**
 * Free weighted statistics array
 *
 * @param array Array to free
 */
void evocore_weighted_array_free(evocore_weighted_array_t *array);

/**
 * Update all parameters in array
 *
 * @param array Statistics array
 * @param values Parameter values
 * @param weights Per-parameter weights (or NULL for uniform weight)
 * @param count Number of values (must match array size)
 * @param global_weight Global weight multiplier applied to all
 * @return true if successful
 */
bool evocore_weighted_array_update(
    evocore_weighted_array_t *array,
    const double *values,
    const double *weights,
    size_t count,
    double global_weight
);

/**
 * Get weighted means for all parameters
 *
 * @param array Statistics array
 * @param out_means Output array (must be pre-allocated)
 * @param count Size of output array
 * @return true if successful
 */
bool evocore_weighted_array_get_means(
    const evocore_weighted_array_t *array,
    double *out_means,
    size_t count
);

/**
 * Get weighted standard deviations for all parameters
 *
 * @param array Statistics array
 * @param out_stds Output array (must be pre-allocated)
 * @param count Size of output array
 * @return true if successful
 */
bool evocore_weighted_array_get_stds(
    const evocore_weighted_array_t *array,
    double *out_stds,
    size_t count
);

/**
 * Sample all parameters from their distributions
 *
 * @param array Statistics array
 * @param out_values Output array (must be pre-allocated)
 * @param count Size of output array
 * @param exploration_factor 0=pure exploit, 1=pure random
 * @param seed Random seed pointer
 * @return true if successful
 */
bool evocore_weighted_array_sample(
    const evocore_weighted_array_t *array,
    double *out_values,
    size_t count,
    double exploration_factor,
    unsigned int *seed
);

/**
 * Reset all statistics in array
 *
 * @param array Statistics array
 */
void evocore_weighted_array_reset(evocore_weighted_array_t *array);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Merge two weighted statistics
 *
 * Combines two independent samples into one aggregate.
 *
 * @param stats1 First statistics (will contain merged result)
 * @param stats2 Second statistics
 * @return true if successful
 */
bool evocore_weighted_merge(
    evocore_weighted_stats_t *stats1,
    const evocore_weighted_stats_t *stats2
);

/**
 * Clone statistics
 *
 * Creates a deep copy of statistics.
 *
 * @param src Source statistics
 * @param dst Destination statistics
 */
void evocore_weighted_clone(
    const evocore_weighted_stats_t *src,
    evocore_weighted_stats_t *dst
);

/**
 * Serialize statistics to JSON
 *
 * @param stats Statistics to serialize
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written, or 0 on error
 */
size_t evocore_weighted_to_json(
    const evocore_weighted_stats_t *stats,
    char *buffer,
    size_t buffer_size
);

/**
 * Deserialize statistics from JSON
 *
 * @param json JSON string
 * @param stats Output statistics
 * @return true if successful
 */
bool evocore_weighted_from_json(
    const char *json,
    evocore_weighted_stats_t *stats
);

#endif /* EVOCORE_WEIGHTED_H */
