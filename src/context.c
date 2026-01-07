/**
 * Evocore Context Learning Implementation
 *
 * Implements multi-dimensional context learning using a hash table
 * for O(1) context lookup.
 */

#define _GNU_SOURCE
#include "evocore/context.h"
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

#define INITIAL_HASH_CAPACITY 256
#define HASH_LOAD_FACTOR 0.75
#define MAX_KEY_LENGTH 256
#define DEFAULT_MIN_SAMPLES 3

/*========================================================================
 * Internal Hash Table
 *========================================================================*/

typedef struct hash_entry {
    char *key;
    evocore_context_stats_t *stats;
    struct hash_entry *next;
    uint32_t hash;
} hash_entry_t;

typedef struct {
    hash_entry_t **entries;
    size_t capacity;
    size_t count;
    size_t dimension_count;
} hash_table_t;

/* Hash function (FNV-1a) */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint32_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

/* Create hash table */
static hash_table_t* hash_create(size_t capacity, size_t dimension_count) {
    hash_table_t *table = calloc(1, sizeof(hash_table_t));
    if (!table) return NULL;

    table->entries = calloc(capacity, sizeof(hash_entry_t*));
    if (!table->entries) {
        free(table);
        return NULL;
    }

    table->capacity = capacity;
    table->count = 0;
    table->dimension_count = dimension_count;
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
            if (entry->stats) {
                if (entry->stats->stats) {
                    evocore_weighted_array_free(entry->stats->stats);
                }
                free(entry->stats);
            }
            free(entry);
            entry = next;
        }
    }

    free(table->entries);
    free(table);
}

/* Get entry from hash table */
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

/* Set entry in hash table */
static hash_entry_t* hash_set(hash_table_t *table, const char *key, size_t param_count) {
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

    new_entry->stats = calloc(1, sizeof(evocore_context_stats_t));
    if (!new_entry->stats) {
        free(new_entry->key);
        free(new_entry);
        return NULL;
    }

    new_entry->stats->key = new_entry->key;
    new_entry->stats->stats = evocore_weighted_array_create(param_count);
    if (!new_entry->stats->stats) {
        free(new_entry->stats->key);
        free(new_entry->stats);
        free(new_entry->key);
        free(new_entry);
        return NULL;
    }

    new_entry->stats->param_count = param_count;
    new_entry->hash = hash;

    /* Insert at head */
    new_entry->next = table->entries[index];
    table->entries[index] = new_entry;
    table->count++;

    return new_entry;
}

/* Resize hash table */
static bool hash_resize(hash_table_t *table, size_t new_capacity) {
    if (new_capacity <= table->capacity) return false;

    hash_table_t *new_table = hash_create(new_capacity, table->dimension_count);
    if (!new_table) return false;

    /* Rehash all entries */
    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            hash_entry_t *next = entry->next;

            /* Re-insert into new table */
            uint32_t hash = entry->hash;
            size_t index = hash % new_capacity;

            entry->next = new_table->entries[index];
            new_table->entries[index] = entry;
            new_table->count++;

            entry = next;
        }
    }

    /* Swap and free old */
    hash_entry_t **old_entries = table->entries;
    size_t old_capacity = table->capacity;

    table->entries = new_table->entries;
    table->capacity = new_table->capacity;

    free(old_entries);
    free(new_table);

    return true;
}

/*========================================================================
 * Context System
 *========================================================================*/

evocore_context_system_t* evocore_context_system_create(
    const evocore_context_dimension_t *dimensions,
    size_t dimension_count,
    size_t param_count
) {
    if (!dimensions || dimension_count == 0 || param_count == 0) {
        return NULL;
    }

    evocore_context_system_t *system = calloc(1, sizeof(evocore_context_system_t));
    if (!system) return NULL;

    /* Copy dimensions */
    system->dimensions = calloc(dimension_count, sizeof(evocore_context_dimension_t));
    if (!system->dimensions) {
        free(system);
        return NULL;
    }

    for (size_t i = 0; i < dimension_count; i++) {
        system->dimensions[i].name = strdup(dimensions[i].name);
        if (!system->dimensions[i].name) {
            /* Cleanup on failure */
            for (size_t j = 0; j < i; j++) {
                free(system->dimensions[j].name);
            }
            free(system->dimensions);
            free(system);
            return NULL;
        }

        system->dimensions[i].value_count = dimensions[i].value_count;
        system->dimensions[i].values = calloc(dimensions[i].value_count, sizeof(char*));
        if (!system->dimensions[i].values) {
            free(system->dimensions[i].name);
            /* Cleanup... */
            for (size_t j = 0; j < i; j++) {
                free(system->dimensions[j].name);
                free(system->dimensions[j].values);
            }
            free(system->dimensions);
            free(system);
            return NULL;
        }

        for (size_t j = 0; j < dimensions[i].value_count; j++) {
            system->dimensions[i].values[j] = strdup(dimensions[i].values[j]);
        }
    }

    system->dimension_count = dimension_count;
    system->param_count = param_count;

    /* Create hash table */
    system->internal = hash_create(INITIAL_HASH_CAPACITY, dimension_count);
    if (!system->internal) {
        /* Cleanup dimensions... */
        for (size_t i = 0; i < dimension_count; i++) {
            free(system->dimensions[i].name);
            for (size_t j = 0; j < system->dimensions[i].value_count; j++) {
                free(system->dimensions[i].values[j]);
            }
            free(system->dimensions[i].values);
        }
        free(system->dimensions);
        free(system);
        return NULL;
    }

    return system;
}

