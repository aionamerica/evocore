/**
 * Evocore Negative Learning Module
 *
 * Systematic tracking and avoidance of parameter combinations that fail.
 * Complements positive learning (what works) with negative learning (what doesn't).
 *
 * Key Concepts:
 * - Failure Records: Stores failed genomes with severity classification
 * - Penalties: Calculated based on severity and repeat occurrences
 * - Decay: Penalties decrease over time/generations
 * - Similarity Matching: Checks if new genomes match known failure patterns
 *
 * Example: Trading system failures
 * - Severity: MILD (< -10%), MODERATE (< -25%), SEVERE (< -50%), FATAL (< -90%)
 * - Penalty: 0.0 = no penalty, 1.0 = maximum penalty
 * - Decay: penalty *= exp(-decay_rate * generations)
 */

#ifndef EVOCORE_NEGATIVE_H
#define EVOCORE_NEGATIVE_H

#include "evocore/genome.h"
#include "evocore/error.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/*========================================================================
 * Failure Severity Classification
 *========================================================================*/

/**
 * Failure severity levels based on fitness/return values
 *
 * Thresholds are configurable but default to:
 * - MILD: Fitness < -10% (poor performance)
 * - MODERATE: Fitness < -25% (significantly below average)
 * - SEVERE: Fitness < -50% (catastrophic loss)
 * - FATAL: Fitness < -90% (complete blowout)
 */
typedef enum {
    EVOCORE_FAILURE_NONE = 0,       /* No failure */
    EVOCORE_FAILURE_MILD = 1,       /* Poor performance */
    EVOCORE_FAILURE_MODERATE = 2,   /* Significantly below average */
    EVOCORE_FAILURE_SEVERE = 3,     /* Catastrophic failure */
    EVOCORE_FAILURE_FATAL = 4       /* Complete blowout */
} evocore_failure_severity_t;

/**
 * Convert fitness value to severity level
 *
 * @param fitness Fitness value (typically negative for trading)
 * @param thresholds Array of 4 thresholds [mild, moderate, severe, fatal]
 * @return Severity level
 */
evocore_failure_severity_t evocore_classify_failure(
    double fitness,
    const double thresholds[4]
);

/*========================================================================
 * Data Structures
 *========================================================================*/

/**
 * Failure record
 *
 * Stores a single failed genome pattern with metadata.
 */
typedef struct {
    evocore_genome_t *genome;           /* Failed genome (owned) */
    double fitness;                      /* Fitness value that caused failure */
    evocore_failure_severity_t severity; /* Classification */
    int generation;                      /* Generation when recorded */
    double penalty_score;                /* Current penalty (0.0-1.0) */
    int repeat_count;                    /* Times similar failure seen */
    time_t first_seen;                   /* First occurrence timestamp */
    time_t last_seen;                    /* Most recent occurrence */
    bool is_active;                      /* Currently being penalized */
} evocore_failure_record_t;

/**
 * Negative learning state
 *
 * Manages collection of failure records with penalty calculation.
 */
typedef struct {
    evocore_failure_record_t *failures; /* Failure records array */
    size_t capacity;                     /* Max failures stored */
    size_t count;                        /* Current count */

    /* Penalty configuration */
    double base_penalty;                 /* Base penalty multiplier (0.0-1.0) */
    double repeat_multiplier;            /* Penalty increase per repeat */
    double decay_rate;                   /* Penalty decay per generation */

    /* Severity thresholds (fitness values) */
    double thresholds[4];                /* [mild, moderate, severe, fatal] */
    double similarity_threshold;         /* Genome similarity for matching (0.0-1.0) */

    time_t last_cleanup;                 /* Last pruning timestamp */
    int current_generation;              /* Current generation for decay calc */
} evocore_negative_learning_t;

/**
 * Negative learning statistics
 *
 * Snapshot of negative learning state for monitoring.
 * Note: typedef name matches forward declaration in context.h
 */
typedef struct evocore_negative_stats_s {
    size_t total_count;      /* Total failures stored */
    size_t active_count;     /* Currently active (penalty > 0.1) */
    size_t mild_count;       /* MILD severity count */
    size_t moderate_count;   /* MODERATE severity count */
    size_t severe_count;     /* SEVERE severity count */
    size_t fatal_count;      /* FATAL severity count */
    double avg_penalty;      /* Average penalty across all failures */
    double max_penalty;      /* Maximum penalty */
    size_t repeat_victims;   /* Failures with repeat_count > 1 */
} evocore_negative_stats_t;  /* typedef alias for compatibility */

