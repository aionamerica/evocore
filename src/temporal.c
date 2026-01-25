/**
 * Evocore Temporal Learning Implementation
 *
 * Implements time-bucketed organic learning for regime adaptation.
 */

#define _GNU_SOURCE
#include "evocore/temporal.h"
#include "evocore/log.h"
#include "internal.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*========================================================================
 * Constants
 *========================================================================*/

#define INITIAL_CONTEXT_CAPACITY 64
#define DEFAULT_RETENTION 36  /* Keep 36 time periods by default */

/* Time durations in seconds */
#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR (60 * 60)
#define SECONDS_PER_DAY (24 * SECONDS_PER_HOUR)
#define SECONDS_PER_WEEK (7 * SECONDS_PER_DAY)
#define SECONDS_PER_MONTH (30 * SECONDS_PER_DAY)
#define SECONDS_PER_YEAR (365 * SECONDS_PER_DAY)

/* Minimum buckets for meaningful statistics */
#define MIN_BUCKETS_FOR_ORGANIC 2
#define MIN_BUCKETS_FOR_TREND 3

/*========================================================================
 * Internal Hash Table
 *========================================================================*/

typedef struct hash_entry {
    char *key;
    evocore_temporal_list_t *list;
    uint32_t hash;
    struct hash_entry *next;
} hash_entry_t;

typedef struct {
    hash_entry_t **entries;
    size_t capacity;
    size_t count;
} hash_table_t;

/* Simple hash function */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint32_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

/* Create hash table */
static hash_table_t* hash_create(size_t capacity) {
    hash_table_t *table = calloc(1, sizeof(hash_table_t));
    if (!table) return NULL;

    table->entries = calloc(capacity, sizeof(hash_entry_t*));
    if (!table->entries) {
        free(table);
        return NULL;
    }

    table->capacity = capacity;
    table->count = 0;
    return table;
}

/* Free hash table */
static void hash_free(hash_table_t *table) {
    if (!table) return;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            hash_entry_t *next = entry->next;
            free(entry->key);
            if (entry->list) {
                if (entry->list->buckets) {
                    for (size_t j = 0; j < entry->list->count; j++) {
                        if (entry->list->buckets[j].stats) {
                            evocore_weighted_array_free(entry->list->buckets[j].stats);
                        }
                    }
                    free(entry->list->buckets);
                }
                free(entry->list);
            }
            free(entry);
            entry = next;
        }
    }

    free(table->entries);
    free(table);
}

/* Get entry */
static hash_entry_t* hash_get(hash_table_t *table, const char *key) {
    uint32_t hash = hash_string(key);
    size_t index = hash % table->capacity;

    hash_entry_t *entry = table->entries[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    return NULL;
}

/* Create or get entry */
static hash_entry_t* hash_set(hash_table_t *table, const char *key, size_t param_count, size_t retention) {
    uint32_t hash = hash_string(key);
    size_t index = hash % table->capacity;

    /* Check if exists */
    hash_entry_t *entry = table->entries[index];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->next;
    }

    /* Create new entry */
    hash_entry_t *new_entry = calloc(1, sizeof(hash_entry_t));
    if (!new_entry) return NULL;

    new_entry->key = strdup(key);
    if (!new_entry->key) {
        free(new_entry);
        return NULL;
    }

    new_entry->list = calloc(1, sizeof(evocore_temporal_list_t));
    if (!new_entry->list) {
        free(new_entry->key);
        free(new_entry);
        return NULL;
    }

    new_entry->list->capacity = retention;
    new_entry->list->buckets = calloc(retention, sizeof(evocore_temporal_bucket_t));
    if (!new_entry->list->buckets) {
        free(new_entry->list);
        free(new_entry->key);
        free(new_entry);
        return NULL;
    }

    new_entry->hash = hash;

    /* Insert at head */
    new_entry->next = table->entries[index];
    table->entries[index] = new_entry;
    table->count++;

    return new_entry;
}

/*========================================================================
 * System Management
 *========================================================================*/

