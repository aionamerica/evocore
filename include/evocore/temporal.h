/**
 * Evocore Temporal Learning Module
 *
 * Time-bucketed organic learning for regime adaptation and temporal intelligence.
 *
 * This module enables the system to:
 * - Learn parameter distributions within time buckets (hourly, daily, monthly)
 * - Compute "organic means" that give equal weight to each time bucket
 * - Detect trends in parameter evolution over time
 * - Compare recent vs historical performance for regime change detection
 *
 * Example: In trading, a parameter that worked well in January 2024 may
 * not work in July 2024 due to market regime changes. Temporal learning
 * maintains separate statistics per time period and can detect when
 * recent performance diverges from historical norms.
 */

#ifndef EVOCORE_TEMPORAL_H
#define EVOCORE_TEMPORAL_H

#include "evocore/weighted.h"
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/*========================================================================
 * Data Structures
 *========================================================================*/

/**
 * Time bucket types
 *
 * Defines the granularity of time-based learning.
 */
typedef enum {
    EVOCORE_BUCKET_MINUTE,    /* 1-minute buckets */
    EVOCORE_BUCKET_HOUR,      /* 1-hour buckets */
    EVOCORE_BUCKET_DAY,       /* 1-day buckets */
    EVOCORE_BUCKET_WEEK,      /* 1-week buckets */
    EVOCORE_BUCKET_MONTH,     /* 1-month buckets */
    EVOCORE_BUCKET_YEAR       /* 1-year buckets */
} evocore_temporal_bucket_type_t;

/**
 * Time bucket data
 *
 * Stores weighted statistics for a specific time period.
 * A bucket is "complete" when the time period has finished.
 */
typedef struct {
    time_t start_time;              /* Bucket start timestamp */
    time_t end_time;                /* Bucket end timestamp */
    bool is_complete;               /* True if time period is finished */

    /* Per-parameter weighted statistics */
    evocore_weighted_array_t *stats; /* Array of weighted stats */
    size_t param_count;             /* Number of parameters */

    /* Metadata */
    size_t sample_count;            /* Number of observations */
    double avg_fitness;             /* Average fitness in this bucket */
    double best_fitness;            /* Best fitness in this bucket */
} evocore_temporal_bucket_t;

/**
 * Temporal bucket list
 *
 * Linked list of time buckets for a specific context.
 * Ordered chronologically.
 */
typedef struct {
    evocore_temporal_bucket_t *buckets;  /* Array of buckets */
    size_t count;                       /* Number of buckets */
    size_t capacity;                    /* Allocated capacity */
    evocore_temporal_bucket_type_t bucket_type; /* Time granularity */
} evocore_temporal_list_t;

/**
 * Context key for temporal system
 *
 * Maps a context string to its temporal bucket list.
 */
typedef struct {
    char *key;                       /* Context identifier */
    evocore_temporal_list_t *list;    /* Time buckets for this context */
} evocore_temporal_key_t;

/**
 * Temporal learning system
 *
 * Manages time-bucketed learning across all contexts.
 */
typedef struct {
    evocore_temporal_bucket_type_t bucket_type;
    size_t param_count;
    size_t retention_count;           /* How many time buckets to keep */
    void *internal;                   /* Internal hash table */
    time_t last_update;               /* Last learning timestamp */
} evocore_temporal_system_t;

/*========================================================================
 * System Management
 *========================================================================*/

/**
 * Create a temporal learning system
 *
 * @param bucket_type Time granularity for buckets
 * @param param_count Number of parameters to track
 * @param retention_count How many time buckets to keep per context
 * @return New temporal system, or NULL on error
 */
evocore_temporal_system_t* evocore_temporal_create(
    evocore_temporal_bucket_type_t bucket_type,
    size_t param_count,
    size_t retention_count
);

/**
 * Free temporal system
 *
 * @param system System to free
 */
void evocore_temporal_free(evocore_temporal_system_t *system);

/**
 * Get bucket duration in seconds
 *
 * @param bucket_type Bucket type
 * @return Duration in seconds
 */
time_t evocore_temporal_bucket_duration(evocore_temporal_bucket_type_t bucket_type);

/*========================================================================
 * Learning Operations
 *========================================================================*/

/**
 * Learn from timestamped experience
 *
 * Adds an observation to the appropriate time bucket for the context.
 * Creates new buckets as needed, removes old ones beyond retention.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param parameters Parameter array
 * @param param_count Number of parameters
 * @param fitness Fitness value
 * @param timestamp Observation timestamp
 * @return true on success
 */
bool evocore_temporal_learn(
    evocore_temporal_system_t *system,
    const char *context_key,
    const double *parameters,
    size_t param_count,
    double fitness,
    time_t timestamp
);

/**
 * Learn with current time
 *
 * Convenience function that uses time(NULL).
 */
bool evocore_temporal_learn_now(
    evocore_temporal_system_t *system,
    const char *context_key,
    const double *parameters,
    size_t param_count,
    double fitness
);

/*========================================================================
 * Organic Mean (Unbiased Learning)
 *========================================================================*/

/**
 * Get organic mean
 *
 * Computes the mean of bucket means, giving equal weight to each
 * time bucket regardless of sample count. This provides an unbiased
 * estimate that adapts to regime changes.
 *
 * Example: If January has 100 samples with mean=10 and February has
 * 10 samples with mean=20, the organic mean is 15 (not 10.9).
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_parameters Output parameter array
 * @param param_count Number of parameters
 * @param out_confidence Output confidence score (0-1), or NULL
 * @return true on success, false if insufficient data
 */
bool evocore_temporal_get_organic_mean(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double *out_confidence
);

/**
 * Get weighted mean (all samples)
 *
 * Computes the mean across all samples in all time buckets.
 * This is influenced by buckets with more samples.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_parameters Output parameter array
 * @param param_count Number of parameters
 * @return true on success
 */