/*========================================================================
 * Lifecycle Functions
 *========================================================================*/

/**
 * Initialize negative learning system
 *
 * @param neg Negative learning state to initialize
 * @param capacity Maximum failures to store
 * @param base_penalty Base penalty multiplier (0.0-1.0)
 * @param decay_rate Penalty decay per generation (0.0-1.0)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_init(
    evocore_negative_learning_t *neg,
    size_t capacity,
    double base_penalty,
    double decay_rate
);

/**
 * Initialize with default values
 *
 * @param neg Negative learning state to initialize
 * @param capacity Maximum failures to store
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_init_default(
    evocore_negative_learning_t *neg,
    size_t capacity
);

/**
 * Set severity thresholds
 *
 * @param neg Negative learning state
 * @param mild Fitness threshold for MILD (e.g., -0.10)
 * @param moderate Fitness threshold for MODERATE (e.g., -0.25)
 * @param severe Fitness threshold for SEVERE (e.g., -0.50)
 * @param fatal Fitness threshold for FATAL (e.g., -0.90)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_set_thresholds(
    evocore_negative_learning_t *neg,
    double mild,
    double moderate,
    double severe,
    double fatal
);

/**
 * Cleanup and free resources
 *
 * Frees all failure records and resets state.
 *
 * @param neg Negative learning state to cleanup
 */
void evocore_negative_learning_cleanup(
    evocore_negative_learning_t *neg
);

/*========================================================================
 * Recording Functions
 *========================================================================*/

/**
 * Record a failure outcome
 *
 * Classifies severity based on configured thresholds.
 * If a similar failure exists, updates repeat count and penalty.
 * Otherwise, creates a new failure record.
 *
 * @param neg Negative learning state
 * @param genome Genome that failed (will be cloned)
 * @param fitness Fitness value (typically negative)
 * @param generation Current generation
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_record_failure(
    evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double fitness,
    int generation
);

/**
 * Record a failure with explicit severity
 *
 * Bypasses automatic classification and uses provided severity.
 *
 * @param neg Negative learning state
 * @param genome Genome that failed (will be cloned)
 * @param fitness Fitness value
 * @param severity Pre-classified severity
 * @param generation Current generation
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_record_failure_severity(
    evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double fitness,
    evocore_failure_severity_t severity,
    int generation
);

/**
 * Update current generation (for decay calculations)
 *
 * @param neg Negative learning state
 * @param generation Current generation number
 */
void evocore_negative_learning_set_generation(
    evocore_negative_learning_t *neg,
    int generation
);

/*========================================================================
 * Query Functions
 *========================================================================*/

/**
 * Check if a genome matches any known failure pattern
 *
 * Calculates similarity against all stored failures and returns
 * the highest weighted penalty based on similarity and severity.
 *
 * @param neg Negative learning state
 * @param genome Genome to check
 * @param penalty_out Output: penalty score (0.0 = no penalty, 1.0 = max)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_check_penalty(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double *penalty_out
);

/**
 * Check if a genome should be forbidden from sampling
 *
 * Returns true if penalty exceeds the given threshold.
 *
 * @param neg Negative learning state
 * @param genome Genome to check
 * @param threshold Penalty threshold (typically 0.5-0.8)
 * @return true if genome should be forbidden, false otherwise
 */
bool evocore_negative_learning_is_forbidden(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double threshold
);

/**
 * Get penalty-adjusted fitness
 *
 * Returns fitness reduced by penalty: fitness * (1 - penalty)
 *
 * @param neg Negative learning state
 * @param genome Genome to check
 * @param raw_fitness Raw fitness value
 * @param adjusted_out Output: penalty-adjusted fitness
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_adjust_fitness(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double raw_fitness,
    double *adjusted_out
);

/**
 * Find most similar failure
 *
 * Returns the failure record most similar to the given genome.
 *
 * @param neg Negative learning state
 * @param genome Genome to compare
 * @param failure_out Output: matching failure record
 * @param similarity_out Output: similarity score (0.0-1.0)
 * @return EVOCORE_OK if match found, EVOCORE_ERR_NOT_FOUND if none
 */