evocore_temporal_system_t* evocore_temporal_create(
    evocore_temporal_bucket_type_t bucket_type,
    size_t param_count,
    size_t retention_count
) {
    if (param_count == 0 || retention_count == 0) return NULL;

    evocore_temporal_system_t *system = calloc(1, sizeof(evocore_temporal_system_t));
    if (!system) return NULL;

    system->bucket_type = bucket_type;
    system->param_count = param_count;
    system->retention_count = retention_count;

    system->internal = hash_create(INITIAL_CONTEXT_CAPACITY);
    if (!system->internal) {
        free(system);
        return NULL;
    }

    return system;
}

void evocore_temporal_free(evocore_temporal_system_t *system) {
    if (!system) return;
    if (system->internal) {
        hash_free(system->internal);
    }
    free(system);
}

time_t evocore_temporal_bucket_duration(evocore_temporal_bucket_type_t bucket_type) {
    switch (bucket_type) {
        case EVOCORE_BUCKET_MINUTE: return SECONDS_PER_MINUTE;
        case EVOCORE_BUCKET_HOUR:   return SECONDS_PER_HOUR;
        case EVOCORE_BUCKET_DAY:    return SECONDS_PER_DAY;
        case EVOCORE_BUCKET_WEEK:   return SECONDS_PER_WEEK;
        case EVOCORE_BUCKET_MONTH:  return SECONDS_PER_MONTH;
        case EVOCORE_BUCKET_YEAR:   return SECONDS_PER_YEAR;
        default: return SECONDS_PER_DAY;
    }
}

/*========================================================================
 * Learning Operations
 *========================================================================*/

/* Get bucket start time for a timestamp */
static time_t get_bucket_start(evocore_temporal_bucket_type_t type, time_t timestamp) {
    struct tm *tm_info = localtime(&timestamp);
    if (!tm_info) return timestamp;

    time_t start = timestamp;

    switch (type) {
        case EVOCORE_BUCKET_MINUTE:
            tm_info->tm_sec = 0;
            break;
        case EVOCORE_BUCKET_HOUR:
            tm_info->tm_sec = 0;
            tm_info->tm_min = 0;
            break;
        case EVOCORE_BUCKET_DAY:
            tm_info->tm_sec = 0;
            tm_info->tm_min = 0;
            tm_info->tm_hour = 0;
            break;
        case EVOCORE_BUCKET_WEEK:
            tm_info->tm_sec = 0;
            tm_info->tm_min = 0;
            tm_info->tm_hour = 0;
            tm_info->tm_wday = 0;
            tm_info->tm_mday -= tm_info->tm_wday;
            break;
        case EVOCORE_BUCKET_MONTH:
            tm_info->tm_sec = 0;
            tm_info->tm_min = 0;
            tm_info->tm_hour = 0;
            tm_info->tm_mday = 1;
            break;
        case EVOCORE_BUCKET_YEAR:
            tm_info->tm_sec = 0;
            tm_info->tm_min = 0;
            tm_info->tm_hour = 0;
            tm_info->tm_mday = 1;
            tm_info->tm_mon = 0;
            break;
    }

    start = mktime(tm_info);
    return (start != -1) ? start : timestamp;
}

