#define _GNU_SOURCE
#include "evocore/meta.h"
#include "internal.h"
#include "evocore/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*========================================================================
 * Adaptive Parameter Adjustment
 *========================================================================*/

/**
 * Calculate statistics from an array of doubles
 */
typedef struct {
    double mean;
    double stddev;
    double min;
    double max;
    double trend;  /* Linear slope */
} stats_t;

static void calculate_stats(const double *values, size_t count, stats_t *stats) {
    if (values == NULL || count == 0 || stats == NULL) {
        memset(stats, 0, sizeof(stats_t));
        return;
    }

    /* Calculate mean, min, max */
    double sum = 0.0;
    stats->min = values[0];
    stats->max = values[0];

    for (size_t i = 0; i < count; i++) {
        sum += values[i];
        if (values[i] < stats->min) stats->min = values[i];
        if (values[i] > stats->max) stats->max = values[i];
    }
    stats->mean = sum / (double)count;

    /* Calculate standard deviation */
    double variance = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = values[i] - stats->mean;
        variance += diff * diff;
    }
    stats->stddev = sqrt(variance / (double)count);

    /* Calculate linear trend (simple regression) */
    if (count >= 2) {
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        for (size_t i = 0; i < count; i++) {
            double x = (double)i;
            double y = values[i];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
        }
        stats->trend = (count * sum_xy - sum_x * sum_y) /
                       (count * sum_x2 - sum_x * sum_x);
    } else {
        stats->trend = 0.0;
    }
}

void evocore_meta_adapt(evocore_meta_params_t *params,
                       const double *recent_fitness,
                       size_t count,
                       bool improvement) {
    if (params == NULL || recent_fitness == NULL || count == 0) {
        return;
    }

    stats_t stats;
    calculate_stats(recent_fitness, count, &stats);

    double learning_rate = params->meta_learning_rate;

    /* Adapt based on improvement trend */
    if (improvement && stats.trend > 0) {
        /* Things are going well - fine-tune */

        /* Reduce exploration slightly to exploit */
        params->exploration_factor *= (1.0 - learning_rate * 0.1);
        params->exploration_factor = fmax(0.1, params->exploration_factor);

        /* Reduce mutation rates for fine-tuning */
        params->optimization_mutation_rate *= (1.0 - learning_rate * 0.2);
        params->optimization_mutation_rate = fmax(0.01, params->optimization_mutation_rate);

        /* Increase selection pressure to converge */
        params->elite_protection_ratio *= (1.0 + learning_rate * 0.1);
        params->elite_protection_ratio = fmin(0.3, params->elite_protection_ratio);

    } else if (!improvement || stats.trend < 0) {
        /* Stagnation - need more exploration */

        /* Increase exploration */
        params->exploration_factor *= (1.0 + learning_rate * 0.2);
        params->exploration_factor = fmin(0.8, params->exploration_factor);

        /* Increase mutation rates */
        params->optimization_mutation_rate *= (1.0 + learning_rate * 0.3);
        params->optimization_mutation_rate = fmin(0.3, params->optimization_mutation_rate);

        params->experimentation_rate *= (1.0 + learning_rate * 0.2);
        params->experimentation_rate = fmin(0.2, params->experimentation_rate);

        /* Reduce selection pressure to maintain diversity */
        params->elite_protection_ratio *= (1.0 - learning_rate * 0.1);
        params->elite_protection_ratio = fmax(0.05, params->elite_protection_ratio);

        /* Increase culling to try new solutions */
        params->culling_ratio *= (1.0 + learning_rate * 0.1);
        params->culling_ratio = fmin(0.5, params->culling_ratio);
    }

    /* Adapt based on fitness variance */
    if (stats.stddev < stats.mean * 0.01) {
        /* Very low variance - population converged, increase exploration */
        params->variance_mutation_rate *= (1.0 + learning_rate * 0.3);
        params->variance_mutation_rate = fmin(0.5, params->variance_mutation_rate);
    } else if (stats.stddev > stats.mean * 0.3) {
        /* High variance - decrease exploration to focus */
        params->variance_mutation_rate *= (1.0 - learning_rate * 0.2);
        params->variance_mutation_rate = fmax(0.05, params->variance_mutation_rate);
    }
}

void evocore_meta_suggest_mutation_rate(double diversity,
                                       evocore_meta_params_t *params) {
    if (params == NULL) return;

    /* High diversity -> lower mutation (exploit) */
    /* Low diversity -> higher mutation (explore) */

    if (diversity > 0.5) {
        /* Very diverse - focus on exploitation */
        params->optimization_mutation_rate = 0.02;
        params->variance_mutation_rate = 0.10;
        params->experimentation_rate = 0.02;
    } else if (diversity > 0.3) {
        /* Moderate diversity - balanced */
        params->optimization_mutation_rate = 0.05;
        params->variance_mutation_rate = 0.15;
        params->experimentation_rate = 0.05;
    } else if (diversity > 0.1) {
        /* Low diversity - increase exploration */
        params->optimization_mutation_rate = 0.10;
        params->variance_mutation_rate = 0.25;
        params->experimentation_rate = 0.10;
    } else {
        /* Very low diversity - heavy exploration needed */
        params->optimization_mutation_rate = 0.20;
        params->variance_mutation_rate = 0.40;
        params->experimentation_rate = 0.20;
    }
}

