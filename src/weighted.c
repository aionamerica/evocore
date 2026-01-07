/**
 * Evocore Weighted Statistics Implementation
 *
 * Implements West's online algorithm for numerically stable
 * weighted mean and variance calculation.
 */

#define _GNU_SOURCE
#include "evocore/weighted.h"
#include "evocore/log.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*========================================================================
 * Constants
 *========================================================================*/

#define DEFAULT_MIN_SAMPLES 3
#define DEFAULT_MAX_SAMPLES_FOR_CONFIDENCE 100
#define MIN_WEIGHT 0.0001  /* Minimum weight to avoid division issues */

/*========================================================================
 * Single Value Statistics - Implementation
 *========================================================================*/

void evocore_weighted_init(evocore_weighted_stats_t *stats) {
    if (!stats) return;

    stats->mean = 0.0;
    stats->variance = 0.0;
    stats->sum_weights = 0.0;
    stats->m2 = 0.0;
    stats->count = 0;
    stats->min_value = INFINITY;
    stats->max_value = -INFINITY;
    stats->sum_weighted_x = 0.0;
}

bool evocore_weighted_update(
    evocore_weighted_stats_t *stats,
    double value,
    double weight
) {
    if (!stats) return false;

    /* Ensure weight is positive */
    if (weight < MIN_WEIGHT) weight = MIN_WEIGHT;

    /* Update min/max */
    if (value < stats->min_value) stats->min_value = value;
    if (value > stats->max_value) stats->max_value = value;

    /* West's online algorithm for weighted statistics */
    if (stats->count == 0) {
        /* First observation */
        stats->mean = value;
        stats->sum_weights = weight;
        stats->m2 = 0.0;
        stats->sum_weighted_x = value * weight;
    } else {
        double prev_sum_weights = stats->sum_weights;
        double new_sum_weights = prev_sum_weights + weight;
        double delta = value - stats->mean;

        /* Update weighted mean */
        stats->mean += (weight / new_sum_weights) * delta;

        /* Update m2 (sum of squared deviations) */
        stats->m2 += prev_sum_weights * weight * delta * delta / new_sum_weights;

        /* Update sum of weights */
        stats->sum_weights = new_sum_weights;

        /* Update weighted sum */
        stats->sum_weighted_x += value * weight;
    }

    stats->count++;
    stats->variance = stats->m2 / stats->sum_weights;

    return true;
}

double evocore_weighted_mean(const evocore_weighted_stats_t *stats) {
    if (!stats || stats->count == 0) return 0.0;
    return stats->mean;
}

double evocore_weighted_std(const evocore_weighted_stats_t *stats) {
    if (!stats || stats->count < 2) return 0.0;
    return sqrt(stats->variance);
}

double evocore_weighted_variance(const evocore_weighted_stats_t *stats) {
    if (!stats || stats->count < 2) return 0.0;
    return stats->variance;
}

double evocore_weighted_sample(
    const evocore_weighted_stats_t *stats,
    unsigned int *seed
) {
    if (!stats || stats->count == 0) return 0.0;

    /* Box-Muller transform for Gaussian sampling */
    double std = evocore_weighted_std(stats);
    double mean = evocore_weighted_mean(stats);

    if (std < 0.0001) {
        /* No variance, return mean */
        return mean;
    }

    /* Generate two uniform random variables */
    double u1 = (double)rand_r(seed) / (double)RAND_MAX;
    double u2 = (double)rand_r(seed) / (double)RAND_MAX;

    /* Avoid log(0) */
    if (u1 < 0.0001) u1 = 0.0001;

    /* Box-Muller transform */
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);

    return mean + std * z0;
}

void evocore_weighted_reset(evocore_weighted_stats_t *stats) {
    if (!stats) return;
    evocore_weighted_init(stats);
}

bool evocore_weighted_has_data(
    const evocore_weighted_stats_t *stats,
    size_t min_samples
) {
    if (!stats) return false;
    if (min_samples == 0) min_samples = DEFAULT_MIN_SAMPLES;
    return stats->count >= min_samples;
}