bool evocore_temporal_learn(
    evocore_temporal_system_t *system,
    const char *context_key,
    const double *parameters,
    size_t param_count,
    double fitness,
    time_t timestamp
) {
    if (!system || !context_key || !parameters) return false;
    if (param_count != system->param_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_set(table, context_key, param_count, system->retention_count);
    if (!entry) return false;

    evocore_temporal_list_t *list = entry->list;

    /* Find or create bucket */
    time_t bucket_start = get_bucket_start(system->bucket_type, timestamp);
    evocore_temporal_bucket_t *bucket = NULL;
    size_t bucket_index = 0;

    for (size_t i = 0; i < list->count; i++) {
        if (list->buckets[i].start_time == bucket_start) {
            bucket = &list->buckets[i];
            bucket_index = i;
            break;
        }
    }

    /* Create new bucket if needed */
    if (!bucket) {
        /* Check if at capacity */
        if (list->count >= list->capacity) {
            /* Remove oldest bucket */
            if (list->count > 0) {
                if (list->buckets[0].stats) {
                    evocore_weighted_array_free(list->buckets[0].stats);
                }
                /* Shift buckets */
                for (size_t i = 0; i < list->count - 1; i++) {
                    list->buckets[i] = list->buckets[i + 1];
                }
                list->count--;
            }
        }

        /* Add new bucket at end */
        bucket_index = list->count;
        bucket = &list->buckets[list->count];
        list->count++;

        bucket->start_time = bucket_start;
        bucket->end_time = bucket_start + evocore_temporal_bucket_duration(system->bucket_type) - 1;
        bucket->is_complete = false;
        bucket->stats = evocore_weighted_array_create(param_count);
        bucket->param_count = param_count;
        bucket->sample_count = 0;
        bucket->avg_fitness = 0.0;
        bucket->best_fitness = 0.0;
    }

    /* Update bucket statistics */
    evocore_weighted_array_update(bucket->stats, parameters, NULL, param_count, fitness);

    bucket->sample_count++;
    double prev_avg = bucket->avg_fitness;
    bucket->avg_fitness = (prev_avg * (bucket->sample_count - 1) + fitness) / bucket->sample_count;
    if (fitness > bucket->best_fitness) {
        bucket->best_fitness = fitness;
    }

    /* Mark older buckets as complete */
    time_t now = time(NULL);
    for (size_t i = 0; i < list->count; i++) {
        if (list->buckets[i].end_time < now - evocore_temporal_bucket_duration(system->bucket_type)) {
            list->buckets[i].is_complete = true;
        }
    }

    system->last_update = now;
    return true;
}

bool evocore_temporal_learn_now(
    evocore_temporal_system_t *system,
    const char *context_key,
    const double *parameters,
    size_t param_count,
    double fitness
) {
    return evocore_temporal_learn(system, context_key, parameters, param_count, fitness, time(NULL));
}

/*========================================================================
 * Organic Mean
 *========================================================================*/

bool evocore_temporal_get_organic_mean(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double *out_confidence
) {
    if (!system || !context_key || !out_parameters) return false;
    if (param_count != system->param_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list || entry->list->count < MIN_BUCKETS_FOR_ORGANIC) {
        return false;
    }

    evocore_temporal_list_t *list = entry->list;

    /* Compute organic mean: average of bucket means (equal weight per bucket) */
    for (size_t i = 0; i < param_count; i++) {
        double sum = 0.0;
        for (size_t j = 0; j < list->count; j++) {
            double mean = evocore_weighted_mean(&list->buckets[j].stats->stats[i]);
            sum += mean;
        }
        out_parameters[i] = sum / list->count;
    }

    /* Confidence based on bucket count */
    if (out_confidence) {
        double max_conf = 10.0; /* Full confidence at 10 buckets */
        *out_confidence = fmin(1.0, sqrt((double)list->count / max_conf));
    }

    return true;
}

bool evocore_temporal_get_weighted_mean(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count
) {
    if (!system || !context_key || !out_parameters) return false;
    if (param_count != system->param_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list || entry->list->count == 0) {
        return false;
    }

    evocore_temporal_list_t *list = entry->list;

    /* Combine all bucket statistics */
    evocore_weighted_array_t *combined = evocore_weighted_array_create(param_count);
    if (!combined) return false;

    for (size_t j = 0; j < list->count; j++) {
        for (size_t i = 0; i < param_count; i++) {
            double mean = evocore_weighted_mean(&list->buckets[j].stats->stats[i]);
            double count = (double)list->buckets[j].sample_count;
            evocore_weighted_update(&combined->stats[i], mean, count);
        }
    }

    /* Get means */
    evocore_weighted_array_get_means(combined, out_parameters, param_count);
    evocore_weighted_array_free(combined);

    return true;
}

/*========================================================================
 * Trend Analysis
 *========================================================================*/

bool evocore_temporal_get_trend(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_slopes,
    size_t param_count
) {
    if (!system || !context_key || !out_slopes) return false;
    if (param_count != system->param_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list || entry->list->count < MIN_BUCKETS_FOR_TREND) {
        return false;
    }

    evocore_temporal_list_t *list = entry->list;

    /* Linear regression for each parameter */
    for (size_t i = 0; i < param_count; i++) {
        double sum_x = 0.0;  /* Sum of bucket indices */
        double sum_y = 0.0;  /* Sum of bucket means */
        double sum_xy = 0.0; /* Sum of x*y */
        double sum_xx = 0.0; /* Sum of x^2 */

        size_t n = list->count;

        for (size_t j = 0; j < n; j++) {
            double y = evocore_weighted_mean(&list->buckets[j].stats->stats[i]);
            double x = (double)j;

            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }

        /* Slope = (n*sum_xy - sum_x*sum_y) / (n*sum_xx - sum_x*sum_x) */
        double denominator = n * sum_xx - sum_x * sum_x;
        if (fabs(denominator) < 0.0001) {
            out_slopes[i] = 0.0;  /* No trend */
        } else {
            out_slopes[i] = (n * sum_xy - sum_x * sum_y) / denominator;
        }
    }

    return true;
}

int evocore_temporal_trend_direction(double slope) {
    if (slope > 0.01) return 1;
    if (slope < -0.01) return -1;
    return 0;
}

/*========================================================================
 * Regime Change Detection
 *========================================================================*/

bool evocore_temporal_compare_recent(
    const evocore_temporal_system_t *system,
    const char *context_key,
    size_t recent_buckets,
    double *out_drift,
    size_t param_count
) {
    if (!system || !context_key || !out_drift) return false;
    if (param_count != system->param_count) return false;
    if (param_count > 64) return false;  /* Bounds check for fixed-size local arrays */
    if (recent_buckets == 0 || recent_buckets >= system->retention_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list || entry->list->count < recent_buckets * 2) {
        return false;
    }

    evocore_temporal_list_t *list = entry->list;
    size_t total = list->count;

    /* Compute recent means */
    double recent_means[64];  /* Max 64 parameters */
    size_t recent_start = total - recent_buckets;

    for (size_t i = 0; i < param_count; i++) {
        recent_means[i] = 0.0;
        for (size_t j = recent_start; j < total; j++) {
            recent_means[i] += evocore_weighted_mean(&list->buckets[j].stats->stats[i]);
        }
        recent_means[i] /= recent_buckets;
    }

    /* Compute historical means (excluding recent) */
    for (size_t i = 0; i < param_count; i++) {
        double historical_mean = 0.0;
        for (size_t j = 0; j < recent_start; j++) {
            historical_mean += evocore_weighted_mean(&list->buckets[j].stats->stats[i]);
        }
        historical_mean /= recent_start;

        /* Drift = difference between recent and historical */
        out_drift[i] = recent_means[i] - historical_mean;
    }

    return true;
}

bool evocore_temporal_detect_regime_change(
    const evocore_temporal_system_t *system,
    const char *context_key,
    size_t recent_buckets,
    double threshold
) {
    if (!system || !context_key) return false;
    if (system->param_count > 64) return false;  /* Bounds check for fixed-size local array */

    double drift[64];
    if (!evocore_temporal_compare_recent(system, context_key, recent_buckets, drift, system->param_count)) {
        return false;
    }

    /* Check if any parameter exceeds threshold */
    for (size_t i = 0; i < system->param_count; i++) {
        if (fabs(drift[i]) > threshold) {
            return true;  /* Regime change detected */
        }
    }

    return false;
}

/*========================================================================
 * Bucket Management
 *========================================================================*/

bool evocore_temporal_get_bucket_at(
    const evocore_temporal_system_t *system,
    const char *context_key,
    time_t timestamp,
    evocore_temporal_bucket_t **out_bucket
) {
    if (!system || !context_key || !out_bucket) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list) return false;

    evocore_temporal_list_t *list = entry->list;
    time_t bucket_start = get_bucket_start(system->bucket_type, timestamp);

    for (size_t i = 0; i < list->count; i++) {
        if (list->buckets[i].start_time == bucket_start) {
            *out_bucket = &list->buckets[i];
            return true;
        }
    }

    return false;
}

