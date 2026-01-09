/**
 * Evocore Negative Learning Implementation
 *
 * Systematic tracking and avoidance of failed parameter combinations.
 */

#define _GNU_SOURCE
#include "evocore/negative.h"
#include "internal.h"
#include "evocore/log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

/*========================================================================
 * Constants
 *========================================================================*/

#define EVOCORE_DEFAULT_CAPACITY 1000
#define EVOCORE_DEFAULT_BASE_PENALTY 0.5
#define EVOCORE_DEFAULT_DECAY_RATE 0.05
#define EVOCORE_DEFAULT_REPEAT_MULTIPLIER 1.5
#define EVOCORE_DEFAULT_SIMILARITY_THRESHOLD 0.8

/* Default severity thresholds (fitness values) */
#define EVOCORE_DEFAULT_MILD_THRESHOLD -0.10
#define EVOCORE_DEFAULT_MODERATE_THRESHOLD -0.25
#define EVOCORE_DEFAULT_SEVERE_THRESHOLD -0.50
#define EVOCORE_DEFAULT_FATAL_THRESHOLD -0.90

/*========================================================================
 * Severity Helpers
 *========================================================================*/

const char* evocore_severity_string(evocore_failure_severity_t severity) {
    switch (severity) {
        case EVOCORE_FAILURE_NONE:    return "NONE";
        case EVOCORE_FAILURE_MILD:     return "MILD";
        case EVOCORE_FAILURE_MODERATE: return "MODERATE";
        case EVOCORE_FAILURE_SEVERE:   return "SEVERE";
        case EVOCORE_FAILURE_FATAL:    return "FATAL";
        default:                      return "UNKNOWN";
    }
}

evocore_failure_severity_t evocore_severity_from_string(const char *str) {
    if (!str) return EVOCORE_FAILURE_NONE;

    if (strcasecmp(str, "mild") == 0) return EVOCORE_FAILURE_MILD;
    if (strcasecmp(str, "moderate") == 0) return EVOCORE_FAILURE_MODERATE;
    if (strcasecmp(str, "severe") == 0) return EVOCORE_FAILURE_SEVERE;
    if (strcasecmp(str, "fatal") == 0) return EVOCORE_FAILURE_FATAL;

    return EVOCORE_FAILURE_NONE;
}

void evocore_severity_color(
    evocore_failure_severity_t severity,
    int *r_out,
    int *g_out,
    int *b_out
) {
    switch (severity) {
        case EVOCORE_FAILURE_NONE:
            *r_out = 128; *g_out = 128; *b_out = 128;
            break;
        case EVOCORE_FAILURE_MILD:
            *r_out = 200; *g_out = 200; *b_out = 100;  /* Yellow-green */
            break;
        case EVOCORE_FAILURE_MODERATE:
            *r_out = 255; *g_out = 200; *b_out = 50;   /* Orange */
            break;
        case EVOCORE_FAILURE_SEVERE:
            *r_out = 255; *g_out = 100; *b_out = 50;   /* Red-orange */
            break;
        case EVOCORE_FAILURE_FATAL:
            *r_out = 255; *g_out = 50; *b_out = 50;    /* Red */
            break;
        default:
            *r_out = 128; *g_out = 128; *b_out = 128;
            break;
    }
}

evocore_failure_severity_t evocore_classify_failure(
    double fitness,
    const double thresholds[4]
) {
    if (!thresholds) {
        /* Use defaults */
        if (fitness <= EVOCORE_DEFAULT_FATAL_THRESHOLD) return EVOCORE_FAILURE_FATAL;
        if (fitness <= EVOCORE_DEFAULT_SEVERE_THRESHOLD) return EVOCORE_FAILURE_SEVERE;
        if (fitness <= EVOCORE_DEFAULT_MODERATE_THRESHOLD) return EVOCORE_FAILURE_MODERATE;
        if (fitness <= EVOCORE_DEFAULT_MILD_THRESHOLD) return EVOCORE_FAILURE_MILD;
        return EVOCORE_FAILURE_NONE;
    }

    if (fitness <= thresholds[3]) return EVOCORE_FAILURE_FATAL;
    if (fitness <= thresholds[2]) return EVOCORE_FAILURE_SEVERE;
    if (fitness <= thresholds[1]) return EVOCORE_FAILURE_MODERATE;
    if (fitness <= thresholds[0]) return EVOCORE_FAILURE_MILD;
    return EVOCORE_FAILURE_NONE;
}