void evocore_context_system_free(evocore_context_system_t *system) {
    if (!system) return;

    /* Free dimensions */
    if (system->dimensions) {
        for (size_t i = 0; i < system->dimension_count; i++) {
            free(system->dimensions[i].name);
            if (system->dimensions[i].values) {
                for (size_t j = 0; j < system->dimensions[i].value_count; j++) {
                    free(system->dimensions[i].values[j]);
                }
                free(system->dimensions[i].values);
            }
        }
        free(system->dimensions);
    }

    /* Free hash table */
    if (system->internal) {
        hash_free(system->internal);
    }

    free(system);
}

bool evocore_context_add_dimension(
    evocore_context_system_t *system,
    const char *name,
    const char **values,
    size_t value_count
) {
    if (!system || !name || !values || value_count == 0) return false;

    /* Reallocate dimensions array */
    evocore_context_dimension_t *new_dims = realloc(
        system->dimensions,
        (system->dimension_count + 1) * sizeof(evocore_context_dimension_t)
    );
    if (!new_dims) return false;

    system->dimensions = new_dims;

    /* Add new dimension */
    size_t idx = system->dimension_count;
    system->dimensions[idx].name = strdup(name);
    system->dimensions[idx].value_count = value_count;
    system->dimensions[idx].values = calloc(value_count, sizeof(char*));

    for (size_t i = 0; i < value_count; i++) {
        system->dimensions[idx].values[i] = strdup(values[i]);
    }

    system->dimension_count++;

    return true;
}

/*========================================================================
 * Context Keys
 *========================================================================*/

bool evocore_context_build_key(
    const evocore_context_system_t *system,
    const char **dimension_values,
    char *out_key,
    size_t key_size
) {
    if (!system || !dimension_values || !out_key || key_size == 0) {
        return false;
    }

    /* Build key: "value1:value2:value3:..." */
    size_t offset = 0;
    for (size_t i = 0; i < system->dimension_count; i++) {
        if (i > 0) {
            if (offset + 1 >= key_size) return false;
            out_key[offset++] = ':';
        }

        const char *val = dimension_values[i] ? dimension_values[i] : "";
        size_t len = strlen(val);
        if (offset + len >= key_size) return false;

        memcpy(out_key + offset, val, len);
        offset += len;
    }

    out_key[offset] = '\0';
    return true;
}

bool evocore_context_parse_key(
    const evocore_context_system_t *system,
    const char *key,
    char **out_values
) {
    if (!system || !key || !out_values) return false;

    char key_copy[MAX_KEY_LENGTH];
    strncpy(key_copy, key, sizeof(key_copy) - 1);
    key_copy[sizeof(key_copy) - 1] = '\0';

    char *token = strtok(key_copy, ":");
    size_t i = 0;

    while (token && i < system->dimension_count) {
        out_values[i] = strdup(token);
        token = strtok(NULL, ":");
        i++;
    }

    return i == system->dimension_count;
}

bool evocore_context_validate_values(
    const evocore_context_system_t *system,
    const char **dimension_values
) {
    if (!system || !dimension_values) return false;

    for (size_t i = 0; i < system->dimension_count; i++) {
        const char *value = dimension_values[i];
        bool found = false;

        for (size_t j = 0; j < system->dimensions[i].value_count; j++) {
            if (strcmp(value, system->dimensions[i].values[j]) == 0) {
                found = true;
                break;
            }
        }

        if (!found) return false;
    }

    return true;
}