bool evocore_temporal_get_current_bucket(
    const evocore_temporal_system_t *system,
    const char *context_key,
    evocore_temporal_bucket_t **out_bucket
) {
    return evocore_temporal_get_bucket_at(system, context_key, time(NULL), out_bucket);
}

bool evocore_temporal_get_buckets(
    const evocore_temporal_system_t *system,
    const char *context_key,
    evocore_temporal_list_t **out_list
) {
    if (!system || !context_key || !out_list) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry) {
        *out_list = NULL;
        return false;
    }

    *out_list = entry->list;
    return true;
}

void evocore_temporal_free_list(evocore_temporal_list_t *list) {
    /* Don't free - pointers owned by hash table */
    (void)list;
}

/*========================================================================
 * Sampling
 *========================================================================*/

bool evocore_temporal_sample_organic(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double exploration_factor,
    unsigned int *seed
) {
    if (!system || !context_key || !out_parameters) return false;
    if (param_count != system->param_count) return false;
    if (param_count > 64) return false;  /* Bounds check for fixed-size local array */

    /* Get organic mean */
    double organic_means[64];
    if (!evocore_temporal_get_organic_mean(system, context_key, organic_means, param_count, NULL)) {
        /* No data, return random */
        for (size_t i = 0; i < param_count; i++) {
            out_parameters[i] = (double)rand_r(seed) / (double)RAND_MAX;
        }
        return true;
    }

    /* Get bucket means for std calculation */
    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list) return false;

    evocore_temporal_list_t *list = entry->list;

    /* Sample with exploration */
    for (size_t i = 0; i < param_count; i++) {
        double mean = organic_means[i];

        /* Calculate std from bucket means */
        double variance = 0.0;
        for (size_t j = 0; j < list->count; j++) {
            double bucket_mean = evocore_weighted_mean(&list->buckets[j].stats->stats[i]);
            variance += (bucket_mean - mean) * (bucket_mean - mean);
        }
        variance /= list->count;
        double std = sqrt(variance);

        /* Add sample variance */
        std += evocore_weighted_std(&list->buckets[0].stats->stats[i]);

        /* Sample from distribution */
        if (exploration_factor >= 1.0) {
            out_parameters[i] = (double)rand_r(seed) / (double)RAND_MAX;
        } else if (exploration_factor <= 0.0) {
            /* Pure exploitation - sample from Gaussian */
            double u1 = (double)rand_r(seed) / (double)RAND_MAX;
            double u2 = (double)rand_r(seed) / (double)RAND_MAX;
            if (u1 < 0.0001) u1 = 0.0001;
            double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            out_parameters[i] = mean + std * z;
        } else {
            /* Mix exploitation with exploration */
            double u1 = (double)rand_r(seed) / (double)RAND_MAX;
            double u2 = (double)rand_r(seed) / (double)RAND_MAX;
            if (u1 < 0.0001) u1 = 0.0001;
            double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            double learned = mean + std * z;
            double random = (double)rand_r(seed) / (double)RAND_MAX;

            out_parameters[i] = (1.0 - exploration_factor) * learned + exploration_factor * random;
        }
    }

    return true;
}