/*========================================================================
 * Internal Helpers
 *========================================================================*/

/**
 * Calculate genome similarity (0.0 to 1.0)
 * Based on normalized Hamming distance
 */
static double genome_similarity(const evocore_genome_t *a, const evocore_genome_t *b) {
    if (!a || !b) return 0.0;
    if (!a->data || !b->data) return 0.0;

    /* Use smaller size for comparison */
    size_t min_size = a->size < b->size ? a->size : b->size;

    if (min_size == 0) return 0.0;

    /* Count matching bytes */
    size_t matching = 0;
    for (size_t i = 0; i < min_size; i++) {
        if (((unsigned char*)a->data)[i] == ((unsigned char*)b->data)[i]) {
            matching++;
        }
    }

    return (double)matching / min_size;
}

/**
 * Calculate initial penalty from severity
 */
static double severity_to_penalty(evocore_failure_severity_t severity) {
    switch (severity) {
        case EVOCORE_FAILURE_MILD:     return 0.2;
        case EVOCORE_FAILURE_MODERATE: return 0.4;
        case EVOCORE_FAILURE_SEVERE:   return 0.7;
        case EVOCORE_FAILURE_FATAL:    return 0.95;
        default:                       return 0.0;
    }
}

/**
 * Create a new failure record
 */
static evocore_failure_record_t* failure_record_create(
    const evocore_genome_t *genome,
    double fitness,
    evocore_failure_severity_t severity,
    int generation
) {
    evocore_failure_record_t *record = evocore_calloc(1, sizeof(evocore_failure_record_t));
    if (!record) return NULL;

    /* Clone genome */
    record->genome = evocore_calloc(1, sizeof(evocore_genome_t));
    if (!record->genome) {
        evocore_free(record);
        return NULL;
    }

    evocore_error_t err = evocore_genome_clone(genome, record->genome);
    if (err != EVOCORE_OK) {
        evocore_free(record->genome);
        evocore_free(record);
        return NULL;
    }

    record->fitness = fitness;
    record->severity = severity;
    record->generation = generation;
    record->penalty_score = severity_to_penalty(severity);
    record->repeat_count = 1;
    record->first_seen = time(NULL);
    record->last_seen = record->first_seen;
    record->is_active = true;

    return record;
}

/**
 * Free a failure record's internal memory
 * Does NOT free the record struct itself - use for array elements
 */
static void failure_record_clear(evocore_failure_record_t *record) {
    if (!record) return;

    if (record->genome) {
        if (record->genome->owns_memory && record->genome->data) {
            evocore_free(record->genome->data);
            record->genome->data = NULL;
        }
        evocore_free(record->genome);
        record->genome = NULL;
    }
}

/*========================================================================
 * Lifecycle Functions
 *========================================================================*/

evocore_error_t evocore_negative_learning_init(
    evocore_negative_learning_t *neg,
    size_t capacity,
    double base_penalty,
    double decay_rate
) {
    if (!neg) return EVOCORE_ERR_NULL_PTR;

    memset(neg, 0, sizeof(evocore_negative_learning_t));

    if (capacity == 0) capacity = EVOCORE_DEFAULT_CAPACITY;

    neg->failures = evocore_calloc(capacity, sizeof(evocore_failure_record_t));
    if (!neg->failures) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    neg->capacity = capacity;
    neg->count = 0;
    neg->base_penalty = base_penalty;
    neg->decay_rate = decay_rate;
    neg->repeat_multiplier = EVOCORE_DEFAULT_REPEAT_MULTIPLIER;
    neg->similarity_threshold = EVOCORE_DEFAULT_SIMILARITY_THRESHOLD;
    neg->current_generation = 0;
    neg->last_cleanup = time(NULL);

    /* Set default thresholds */
    neg->thresholds[0] = EVOCORE_DEFAULT_MILD_THRESHOLD;
    neg->thresholds[1] = EVOCORE_DEFAULT_MODERATE_THRESHOLD;
    neg->thresholds[2] = EVOCORE_DEFAULT_SEVERE_THRESHOLD;
    neg->thresholds[3] = EVOCORE_DEFAULT_FATAL_THRESHOLD;

    return EVOCORE_OK;
}