/*========================================================================
 * Learning Operations
 *========================================================================*/

bool evocore_context_learn(
    evocore_context_system_t *system,
    const char **dimension_values,
    const double *parameters,
    size_t param_count,
    double fitness
) {
    if (!system || !dimension_values || !parameters) return false;
    if (param_count != system->param_count) return false;

    /* Build context key */
    char key[MAX_KEY_LENGTH];
    if (!evocore_context_build_key(system, dimension_values, key, sizeof(key))) {
        return false;
    }

    return evocore_context_learn_key(system, key, parameters, param_count, fitness);
}

bool evocore_context_learn_key(
    evocore_context_system_t *system,
    const char *context_key,
    const double *parameters,
    size_t param_count,
    double fitness
) {
    if (!system || !context_key || !parameters) return false;
    if (param_count != system->param_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;

    /* Check for resize */
    if (table->count >= (size_t)(table->capacity * HASH_LOAD_FACTOR)) {
        hash_resize(table, table->capacity * 2);
    }

    /* Get or create entry */
    hash_entry_t *entry = hash_set(table, context_key, param_count);
    if (!entry) return false;

    evocore_context_stats_t *stats = entry->stats;

    /* Update weighted statistics */
    evocore_weighted_array_update(stats->stats, parameters, NULL, param_count, fitness);

    /* Update metadata */
    time_t now = time(NULL);
    if (stats->total_experiences == 0) {
        stats->first_update = now;
    }
    stats->last_update = now;
    stats->total_experiences++;

    /* Update fitness tracking */
    double prev_avg = stats->avg_fitness;
    stats->avg_fitness = (prev_avg * (stats->total_experiences - 1) + fitness) / stats->total_experiences;

    if (fitness > stats->best_fitness) {
        stats->best_fitness = fitness;
    }

    /* Update confidence */
    stats->confidence = evocore_weighted_confidence(
        &stats->stats->stats[0],
        100
    );

    return true;
}

/*========================================================================
 * Statistics Retrieval
 *========================================================================*/

bool evocore_context_get_stats(
    evocore_context_system_t *system,
    const char **dimension_values,
    evocore_context_stats_t **out_stats
) {
    if (!system || !dimension_values || !out_stats) return false;

    char key[MAX_KEY_LENGTH];
    if (!evocore_context_build_key(system, dimension_values, key, sizeof(key))) {
        return false;
    }

    return evocore_context_get_stats_key(system, key, out_stats);
}

bool evocore_context_get_stats_key(
    const evocore_context_system_t *system,
    const char *context_key,
    evocore_context_stats_t **out_stats
) {
    if (!system || !context_key || !out_stats) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);

    if (entry) {
        *out_stats = entry->stats;
        return true;
    }

    *out_stats = NULL;
    return false;
}

bool evocore_context_has_data(
    const evocore_context_stats_t *stats,
    size_t min_samples
) {
    if (!stats) return false;
    if (min_samples == 0) min_samples = DEFAULT_MIN_SAMPLES;

    return stats->total_experiences >= min_samples;
}

/*========================================================================
 * Sampling
 *========================================================================*/

bool evocore_context_sample(
    const evocore_context_system_t *system,
    const char **dimension_values,
    double *out_parameters,
    size_t param_count,
    double exploration_factor,
    unsigned int *seed
) {
    if (!system || !dimension_values || !out_parameters) return false;
    if (param_count != system->param_count) return false;

    char key[MAX_KEY_LENGTH];
    if (!evocore_context_build_key(system, dimension_values, key, sizeof(key))) {
        return false;
    }

    return evocore_context_sample_key(system, key, out_parameters, param_count, exploration_factor, seed);
}

bool evocore_context_sample_key(
    const evocore_context_system_t *system,
    const char *context_key,
    double *out_parameters,
    size_t param_count,
    double exploration_factor,
    unsigned int *seed
) {
    if (!system || !context_key || !out_parameters) return false;
    if (param_count != system->param_count) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, context_key);

    if (!entry) {
        /* No context data, return random */
        for (size_t i = 0; i < param_count; i++) {
            out_parameters[i] = (double)rand_r(seed) / (double)RAND_MAX;
        }
        return true;
    }

    /* Sample from learned distribution */
    return evocore_weighted_array_sample(
        entry->stats->stats,
        out_parameters,
        param_count,
        exploration_factor,
        seed
    );
}

/*========================================================================
 * Query Operations
 *========================================================================*/