double evocore_weighted_confidence(
    const evocore_weighted_stats_t *stats,
    size_t max_samples
) {
    if (!stats || stats->count == 0) return 0.0;
    if (max_samples == 0) max_samples = DEFAULT_MAX_SAMPLES_FOR_CONFIDENCE;

    /* Sqrt scaling: confidence = min(1.0, sqrt(n / max_samples)) */
    double ratio = (double)stats->count / (double)max_samples;
    return fmin(1.0, sqrt(ratio));
}

/*========================================================================
 * Array Statistics - Implementation
 *========================================================================*/

evocore_weighted_array_t* evocore_weighted_array_create(size_t count) {
    if (count == 0) return NULL;

    evocore_weighted_array_t *array = malloc(sizeof(evocore_weighted_array_t));
    if (!array) return NULL;

    array->stats = calloc(count, sizeof(evocore_weighted_stats_t));
    if (!array->stats) {
        free(array);
        return NULL;
    }

    array->count = count;

    /* Initialize all stats */
    for (size_t i = 0; i < count; i++) {
        evocore_weighted_init(&array->stats[i]);
    }

    return array;
}

void evocore_weighted_array_free(evocore_weighted_array_t *array) {
    if (!array) return;
    free(array->stats);
    free(array);
}

bool evocore_weighted_array_update(
    evocore_weighted_array_t *array,
    const double *values,
    const double *weights,
    size_t count,
    double global_weight
) {
    if (!array || !values) return false;
    if (count != array->count) return false;

    for (size_t i = 0; i < count; i++) {
        double weight = global_weight;
        if (weights) {
            weight *= weights[i];
        }
        evocore_weighted_update(&array->stats[i], values[i], weight);
    }

    return true;
}

bool evocore_weighted_array_get_means(
    const evocore_weighted_array_t *array,
    double *out_means,
    size_t count
) {
    if (!array || !out_means) return false;
    if (count != array->count) return false;

    for (size_t i = 0; i < count; i++) {
        out_means[i] = evocore_weighted_mean(&array->stats[i]);
    }

    return true;
}

bool evocore_weighted_array_get_stds(
    const evocore_weighted_array_t *array,
    double *out_stds,
    size_t count
) {
    if (!array || !out_stds) return false;
    if (count != array->count) return false;

    for (size_t i = 0; i < count; i++) {
        out_stds[i] = evocore_weighted_std(&array->stats[i]);
    }

    return true;
}

bool evocore_weighted_array_sample(
    const evocore_weighted_array_t *array,
    double *out_values,
    size_t count,
    double exploration_factor,
    unsigned int *seed
) {
    if (!array || !out_values) return false;
    if (count != array->count) return false;

    /* Clamp exploration factor */
    if (exploration_factor < 0.0) exploration_factor = 0.0;
    if (exploration_factor > 1.0) exploration_factor = 1.0;

    for (size_t i = 0; i < count; i++) {
        const evocore_weighted_stats_t *stat = &array->stats[i];

        if (stat->count < DEFAULT_MIN_SAMPLES) {
            /* No data yet, random uniform */
            out_values[i] = (double)rand_r(seed) / (double)RAND_MAX;
        } else {
            double mean = evocore_weighted_mean(stat);
            double std = evocore_weighted_std(stat);

            /* Mix learned distribution with random based on exploration */
            if (exploration_factor > 0.0) {
                double learned_value = evocore_weighted_sample(stat, seed);
                double random_value = (double)rand_r(seed) / (double)RAND_MAX;

                /* Linear interpolation based on exploration factor */
                out_values[i] = (1.0 - exploration_factor) * learned_value
                              + exploration_factor * random_value;
            } else {
                out_values[i] = evocore_weighted_sample(stat, seed);
            }
        }
    }

    return true;
}

void evocore_weighted_array_reset(evocore_weighted_array_t *array) {
    if (!array) return;

    for (size_t i = 0; i < array->count; i++) {
        evocore_weighted_reset(&array->stats[i]);
    }
}

/*========================================================================
 * Utility Functions - Implementation
 *========================================================================*/