evocore_error_t evocore_negative_learning_init_default(
    evocore_negative_learning_t *neg,
    size_t capacity
) {
    return evocore_negative_learning_init(
        neg,
        capacity,
        EVOCORE_DEFAULT_BASE_PENALTY,
        EVOCORE_DEFAULT_DECAY_RATE
    );
}

evocore_error_t evocore_negative_learning_set_thresholds(
    evocore_negative_learning_t *neg,
    double mild,
    double moderate,
    double severe,
    double fatal
) {
    if (!neg) return EVOCORE_ERR_NULL_PTR;

    neg->thresholds[0] = mild;
    neg->thresholds[1] = moderate;
    neg->thresholds[2] = severe;
    neg->thresholds[3] = fatal;

    return EVOCORE_OK;
}

void evocore_negative_learning_cleanup(evocore_negative_learning_t *neg) {
    if (!neg) return;

    for (size_t i = 0; i < neg->count; i++) {
        failure_record_clear(&neg->failures[i]);
    }

    evocore_free(neg->failures);

    neg->failures = NULL;
    neg->capacity = 0;
    neg->count = 0;
}

/*========================================================================
 * Recording Functions
 *========================================================================*/

evocore_error_t evocore_negative_learning_record_failure(
    evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double fitness,
    int generation
) {
    if (!neg || !genome) return EVOCORE_ERR_NULL_PTR;

    /* Classify severity */
    evocore_failure_severity_t severity = evocore_classify_failure(fitness, neg->thresholds);

    return evocore_negative_learning_record_failure_severity(neg, genome, fitness, severity, generation);
}

evocore_error_t evocore_negative_learning_record_failure_severity(
    evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double fitness,
    evocore_failure_severity_t severity,
    int generation
) {
    if (!neg || !genome) return EVOCORE_ERR_NULL_PTR;
    if (severity == EVOCORE_FAILURE_NONE) return EVOCORE_OK;  /* Not a failure */

    /* Update generation */
    neg->current_generation = generation;

    /* Check for similar existing failure */
    double best_similarity = 0.0;
    size_t best_index = neg->count;  /* Default to new */

    for (size_t i = 0; i < neg->count; i++) {
        if (!neg->failures[i].is_active) continue;

        double sim = genome_similarity(genome, neg->failures[i].genome);
        if (sim > best_similarity) {
            best_similarity = sim;
            best_index = i;
        }
    }

    /* If similar enough, update existing record */
    if (best_index < neg->count && best_similarity >= neg->similarity_threshold) {
        evocore_failure_record_t *record = &neg->failures[best_index];

        record->repeat_count++;
        record->last_seen = time(NULL);
        record->generation = generation;

        /* Increase penalty based on repeat */
        double penalty_increase = neg->repeat_multiplier * (double)record->repeat_count / 10.0;
        record->penalty_score = fmin(1.0, record->penalty_score + penalty_increase);

        /* Update to worst fitness seen */
        if (fitness < record->fitness) {
            record->fitness = fitness;
            /* Potentially increase severity if worse */
            evocore_failure_severity_t new_severity = evocore_classify_failure(fitness, neg->thresholds);
            if (new_severity > record->severity) {
                record->severity = new_severity;
            }
        }

        evocore_log_debug("Updated failure: similarity=%.2f, repeat=%d, penalty=%.2f",
                          best_similarity, record->repeat_count, record->penalty_score);

        return EVOCORE_OK;
    }

    /* Create new failure record */
    if (neg->count >= neg->capacity) {
        /* Try to prune first to make room */
        evocore_negative_learning_prune(neg, 0.01, 100);

        if (neg->count >= neg->capacity) {
            evocore_log_warn("Negative learning at capacity, cannot record failure");
            return EVOCORE_ERR_POP_FULL;
        }
    }

    evocore_failure_record_t *record = failure_record_create(genome, fitness, severity, generation);
    if (!record) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    neg->failures[neg->count++] = *record;
    /* Clear the genome pointer in the temp record so it's not freed */
    record->genome = NULL;
    evocore_free(record);  /* Free the temporary struct */

    evocore_log_debug("Recorded new failure: severity=%s, penalty=%.2f",
                      evocore_severity_string(severity), neg->failures[neg->count - 1].penalty_score);

    return EVOCORE_OK;
}