/* Comparison function for sorting contexts by fitness */
static int compare_contexts_by_fitness(const void *a, const void *b) {
    const evocore_context_stats_t *ca = *(const evocore_context_stats_t**)a;
    const evocore_context_stats_t *cb = *(const evocore_context_stats_t**)b;

    /* Sort by best_fitness descending */
    if (ca->best_fitness > cb->best_fitness) return -1;
    if (ca->best_fitness < cb->best_fitness) return 1;
    return 0;
}

bool evocore_context_query_best(
    const evocore_context_system_t *system,
    const char *partial_match,
    size_t min_samples,
    evocore_context_query_t *out_results,
    size_t max_results
) {
    if (!system || !out_results) return false;

    /* Allocate temporary array for results */
    evocore_context_stats_t **contexts = calloc(max_results, sizeof(evocore_context_stats_t*));
    if (!contexts) return false;

    hash_table_t *table = (hash_table_t*)system->internal;
    size_t count = 0;

    /* Scan all contexts */
    for (size_t i = 0; i < table->capacity && count < max_results; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry && count < max_results) {
            evocore_context_stats_t *stats = entry->stats;

            /* Check filters */
            bool matches = true;

            if (min_samples > 0 && stats->total_experiences < min_samples) {
                matches = false;
            }

            if (partial_match && matches) {
                if (strstr(stats->key, partial_match) == NULL) {
                    matches = false;
                }
            }

            if (matches) {
                contexts[count++] = stats;
            }

            entry = entry->next;
        }
    }

    /* Sort by fitness */
    qsort(contexts, count, sizeof(evocore_context_stats_t*), compare_contexts_by_fitness);

    /* Set output */
    out_results->contexts = contexts;
    out_results->count = count;
    out_results->capacity = max_results;

    return true;
}

void evocore_context_query_free(evocore_context_query_t *results) {
    if (!results) return;
    free(results->contexts);
    results->contexts = NULL;
    results->count = 0;
}

size_t evocore_context_count(const evocore_context_system_t *system) {
    if (!system) return 0;
    hash_table_t *table = (hash_table_t*)system->internal;
    return table->count;
}

size_t evocore_context_get_keys(
    const evocore_context_system_t *system,
    char **out_keys,
    size_t max_keys
) {
    if (!system || !out_keys) return 0;

    hash_table_t *table = (hash_table_t*)system->internal;
    size_t count = 0;

    for (size_t i = 0; i < table->capacity && count < max_keys; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry && count < max_keys) {
            out_keys[count] = strdup(entry->key);
            count++;
            entry = entry->next;
        }
    }

    return count;
}

/*========================================================================
 * Persistence
 *========================================================================*/

bool evocore_context_save_json(
    const evocore_context_system_t *system,
    const char *filepath
) {
    if (!system || !filepath) return false;

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"dimensions\": [\n");

    for (size_t i = 0; i < system->dimension_count; i++) {
        fprintf(f, "    {\"name\": \"%s\", \"values\": [",
                system->dimensions[i].name);

        for (size_t j = 0; j < system->dimensions[i].value_count; j++) {
            fprintf(f, "\"%s\"%s", system->dimensions[i].values[j],
                    j + 1 < system->dimensions[i].value_count ? ", " : "");
        }

        fprintf(f, "]}%s\n", i + 1 < system->dimension_count ? "," : "");
    }

    fprintf(f, "  ],\n");
    fprintf(f, "  \"param_count\": %zu,\n", system->param_count);
    fprintf(f, "  \"contexts\": {\n");

    hash_table_t *table = (hash_table_t*)system->internal;
    size_t context_idx = 0;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            evocore_context_stats_t *stats = entry->stats;

            fprintf(f, "    \"%s\": {\n", stats->key);
            fprintf(f, "      \"param_count\": %zu,\n", stats->param_count);
            fprintf(f, "      \"total_experiences\": %zu,\n", stats->total_experiences);
            fprintf(f, "      \"confidence\": %.6g,\n", stats->confidence);
            fprintf(f, "      \"avg_fitness\": %.6g,\n", stats->avg_fitness);
            fprintf(f, "      \"best_fitness\": %.6g,\n", stats->best_fitness);

            /* Write means */
            fprintf(f, "      \"means\": [");
            for (size_t j = 0; j < stats->param_count; j++) {
                double mean = evocore_weighted_mean(&stats->stats->stats[j]);
                fprintf(f, "%.6g%s", mean, j + 1 < stats->param_count ? ", " : "");
            }
            fprintf(f, "],\n");

            /* Write stds */
            fprintf(f, "      \"stds\": [");
            for (size_t j = 0; j < stats->param_count; j++) {
                double std = evocore_weighted_std(&stats->stats->stats[j]);
                fprintf(f, "%.6g%s", std, j + 1 < stats->param_count ? ", " : "");
            }
            fprintf(f, "]\n");

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