evocore_error_t evocore_negative_learning_find_similar(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    evocore_failure_record_t **failure_out,
    double *similarity_out
);

/*========================================================================
 * Maintenance Functions
 *========================================================================*/

/**
 * Decay penalties based on age and generation
 *
 * Applies exponential decay: penalty *= exp(-decay_rate * generations)
 * Marks failures as inactive if penalty drops below threshold.
 *
 * @param neg Negative learning state
 * @param generations_passed Number of generations that passed
 */
void evocore_negative_learning_decay(
    evocore_negative_learning_t *neg,
    int generations_passed
);

/**
 * Prune old/inactive failures
 *
 * Removes failures that are:
 * - Inactive (penalty < threshold)
 * - Older than max_age_generations
 * - Below minimum penalty threshold
 *
 * @param neg Negative learning state
 * @param min_penalty Minimum penalty to keep (default 0.05)
 * @param max_age_generations Maximum age in generations (0 = no limit)
 * @return Number of failures pruned
 */
size_t evocore_negative_learning_prune(
    evocore_negative_learning_t *neg,
    double min_penalty,
    int max_age_generations
);

/**
 * Get statistics about stored failures
 *
 * @param neg Negative learning state
 * @param stats_out Output: statistics snapshot
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_negative_learning_stats(
    const evocore_negative_learning_t *neg,
    evocore_negative_stats_t *stats_out
);

/**
 * Get failure count
 *
 * @param neg Negative learning state
 * @return Number of stored failures
 */
size_t evocore_negative_learning_count(
    const evocore_negative_learning_t *neg
);

/**
 * Get active failure count
 *
 * @param neg Negative learning state
 * @return Number of failures with penalty > 0.1
 */
size_t evocore_negative_learning_active_count(
    const evocore_negative_learning_t *neg
);

/**
 * Clear all failures
 *
 * Removes all stored failure records.
 *
 * @param neg Negative learning state
 */
void evocore_negative_learning_clear(
    evocore_negative_learning_t *neg
);

/*========================================================================
 * Configuration
 *========================================================================*/

/**
 * Set base penalty multiplier
 *
 * @param neg Negative learning state
 * @param base_penalty Base penalty (0.0-1.0)
 */
void evocore_negative_learning_set_base_penalty(
    evocore_negative_learning_t *neg,
    double base_penalty
);

/**
 * Set repeat multiplier
 *
 * Controls how much penalty increases for repeated failures.
 *
 * @param neg Negative learning state
 * @param multiplier Repeat multiplier (typically 1.1-2.0)
 */
void evocore_negative_learning_set_repeat_multiplier(
    evocore_negative_learning_t *neg,
    double multiplier
);

/**
 * Set decay rate
 *
 * Controls how fast penalties decay over generations.
 *
 * @param neg Negative learning state
 * @param decay_rate Decay rate (0.0 = no decay, 1.0 = instant decay)
 */
void evocore_negative_learning_set_decay_rate(
    evocore_negative_learning_t *neg,
    double decay_rate
);

/**
 * Set similarity threshold
 *
 * Genomes with similarity below this threshold are considered
 * distinct failures.
 *
 * @param neg Negative learning state
 * @param threshold Similarity threshold (0.0-1.0)
 */
void evocore_negative_learning_set_similarity_threshold(
    evocore_negative_learning_t *neg,
    double threshold
);

/*========================================================================
 * Severity String Helpers
 *========================================================================*/

/**
 * Convert severity to string
 *
 * @param severity Severity level
 * @return String representation
 */
const char* evocore_severity_string(evocore_failure_severity_t severity);

/**
 * Convert string to severity
 *
 * @param str String representation ("mild", "moderate", "severe", "fatal")
 * @return Severity level, or EVOCORE_FAILURE_NONE if invalid
 */
evocore_failure_severity_t evocore_severity_from_string(const char *str);

/**
 * Get color code for severity (for UI)
 *
 * Returns RGB values as 0-255.
 *
 * @param severity Severity level
 * @param r_out Output: red component
 * @param g_out Output: green component
 * @param b_out Output: blue component
 */
void evocore_severity_color(
    evocore_failure_severity_t severity,
    int *r_out,
    int *g_out,
    int *b_out
);

#endif /* EVOCORE_NEGATIVE_H */