void evocore_meta_suggest_selection_pressure(double fitness_stddev,
                                            evocore_meta_params_t *params) {
    if (params == NULL) return;

    /* Low fitness spread -> higher pressure to differentiate */
    /* High fitness spread -> lower pressure to maintain diversity */

    /* Normalize by mean (rough approximation) */
    double normalized_cv = fitness_stddev;  /* Coefficient of variation proxy */

    if (normalized_cv < 0.05) {
        /* Tight fitness distribution - high pressure needed */
        params->elite_protection_ratio = 0.15;
        params->culling_ratio = 0.35;
        params->fitness_threshold_for_breeding = 0.1;
    } else if (normalized_cv < 0.15) {
        /* Moderate spread - medium pressure */
        params->elite_protection_ratio = 0.10;
        params->culling_ratio = 0.25;
        params->fitness_threshold_for_breeding = 0.0;
    } else {
        /* Wide spread - low pressure to maintain diversity */
        params->elite_protection_ratio = 0.05;
        params->culling_ratio = 0.15;
        params->fitness_threshold_for_breeding = 0.0;
    }
}

/*========================================================================
 * Online Learning
 *========================================================================*/

/**
 * Learning bucket for tracking parameter performance
 */
typedef struct {
    double param_value;
    double total_fitness;
    int count;
    double avg_fitness;
} learning_bucket_t;

#define BUCKET_COUNT 20

static learning_bucket_t mutation_rate_buckets[BUCKET_COUNT];
static learning_bucket_t exploration_buckets[BUCKET_COUNT];
static bool learning_initialized = false;

static void init_learning(void) {
    if (learning_initialized) return;

    memset(mutation_rate_buckets, 0, sizeof(mutation_rate_buckets));
    memset(exploration_buckets, 0, sizeof(exploration_buckets));

    /* Initialize bucket ranges */
    for (int i = 0; i < BUCKET_COUNT; i++) {
        mutation_rate_buckets[i].param_value = 0.01 + (i * 0.02);
        exploration_buckets[i].param_value = (double)i / (double)BUCKET_COUNT;
    }

    learning_initialized = true;
}

/**
 * Update learning bucket with new fitness observation
 */
static void update_bucket(learning_bucket_t *buckets,
                          double param_value,
                          double fitness,
                          double learning_rate) {
    /* Find closest bucket */
    int best_idx = 0;
    double best_diff = fabs(buckets[0].param_value - param_value);

    for (int i = 1; i < BUCKET_COUNT; i++) {
        double diff = fabs(buckets[i].param_value - param_value);
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    /* Update bucket with exponential moving average */
    learning_bucket_t *bucket = &buckets[best_idx];
    bucket->count++;

    if (bucket->count == 1) {
        bucket->avg_fitness = fitness;
    } else {
        bucket->avg_fitness = learning_rate * fitness +
                             (1.0 - learning_rate) * bucket->avg_fitness;
    }
}

/**
 * Get best value from learning buckets
 */
static double get_best_bucket_value(const learning_bucket_t *buckets,
                                     int min_samples) {
    int best_idx = -1;
    double best_fitness = -INFINITY;

    for (int i = 0; i < BUCKET_COUNT; i++) {
        if (buckets[i].count >= min_samples &&
            buckets[i].avg_fitness > best_fitness) {
            best_fitness = buckets[i].avg_fitness;
            best_idx = i;
        }
    }

    if (best_idx >= 0) {
        return buckets[best_idx].param_value;
    }

    return -1.0;  /* Not enough data */
}

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
                               double learning_rate) {
    init_learning();

    update_bucket(mutation_rate_buckets, mutation_rate, fitness, learning_rate);
    update_bucket(exploration_buckets, exploration_factor, fitness, learning_rate);
}

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
                                    int min_samples) {
    if (!learning_initialized) {
        return false;
    }

    double mut_rate = get_best_bucket_value(mutation_rate_buckets, min_samples);
    double expl = get_best_bucket_value(exploration_buckets, min_samples);

    if (mut_rate > 0 && expl >= 0) {
        if (mutation_rate_out) *mutation_rate_out = mut_rate;
        if (exploration_out) *exploration_out = expl;
        return true;
    }

    return false;
}

/**
 * Reset learning history
 */
void evocore_meta_reset_learning(void) {
    learning_initialized = false;
    memset(mutation_rate_buckets, 0, sizeof(mutation_rate_buckets));
    memset(exploration_buckets, 0, sizeof(exploration_buckets));
}