bool evocore_weighted_merge(
    evocore_weighted_stats_t *stats1,
    const evocore_weighted_stats_t *stats2
) {
    if (!stats1 || !stats2) return false;
    if (stats2->count == 0) return true;  /* Nothing to merge */

    if (stats1->count == 0) {
        /* stats1 is empty, just copy stats2 */
        evocore_weighted_clone(stats2, stats1);
        return true;
    }

    /* Parallel algorithm for merging weighted statistics */
    double n1 = stats1->sum_weights;
    double n2 = stats2->sum_weights;
    double total = n1 + n2;

    if (total < MIN_WEIGHT) return false;

    double delta = stats2->mean - stats1->mean;
    double new_mean = stats1->mean + (n2 / total) * delta;

    /* Combine m2 values */
    double new_m2 = stats1->m2 + stats2->m2
                  + (n1 * n2 * delta * delta) / total;

    /* Update stats1 */
    stats1->mean = new_mean;
    stats1->m2 = new_m2;
    stats1->variance = new_m2 / total;
    stats1->sum_weights = total;
    stats1->count += stats2->count;
    stats1->sum_weighted_x += stats2->sum_weighted_x;

    /* Update min/max */
    if (stats2->min_value < stats1->min_value) {
        stats1->min_value = stats2->min_value;
    }
    if (stats2->max_value > stats1->max_value) {
        stats1->max_value = stats2->max_value;
    }

    return true;
}

void evocore_weighted_clone(
    const evocore_weighted_stats_t *src,
    evocore_weighted_stats_t *dst
) {
    if (!src || !dst) return;

    dst->mean = src->mean;
    dst->variance = src->variance;
    dst->sum_weights = src->sum_weights;
    dst->m2 = src->m2;
    dst->count = src->count;
    dst->min_value = src->min_value;
    dst->max_value = src->max_value;
    dst->sum_weighted_x = src->sum_weighted_x;
}

size_t evocore_weighted_to_json(
    const evocore_weighted_stats_t *stats,
    char *buffer,
    size_t buffer_size
) {
    if (!stats || !buffer || buffer_size == 0) return 0;

    int written = snprintf(buffer, buffer_size,
        "{"
        "\"mean\":%.6g,"
        "\"variance\":%.6g,"
        "\"std\":%.6g,"
        "\"sum_weights\":%.6g,"
        "\"count\":%zu,"
        "\"min\":%.6g,"
        "\"max\":%.6g"
        "}",
        stats->mean,
        stats->variance,
        sqrt(stats->variance),
        stats->sum_weights,
        stats->count,
        stats->min_value,
        stats->max_value
    );

    return (written < 0 || (size_t)written >= buffer_size) ? 0 : (size_t)written;
}

bool evocore_weighted_from_json(
    const char *json,
    evocore_weighted_stats_t *stats
) {
    if (!json || !stats) return false;

    /* Simple JSON parsing - extract values */
    double mean = 0.0, variance = 0.0, sum_weights = 0.0;
    size_t count = 0;
    double min_val = 0.0, max_val = 0.0;

    /* Parse mean */
    const char *mean_ptr = strstr(json, "\"mean\":");
    if (mean_ptr) {
        mean = atof(mean_ptr + 7);
    }

    /* Parse variance */
    const char *var_ptr = strstr(json, "\"variance\":");
    if (var_ptr) {
        variance = atof(var_ptr + 11);
    }

    /* Parse sum_weights */
    const char *sum_ptr = strstr(json, "\"sum_weights\":");
    if (sum_ptr) {
        sum_weights = atof(sum_ptr + 14);
    }

    /* Parse count */
    const char *count_ptr = strstr(json, "\"count\":");
    if (count_ptr) {
        count = (size_t)atoll(count_ptr + 8);
    }

    /* Parse min */
    const char *min_ptr = strstr(json, "\"min\":");
    if (min_ptr) {
        min_val = atof(min_ptr + 6);
    }

    /* Parse max */
    const char *max_ptr = strstr(json, "\"max\":");
    if (max_ptr) {
        max_val = atof(max_ptr + 6);
    }

    /* Initialize stats with parsed values */
    evocore_weighted_init(stats);
    stats->mean = mean;
    stats->variance = variance;
    stats->sum_weights = sum_weights;
    stats->count = count;
    stats->min_value = min_val;
    stats->max_value = max_val;
    stats->m2 = variance * sum_weights;

    return true;
}