bool evocore_temporal_get_weighted_mean(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count
);

/*========================================================================
 * Trend Analysis
 *========================================================================*/

/**
 * Get trend for each parameter
 *
 * Performs linear regression on bucket means over time to detect
 * increasing, decreasing, or stable trends.
 *
 * Returns slopes where:
 * - positive = increasing trend
 * - negative = decreasing trend
 * - near zero = stable
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_slopes Output slope array (one per parameter)
 * @param param_count Number of parameters
 * @return true on success, false if insufficient buckets
 */
bool evocore_temporal_get_trend(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_slopes,
    size_t param_count
);

/**
 * Get trend direction
 *
 * Simplified trend detection returning -1, 0, or +1.
 *
 * @param slope Trend slope value
 * @return -1 (decreasing), 0 (stable), +1 (increasing)
 */
int evocore_temporal_trend_direction(double slope);

/*========================================================================
 * Regime Change Detection
 *========================================================================*/

/**
 * Compare recent vs historical performance
 *
 * Detects if recent parameter performance differs significantly from
 * historical norms, indicating a possible regime change.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param recent_buckets Number of recent buckets to compare
 * @param param_count Number of parameters
 * @param out_drift Output drift amount per parameter
 * @return true on success, false if insufficient data
 */
bool evocore_temporal_compare_recent(
    const evocore_temporal_system_t *system,
    const char *context_key,
    size_t recent_buckets,
    double *out_drift,
    size_t param_count
);

/**
 * Detect regime change
 *
 * Returns true if recent performance deviates significantly
 * from historical norms.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param recent_buckets Number of recent buckets
 * @param threshold Drift threshold for regime change
 * @return true if regime change detected
 */
bool evocore_temporal_detect_regime_change(
    const evocore_temporal_system_t *system,
    const char *context_key,
    size_t recent_buckets,
    double threshold
);

/*========================================================================
 * Bucket Management
 *========================================================================*/

/**
 * Get bucket for a specific time
 *
 * Returns the time bucket that contains the given timestamp.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param timestamp Time to find bucket for
 * @param out_bucket Output bucket pointer
 * @return true on success
 */
bool evocore_temporal_get_bucket_at(
    const evocore_temporal_system_t *system,
    const char *context_key,
    time_t timestamp,
    evocore_temporal_bucket_t **out_bucket
);

/**
 * Get current bucket
 *
 * Returns the time bucket for the current time.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_bucket Output bucket pointer
 * @return true on success
 */
bool evocore_temporal_get_current_bucket(
    const evocore_temporal_system_t *system,
    const char *context_key,
    evocore_temporal_bucket_t **out_bucket
);

/**
 * Get bucket list
 *
 * Returns all time buckets for a context.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_list Output bucket list
 * @return true on success
 */
bool evocore_temporal_get_buckets(
    const evocore_temporal_system_t *system,
    const char *context_key,
    evocore_temporal_list_t **out_list
);

/**
 * Free bucket list
 *
 * @param list List to free
 */
void evocore_temporal_free_list(evocore_temporal_list_t *list);

/*========================================================================
 * Sampling
 *========================================================================*/

/**
 * Sample using organic mean
 *
 * Samples from the organic (unbiased) parameter distribution.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_parameters Output parameter array
 * @param param_count Number of parameters
 * @param exploration_factor 0=pure exploit, 1=pure explore
 * @param seed Random seed pointer
 * @return true on success
 */
bool evocore_temporal_sample_organic(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double exploration_factor,
    unsigned int *seed
);

/**
 * Sample with trend following
 *
 * Samples parameters biased toward the detected trend direction.
 * Useful for adapting to directional market changes.
 *
 * @param system Temporal system
 * @param context_key Context identifier
 * @param out_parameters Output parameter array
 * @param param_count Number of parameters
 * @param trend_strength How much to follow trend (0-1)
 * @param seed Random seed pointer
 * @return true on success
 */
bool evocore_temporal_sample_trend(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double trend_strength,
    unsigned int *seed
);

/*========================================================================
 * Persistence
 *========================================================================*/

/**
 * Save temporal system to JSON
 *
 * @param system Temporal system
 * @param filepath Output file path
 * @return true on success
 */
bool evocore_temporal_save_json(
    const evocore_temporal_system_t *system,
    const char *filepath
);

/**
 * Load temporal system from JSON
 *
 * @param filepath Input file path
 * @param out_system Output temporal system
 * @return true on success
 */
bool evocore_temporal_load_json(
    const char *filepath,
    evocore_temporal_system_t **out_system
);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Get total bucket count across all contexts
 *
 * @param system Temporal system
 * @return Total number of buckets stored
 */
size_t evocore_temporal_bucket_count(const evocore_temporal_system_t *system);

/**
 * Get number of contexts tracked
 *
 * @param system Temporal system
 * @return Number of unique context keys
 */
size_t evocore_temporal_context_count(const evocore_temporal_system_t *system);

/**
 * Clear old buckets
 *
 * Removes buckets older than the retention period.
 *
 * @param system Temporal system
 * @return Number of buckets removed
 */
size_t evocore_temporal_prune_old(evocore_temporal_system_t *system);

/**
 * Reset a context
 *
 * Clears all temporal learning for a specific context.
 *
 * @param system Temporal system
 * @param context_key Context to reset
 * @return true on success
 */
bool evocore_temporal_reset_context(
    evocore_temporal_system_t *system,
    const char *context_key
);

/**
 * Reset entire system
 *
 * Clears all temporal data.
 *
 * @param system Temporal system
 */
void evocore_temporal_reset_all(evocore_temporal_system_t *system);

#endif /* EVOCORE_TEMPORAL_H */