void evocore_negative_learning_set_generation(
    evocore_negative_learning_t *neg,
    int generation
) {
    if (!neg) return;

    /* Calculate generations passed for decay */
    int generations_passed = generation - neg->current_generation;
    if (generations_passed > 0) {
        evocore_negative_learning_decay(neg, generations_passed);
    }

    neg->current_generation = generation;
}

/*========================================================================
 * Query Functions
 *========================================================================*/

evocore_error_t evocore_negative_learning_check_penalty(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double *penalty_out
) {
    if (!neg || !genome || !penalty_out) return EVOCORE_ERR_NULL_PTR;

    *penalty_out = 0.0;

    double max_weighted_penalty = 0.0;

    for (size_t i = 0; i < neg->count; i++) {
        evocore_failure_record_t *record = &neg->failures[i];
        if (!record->is_active) continue;

        double similarity = genome_similarity(genome, record->genome);
        if (similarity < neg->similarity_threshold) continue;

        /* Weight penalty by similarity */
        double weighted_penalty = record->penalty_score * similarity;
        if (weighted_penalty > max_weighted_penalty) {
            max_weighted_penalty = weighted_penalty;
        }
    }

    *penalty_out = max_weighted_penalty;
    return EVOCORE_OK;
}

bool evocore_negative_learning_is_forbidden(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double threshold
) {
    if (!neg || !genome) return false;

    double penalty = 0.0;
    evocore_negative_learning_check_penalty(neg, genome, &penalty);

    return penalty >= threshold;
}

evocore_error_t evocore_negative_learning_adjust_fitness(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    double raw_fitness,
    double *adjusted_out
) {
    if (!neg || !genome || !adjusted_out) return EVOCORE_ERR_NULL_PTR;

    double penalty = 0.0;
    evocore_negative_learning_check_penalty(neg, genome, &penalty);

    /* Apply penalty: reduce fitness by penalty amount */
    *adjusted_out = raw_fitness * (1.0 - penalty);

    return EVOCORE_OK;
}

evocore_error_t evocore_negative_learning_find_similar(
    const evocore_negative_learning_t *neg,
    const evocore_genome_t *genome,
    evocore_failure_record_t **failure_out,
    double *similarity_out
) {
    if (!neg || !genome) return EVOCORE_ERR_NULL_PTR;
    if (neg->count == 0) return EVOCORE_ERR_POP_EMPTY;

    *failure_out = NULL;
    if (similarity_out) *similarity_out = 0.0;

    double best_similarity = 0.0;
    evocore_failure_record_t *best_record = NULL;

    for (size_t i = 0; i < neg->count; i++) {
        evocore_failure_record_t *record = &neg->failures[i];
        if (!record->is_active) continue;

        double sim = genome_similarity(genome, record->genome);
        if (sim > best_similarity) {
            best_similarity = sim;
            best_record = record;
        }
    }

    if (best_record && best_similarity >= neg->similarity_threshold) {
        *failure_out = best_record;
        if (similarity_out) *similarity_out = best_similarity;
        return EVOCORE_OK;
    }

    return EVOCORE_ERR_UNKNOWN;  /* No similar failure found */
}

/*========================================================================
 * Maintenance Functions
 *========================================================================*/

void evocore_negative_learning_decay(
    evocore_negative_learning_t *neg,
    int generations_passed
) {
    if (!neg || generations_passed <= 0) return;

    double decay_factor = exp(-neg->decay_rate * generations_passed);

    for (size_t i = 0; i < neg->count; i++) {
        evocore_failure_record_t *record = &neg->failures[i];

        record->penalty_score *= decay_factor;

        /* Mark as inactive if penalty too low */
        if (record->penalty_score < 0.05) {
            record->is_active = false;
        }
    }

    evocore_log_debug("Decayed penalties: generations=%d, factor=%.4f",
                      generations_passed, decay_factor);
}