bool evocore_context_load_json(
    const char *filepath,
    evocore_context_system_t **out_system
) {
    /* TODO: Implement JSON loading */
    (void)filepath;
    (void)out_system;
    return false;
}

bool evocore_context_export_csv(
    const evocore_context_system_t *system,
    const char *filepath
) {
    if (!system || !filepath) return false;

    FILE *f = fopen(filepath, "w");
    if (!f) return false;

    /* Write header */
    fprintf(f, "context");
    for (size_t i = 0; i < system->param_count; i++) {
        fprintf(f, ",param_%zu_mean,param_%zu_std", i, i);
    }
    fprintf(f, ",experiences,confidence,avg_fitness,best_fitness\n");

    /* Write data */
    hash_table_t *table = (hash_table_t*)system->internal;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            evocore_context_stats_t *stats = entry->stats;

            fprintf(f, "%s", stats->key);

            for (size_t j = 0; j < stats->param_count; j++) {
                double mean = evocore_weighted_mean(&stats->stats->stats[j]);
                double std = evocore_weighted_std(&stats->stats->stats[j]);
                fprintf(f, ",%.6g,%.6g", mean, std);
            }

            fprintf(f, ",%zu,%.6g,%.6g,%.6g\n",
                    stats->total_experiences,
                    stats->confidence,
                    stats->avg_fitness,
                    stats->best_fitness);

            entry = entry->next;
        }
    }

    fclose(f);
    return true;
}

/*========================================================================
 * Utility Functions
 *========================================================================*/

bool evocore_context_reset(
    evocore_context_system_t *system,
    const char **dimension_values
) {
    if (!system || !dimension_values) return false;

    char key[MAX_KEY_LENGTH];
    if (!evocore_context_build_key(system, dimension_values, key, sizeof(key))) {
        return false;
    }

    hash_table_t *table = (hash_table_t*)system->internal;
    hash_entry_t *entry = hash_get(table, key);

    if (entry && entry->stats) {
        evocore_weighted_array_reset(entry->stats->stats);
        entry->stats->total_experiences = 0;
        entry->stats->confidence = 0.0;
        entry->stats->avg_fitness = 0.0;
        entry->stats->best_fitness = 0.0;
        entry->stats->first_update = 0;
        entry->stats->last_update = 0;
        return true;
    }

    return false;
}

void evocore_context_reset_all(evocore_context_system_t *system) {
    if (!system) return;

    hash_table_t *table = (hash_table_t*)system->internal;

    for (size_t i = 0; i < table->capacity; i++) {
        hash_entry_t *entry = table->entries[i];
        while (entry) {
            if (entry->stats) {
                evocore_weighted_array_reset(entry->stats->stats);
                entry->stats->total_experiences = 0;
                entry->stats->confidence = 0.0;
                entry->stats->avg_fitness = 0.0;
                entry->stats->best_fitness = 0.0;
                entry->stats->first_update = 0;
                entry->stats->last_update = 0;
            }
            entry = entry->next;
        }
    }
}

double evocore_context_confidence(const evocore_context_stats_t *stats) {
    if (!stats) return 0.0;
    return stats->confidence;
}

bool evocore_context_merge(
    evocore_context_system_t *system,
    const char *target_key,
    const char *source_key
) {
    if (!system || !target_key || !source_key) return false;

    hash_table_t *table = (hash_table_t*)system->internal;

    hash_entry_t *target_entry = hash_get(table, target_key);
    hash_entry_t *source_entry = hash_get(table, source_key);

    if (!target_entry || !source_entry) return false;

    /* Merge weighted stats for each parameter */
    for (size_t i = 0; i < system->param_count; i++) {
        evocore_weighted_merge(
            &target_entry->stats->stats->stats[i],
            &source_entry->stats->stats->stats[i]
        );
    }

    /* Update metadata */
    target_entry->stats->total_experiences += source_entry->stats->total_experiences;
    if (source_entry->stats->best_fitness > target_entry->stats->best_fitness) {
        target_entry->stats->best_fitness = source_entry->stats->best_fitness;
    }

    return true;
}