bool evocore_temporal_sample_trend(
    const evocore_temporal_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double trend_strength,
    unsigned int *seed
) {
    if (!system || !context_key || !out_parameters) return false;
    if (param_count != system->param_count) return false;
    if (param_count > 64) return false;  /* Bounds check for fixed-size local arrays */

    /* Get trends */
    double slopes[64];
    if (!evocore_temporal_get_trend(system, context_key, slopes, param_count)) {
        return false;
    }

    /* Get organic means */
    double means[64];
    if (!evocore_temporal_get_organic_mean(system, context_key, means, param_count, NULL)) {
        return false;
    }

    /* Sample biased by trend */
    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list) return false;

    evocore_temporal_list_t *list = entry->list;

    for (size_t i = 0; i < param_count; i++) {
        /* Get std from first bucket */
        double std = evocore_weighted_std(&list->buckets[0].stats->stats[i]);

        /* Bias mean by trend */
        double biased_mean = means[i] + slopes[i] * trend_strength;

        /* Sample */
        double u1 = (double)rand_r(seed) / (double)RAND_MAX;
        double u2 = (double)rand_r(seed) / (double)RAND_MAX;
        if (u1 < 0.0001) u1 = 0.0001;
        double z = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);

        out_parameters[i] = biased_mean + std * z;
    }

    return true;
}