size_t evocore_negative_learning_prune(
    evocore_negative_learning_t *neg,
    double min_penalty,
    int max_age_generations
) {
    if (!neg) return 0;

    time_t now = time(NULL);
    size_t pruned = 0;

    /* Compact array by removing pruned entries */
    size_t write_idx = 0;
    for (size_t read_idx = 0; read_idx < neg->count; read_idx++) {
        evocore_failure_record_t *record = &neg->failures[read_idx];

        bool should_prune = false;

        /* Prune if penalty too low */
        if (record->penalty_score < min_penalty) {
            should_prune = true;
        }

        /* Prune if too old */
        if (max_age_generations > 0) {
            int age = neg->current_generation - record->generation;
            if (age > max_age_generations) {
                should_prune = true;
            }
        }

        if (should_prune) {
            failure_record_clear(record);
            pruned++;
        } else {
            neg->failures[write_idx++] = *record;
        }
    }

    neg->count = write_idx;
    neg->last_cleanup = now;

    if (pruned > 0) {
        evocore_log_debug("Pruned %zu failure records", pruned);
    }

    return pruned;
}

evocore_error_t evocore_negative_learning_stats(
    const evocore_negative_learning_t *neg,
    evocore_negative_stats_t *stats_out
) {
    if (!neg || !stats_out) return EVOCORE_ERR_NULL_PTR;

    memset(stats_out, 0, sizeof(evocore_negative_stats_t));

    stats_out->total_count = neg->count;

    double penalty_sum = 0.0;
    double max_penalty = 0.0;

    for (size_t i = 0; i < neg->count; i++) {
        evocore_failure_record_t *record = &neg->failures[i];

        if (record->is_active) {
            stats_out->active_count++;
            penalty_sum += record->penalty_score;
            if (record->penalty_score > max_penalty) {
                max_penalty = record->penalty_score;
            }
        }

        switch (record->severity) {
            case EVOCORE_FAILURE_MILD:     stats_out->mild_count++; break;
            case EVOCORE_FAILURE_MODERATE: stats_out->moderate_count++; break;
            case EVOCORE_FAILURE_SEVERE:   stats_out->severe_count++; break;
            case EVOCORE_FAILURE_FATAL:    stats_out->fatal_count++; break;
            default: break;
        }

        if (record->repeat_count > 1) {
            stats_out->repeat_victims++;
        }
    }

    stats_out->max_penalty = max_penalty;

    if (stats_out->active_count > 0) {
        stats_out->avg_penalty = penalty_sum / stats_out->active_count;
    }

    return EVOCORE_OK;
}

size_t evocore_negative_learning_count(const evocore_negative_learning_t *neg) {
    return neg ? neg->count : 0;
}

size_t evocore_negative_learning_active_count(const evocore_negative_learning_t *neg) {
    if (!neg) return 0;

    size_t active = 0;
    for (size_t i = 0; i < neg->count; i++) {
        if (neg->failures[i].is_active) {
            active++;
        }
    }
    return active;
}

void evocore_negative_learning_clear(evocore_negative_learning_t *neg) {
    if (!neg) return;

    for (size_t i = 0; i < neg->count; i++) {
        failure_record_clear(&neg->failures[i]);
    }

    neg->count = 0;
}

/*========================================================================
 * Configuration Functions
 *========================================================================*/

void evocore_negative_learning_set_base_penalty(
    evocore_negative_learning_t *neg,
    double base_penalty
) {
    if (neg) {
        neg->base_penalty = fmax(0.0, fmin(1.0, base_penalty));
    }
}

void evocore_negative_learning_set_repeat_multiplier(
    evocore_negative_learning_t *neg,
    double multiplier
) {
    if (neg) {
        neg->repeat_multiplier = fmax(1.0, multiplier);
    }
}

void evocore_negative_learning_set_decay_rate(
    evocore_negative_learning_t *neg,
    double decay_rate
) {
    if (neg) {
        neg->decay_rate = fmax(0.0, fmin(1.0, decay_rate));
    }
}

void evocore_negative_learning_set_similarity_threshold(
    evocore_negative_learning_t *neg,
    double threshold
) {
    if (neg) {
        neg->similarity_threshold = fmax(0.0, fmin(1.0, threshold));
    }
}