/*========================================================================
 * Persistence
 *========================================================================*/

bool evocore_temporal_save_json(
    const evocore_temporal_system_t *system,
    const char *filepath
) {
    if (!system || !filepath) return false;

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"bucket_type\": %d,\n", system->bucket_type);
    fprintf(f, "  \"param_count\": %zu,\n", system->param_count);
    fprintf(f, "  \"retention_count\": %zu,\n", system->retention_count);
    fprintf(f, "  \"contexts\": {\n");

    hash_table_t *table = (hash_table_t*)system->internal;
    size_t context_idx = 0;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            evocore_temporal_list_t *list = entry->list;

            fprintf(f, "    \"%s\": {\n", entry->key);
            fprintf(f, "      \"bucket_count\": %zu,\n", list->count);

            /* Write bucket data */
            fprintf(f, "      \"buckets\": [\n");
            for (size_t j = 0; j < list->count; j++) {
                evocore_temporal_bucket_t *bucket = &list->buckets[j];

                fprintf(f, "        {\"start_time\": %ld, \"end_time\": %ld, \"samples\": %zu, \"means\": [",
                        (long)bucket->start_time, (long)bucket->end_time, bucket->sample_count);

                for (size_t k = 0; k < system->param_count; k++) {
                    double mean = evocore_weighted_mean(&bucket->stats->stats[k]);
                    fprintf(f, "%.6g%s", mean, k + 1 < system->param_count ? ", " : "");
                }

                fprintf(f, "] }%s\n", j + 1 < list->count ? "," : "");
            }

            fprintf(f, "      ]\n");
            fprintf(f, "    }%s\n", context_idx + 1 < table->count ? "," : "");
            context_idx++;

            entry = entry->next;
        }
    }

    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}

bool evocore_temporal_load_json(
    const char *filepath,
    evocore_temporal_system_t **out_system
) {
    /* TODO: Implement JSON loading */
    (void)filepath;
    (void)out_system;
    return false;
}

/*========================================================================
 * Utility Functions
 *========================================================================*/

size_t evocore_temporal_bucket_count(const evocore_temporal_system_t *system) {
    if (!system) return 0;

    hash_table_t *table = (hash_table_t*)system->internal;
    size_t count = 0;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            count += entry->list->count;
            entry = entry->next;
        }
    }

    return count;
}

size_t evocore_temporal_context_count(const evocore_temporal_system_t *system) {
    if (!system) return 0;
    hash_table_t *table = (hash_table_t*)system->internal;
    return table->count;
}

size_t evocore_temporal_prune_old(evocore_temporal_system_t *system) {
    if (!system) return 0;

    size_t pruned = 0;
    time_t cutoff = time(NULL) - evocore_temporal_bucket_duration(system->bucket_type) * system->retention_count;

    hash_table_t *table = (hash_table_t*)system->internal;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            evocore_temporal_list_t *list = entry->list;
            size_t write_idx = 0;

            for (size_t j = 0; j < list->count; j++) {
                if (list->buckets[j].end_time >= cutoff) {
                    if (write_idx != j) {
                        list->buckets[write_idx] = list->buckets[j];
                    }
                    write_idx++;
                } else {
                    if (list->buckets[j].stats) {
                        evocore_weighted_array_free(list->buckets[j].stats);
                    }
                    pruned++;
                }
            }

            list->count = write_idx;
            entry = entry->next;
        }
    }

    return pruned;
}

bool evocore_temporal_reset_context(
    evocore_temporal_system_t *system,
    const char *context_key
) {
    if (!system || !context_key) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);
    if (!entry || !entry->list) return false;

    /* Free all buckets */
    for (size_t j = 0; j < entry->list->count; j++) {
        if (entry->list->buckets[j].stats) {
            evocore_weighted_array_free(entry->list->buckets[j].stats);
        }
    }

    entry->list->count = 0;
    return true;
}

void evocore_temporal_reset_all(evocore_temporal_system_t *system) {
    if (!system) return;

    hash_table_t *table = (hash_table_t*)system->internal;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            evocore_temporal_reset_context(system, entry->key);
            entry = entry->next;
        }
    }
}
