#define _GNU_SOURCE
#include "evocore/meta.h"
#include "internal.h"
#include "evocore/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/*========================================================================
 * Default Meta-Parameters
 *========================================================================*/

static const evocore_meta_params_t DEFAULT_META_PARAMS = {
    /* Mutation rates */
    .optimization_mutation_rate = 0.05,
    .variance_mutation_rate = 0.15,
    .experimentation_rate = 0.05,

    /* Selection pressure */
    .elite_protection_ratio = 0.10,
    .culling_ratio = 0.25,
    .fitness_threshold_for_breeding = 0.0,

    /* Population dynamics */
    .target_population_size = 500,
    .min_population_size = 50,
    .max_population_size = 2000,

    /* Learning parameters */
    .learning_rate = 0.1,
    .exploration_factor = 0.3,
    .confidence_threshold = 0.7,

    /* Breeding ratios */
    .profitable_optimization_ratio = 0.80,
    .profitable_random_ratio = 0.05,
    .losing_optimization_ratio = 0.50,
    .losing_random_ratio = 0.25,

    /* Meta-meta parameters */
    .meta_mutation_rate = 0.05,
    .meta_learning_rate = 0.1,
    .meta_convergence_threshold = 0.01
};

/*========================================================================
 * Meta-Parameter Management
 *========================================================================*/

void evocore_meta_params_init(evocore_meta_params_t *params) {
    if (params == NULL) return;
    memcpy(params, &DEFAULT_META_PARAMS, sizeof(evocore_meta_params_t));
}

static int count_set_bits(const char *s) {
    int n = 0;
    while (*s) {
        if (*s == '1') n++;
        s++;
    }
    return n;
}

evocore_error_t evocore_meta_params_validate(const evocore_meta_params_t *params) {
    if (params == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    /* Validate ranges */
    if (params->optimization_mutation_rate < 0.01 ||
        params->optimization_mutation_rate > 0.50) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->variance_mutation_rate < 0.05 ||
        params->variance_mutation_rate > 0.50) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->experimentation_rate < 0.01 ||
        params->experimentation_rate > 0.30) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    if (params->elite_protection_ratio < 0.05 ||
        params->elite_protection_ratio > 0.30) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->culling_ratio < 0.10 ||
        params->culling_ratio > 0.50) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    if (params->target_population_size < 50 ||
        params->target_population_size > 10000) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->min_population_size < 10 ||
        params->min_population_size > params->target_population_size) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->max_population_size < params->target_population_size ||
        params->max_population_size > 20000) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    if (params->learning_rate < 0.01 || params->learning_rate > 1.0) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->exploration_factor < 0.0 || params->exploration_factor > 1.0) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->confidence_threshold < 0.0 || params->confidence_threshold > 1.0) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    if (params->profitable_optimization_ratio < 0.5 ||
        params->profitable_optimization_ratio > 1.0) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->profitable_random_ratio < 0.0 ||
        params->profitable_random_ratio > 0.2) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->losing_optimization_ratio < 0.2 ||
        params->losing_optimization_ratio > 0.8) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->losing_random_ratio < 0.1 ||
        params->losing_random_ratio > 0.5) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    if (params->meta_mutation_rate < 0.01 ||
        params->meta_mutation_rate > 0.20) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->meta_learning_rate < 0.01 ||
        params->meta_learning_rate > 0.50) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    if (params->meta_convergence_threshold < 0.001 ||
        params->meta_convergence_threshold > 0.1) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    return EVOCORE_OK;
}

void evocore_meta_params_mutate(evocore_meta_params_t *params,
                               unsigned int *seed) {
    if (params == NULL) return;

    double rate = params->meta_mutation_rate;

    /* Helper macro for mutation */
#define MUTATE_FIELD(field, min_val, max_val) \
    do { \
        if ((rand_r(seed) % 1000) / 1000.0 < rate) { \
            double delta = ((rand_r(seed) % 1000) / 1000.0 - 0.5) * 0.2; \
            params->field *= (1.0 + delta); \
            if (params->field < (min_val)) params->field = (min_val); \
            if (params->field > (max_val)) params->field = (max_val); \
        } \
    } while(0)

    /* Mutate continuous values */
    MUTATE_FIELD(optimization_mutation_rate, 0.01, 0.50);
    MUTATE_FIELD(variance_mutation_rate, 0.05, 0.50);
    MUTATE_FIELD(experimentation_rate, 0.01, 0.30);

    MUTATE_FIELD(elite_protection_ratio, 0.05, 0.30);
    MUTATE_FIELD(culling_ratio, 0.10, 0.50);

    MUTATE_FIELD(learning_rate, 0.01, 1.0);
    MUTATE_FIELD(exploration_factor, 0.0, 1.0);
    MUTATE_FIELD(confidence_threshold, 0.0, 1.0);

    MUTATE_FIELD(profitable_optimization_ratio, 0.5, 1.0);
    MUTATE_FIELD(profitable_random_ratio, 0.0, 0.2);
    MUTATE_FIELD(losing_optimization_ratio, 0.2, 0.8);
    MUTATE_FIELD(losing_random_ratio, 0.1, 0.5);

    MUTATE_FIELD(meta_mutation_rate, 0.01, 0.20);
    MUTATE_FIELD(meta_learning_rate, 0.01, 0.50);
    MUTATE_FIELD(meta_convergence_threshold, 0.001, 0.1);

#undef MUTATE_FIELD

    /* Mutate integer values with larger steps */
    if ((rand_r(seed) % 1000) / 1000.0 < rate) {
        int change = (rand_r(seed) % 100) - 50;
        params->target_population_size += change;
        if (params->target_population_size < 50) params->target_population_size = 50;
        if (params->target_population_size > 10000) params->target_population_size = 10000;
    }
}

void evocore_meta_params_clone(const evocore_meta_params_t *src,
                              evocore_meta_params_t *dst) {
    if (src == NULL || dst == NULL) return;
    memcpy(dst, src, sizeof(evocore_meta_params_t));
}

/*========================================================================
 * Meta-Individual Management
 *========================================================================*/

evocore_error_t evocore_meta_individual_init(evocore_meta_individual_t *individual,
                                          const evocore_meta_params_t *params,
                                          size_t history_capacity) {
    if (individual == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    memset(individual, 0, sizeof(evocore_meta_individual_t));

    if (params != NULL) {
        evocore_meta_params_clone(params, &individual->params);
    } else {
        evocore_meta_params_init(&individual->params);
    }

    individual->meta_fitness = 0.0;
    individual->generation = 0;
    individual->history_size = 0;

    if (history_capacity > 0) {
        individual->fitness_history = evocore_calloc(history_capacity, sizeof(double));
        if (individual->fitness_history == NULL) {
            return EVOCORE_ERR_OUT_OF_MEMORY;
        }
        individual->history_capacity = history_capacity;
    }

    return EVOCORE_OK;
}

void evocore_meta_individual_cleanup(evocore_meta_individual_t *individual) {
    if (individual == NULL) return;

    evocore_free(individual->fitness_history);
    individual->fitness_history = NULL;
    individual->history_size = 0;
    individual->history_capacity = 0;
}

evocore_error_t evocore_meta_individual_record_fitness(evocore_meta_individual_t *individual,
                                                    double fitness) {
    if (individual == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    individual->meta_fitness = fitness;

    if (individual->fitness_history != NULL &&
        individual->history_capacity > 0) {

        /* Shift history if full */
        if (individual->history_size >= individual->history_capacity) {
            memmove(individual->fitness_history,
                    individual->fitness_history + 1,
                    (individual->history_capacity - 1) * sizeof(double));
            individual->history_size = individual->history_capacity - 1;
        }

        individual->fitness_history[individual->history_size++] = fitness;
    }

    return EVOCORE_OK;
}

double evocore_meta_individual_average_fitness(const evocore_meta_individual_t *individual) {
    if (individual == NULL || individual->history_size == 0) {
        return 0.0;
    }

    double sum = 0.0;
    for (size_t i = 0; i < individual->history_size; i++) {
        sum += individual->fitness_history[i];
    }

    return sum / (double)individual->history_size;
}

double evocore_meta_individual_improvement_trend(const evocore_meta_individual_t *individual) {
    if (individual == NULL || individual->history_size < 2) {
        return 0.0;
    }

    /* Simple linear regression to find trend */
    size_t n = individual->history_size;
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;

    for (size_t i = 0; i < n; i++) {
        double x = (double)i;
        double y = individual->fitness_history[i];
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_x2 += x * x;
    }

    double denominator = n * sum_x2 - sum_x * sum_x;
    if (fabs(denominator) < 0.0001) {
        return 0.0;  /* Avoid division by zero */
    }
    double slope = (n * sum_xy - sum_x * sum_y) / denominator;
    return slope;
}

/*========================================================================
 * Meta-Population Management
 *========================================================================*/

static int compare_meta_individuals(const void *a, const void *b) {
    const evocore_meta_individual_t *ia = (const evocore_meta_individual_t*)a;
    const evocore_meta_individual_t *ib = (const evocore_meta_individual_t*)b;

    if (ia->meta_fitness > ib->meta_fitness) return -1;
    if (ia->meta_fitness < ib->meta_fitness) return 1;
    return 0;
}

evocore_error_t evocore_meta_population_init(evocore_meta_population_t *meta_pop,
                                          int size,
                                          unsigned int *seed) {
    if (meta_pop == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (size < 1 || size > EVOCORE_MAX_META_INDIVIDUALS) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    memset(meta_pop, 0, sizeof(evocore_meta_population_t));
    meta_pop->count = size;
    meta_pop->current_generation = 0;
    meta_pop->best_meta_fitness = -INFINITY;
    meta_pop->initialized = true;

    unsigned int local_seed = seed ? *seed : (unsigned int)time(NULL);

    /* Initialize individuals with varied parameters */
    for (int i = 0; i < size; i++) {
        evocore_error_t err = evocore_meta_individual_init(
            &meta_pop->individuals[i], NULL, 50);

        if (err != EVOCORE_OK) {
            /* Cleanup on error */
            for (int j = 0; j < i; j++) {
                evocore_meta_individual_cleanup(&meta_pop->individuals[j]);
            }
            return err;
        }

        /* Vary parameters for diversity */
        if (i > 0) {
            evocore_meta_params_mutate(&meta_pop->individuals[i].params, &local_seed);
        }
    }

    evocore_log_debug("Meta-population initialized with %d individuals", size);
    return EVOCORE_OK;
}

void evocore_meta_population_cleanup(evocore_meta_population_t *meta_pop) {
    if (meta_pop == NULL || !meta_pop->initialized) {
        return;
    }

    for (int i = 0; i < meta_pop->count; i++) {
        evocore_meta_individual_cleanup(&meta_pop->individuals[i]);
    }

    memset(meta_pop, 0, sizeof(evocore_meta_population_t));
}

evocore_meta_individual_t* evocore_meta_population_best(evocore_meta_population_t *meta_pop) {
    if (meta_pop == NULL || meta_pop->count == 0) {
        return NULL;
    }

    evocore_meta_individual_t *best = &meta_pop->individuals[0];
    for (int i = 1; i < meta_pop->count; i++) {
        if (meta_pop->individuals[i].meta_fitness > best->meta_fitness) {
            best = &meta_pop->individuals[i];
        }
    }

    return best;
}

evocore_error_t evocore_meta_population_evolve(evocore_meta_population_t *meta_pop,
                                            unsigned int *seed) {
    if (meta_pop == NULL || !meta_pop->initialized) {
        return EVOCORE_ERR_NULL_PTR;
    }

    unsigned int local_seed = seed ? *seed : (unsigned int)time(NULL);

    /* Sort by fitness */
    evocore_meta_population_sort(meta_pop);

    /* Update best */
    evocore_meta_individual_t *best = &meta_pop->individuals[0];
    if (best->meta_fitness > meta_pop->best_meta_fitness) {
        meta_pop->best_meta_fitness = best->meta_fitness;
        evocore_meta_params_clone(&best->params, &meta_pop->best_params);
    }

    /* Elitism: keep top 30% */
    int elite_count = (int)(meta_pop->count * 0.3);
    if (elite_count < 1) elite_count = 1;

    /* Replace bottom 50% with children of elite */
    int replace_start = meta_pop->count - (int)(meta_pop->count * 0.5);

    for (int i = replace_start; i < meta_pop->count; i++) {
        /* Select two parents from elite */
        int p1 = rand_r(&local_seed) % elite_count;
        int p2 = rand_r(&local_seed) % elite_count;

        /* Clone better parent */
        int better = (meta_pop->individuals[p1].meta_fitness >
                      meta_pop->individuals[p2].meta_fitness) ? p1 : p2;

        evocore_meta_individual_cleanup(&meta_pop->individuals[i]);
        evocore_meta_individual_init(&meta_pop->individuals[i],
                                    &meta_pop->individuals[better].params,
                                    50);

        /* Mutate */
        evocore_meta_params_mutate(&meta_pop->individuals[i].params, &local_seed);
    }

    meta_pop->current_generation++;

    evocore_log_trace("Meta-population evolved to generation %d",
                    meta_pop->current_generation);

    return EVOCORE_OK;
}

void evocore_meta_population_sort(evocore_meta_population_t *meta_pop) {
    if (meta_pop == NULL || !meta_pop->initialized) {
        return;
    }

    qsort(meta_pop->individuals, meta_pop->count,
          sizeof(evocore_meta_individual_t), compare_meta_individuals);
}

bool evocore_meta_population_converged(const evocore_meta_population_t *meta_pop,
                                      double threshold,
                                      int generations) {
    if (meta_pop == NULL || !meta_pop->initialized) {
        return false;
    }

    if (meta_pop->current_generation < generations) {
        return false;
    }

    /* Check if best fitness hasn't improved significantly */
    evocore_meta_individual_t *best = evocore_meta_population_best(meta_pop);
    if (best == NULL) return false;

    /* Use improvement trend as convergence indicator */
    double trend = evocore_meta_individual_improvement_trend(best);
    return fabs(trend) < threshold;
}

/*========================================================================
 * Meta-Evaluation
 *========================================================================*/

double evocore_meta_evaluate(const evocore_meta_params_t *params,
                            double best_fitness,
                            double avg_fitness,
                            double diversity,
                            int generations) {
    if (params == NULL) {
        return 0.0;
    }

    /* Meta-fitness combines multiple factors */
    double score = 0.0;

    /* 1. Best fitness achieved (50% weight) */
    score += best_fitness * 0.5;

    /* 2. Average fitness (20% weight) */
    score += avg_fitness * 0.2;

    /* 3. Population diversity bonus (20% weight) */
    /* Reward maintaining diversity around 0.3-0.5 */
    double diversity_bonus = diversity;
    if (diversity > 0.3 && diversity < 0.5) {
        diversity_bonus *= 1.2;
    }
    score += diversity_bonus * 100.0 * 0.2;

    /* 4. Efficiency - generations to reach this fitness (10% weight) */
    /* Fewer generations = better */
    if (generations > 0) {
        score += (1000.0 / (double)generations) * 0.1;
    }

    return score;
}

/*========================================================================
 * Utility Functions
 *========================================================================*/

void evocore_meta_params_print(const evocore_meta_params_t *params) {
    if (params == NULL) {
        printf("Meta-params: NULL\n");
        return;
    }

    printf("=== Meta-Parameters ===\n");

    printf("Mutation Rates:\n");
    printf("  optimization_mutation_rate: %.4f\n", params->optimization_mutation_rate);
    printf("  variance_mutation_rate:     %.4f\n", params->variance_mutation_rate);
    printf("  experimentation_rate:        %.4f\n", params->experimentation_rate);

    printf("Selection Pressure:\n");
    printf("  elite_protection_ratio:      %.4f\n", params->elite_protection_ratio);
    printf("  culling_ratio:               %.4f\n", params->culling_ratio);
    printf("  fitness_threshold:           %.4f\n", params->fitness_threshold_for_breeding);

    printf("Population Dynamics:\n");
    printf("  target_population_size:      %d\n", params->target_population_size);
    printf("  min_population_size:         %d\n", params->min_population_size);
    printf("  max_population_size:         %d\n", params->max_population_size);

    printf("Learning:\n");
    printf("  learning_rate:               %.4f\n", params->learning_rate);
    printf("  exploration_factor:          %.4f\n", params->exploration_factor);
    printf("  confidence_threshold:        %.4f\n", params->confidence_threshold);

    printf("Breeding Ratios:\n");
    printf("  profitable_opt_ratio:        %.4f\n", params->profitable_optimization_ratio);
    printf("  profitable_rand_ratio:       %.4f\n", params->profitable_random_ratio);
    printf("  losing_opt_ratio:            %.4f\n", params->losing_optimization_ratio);
    printf("  losing_rand_ratio:           %.4f\n", params->losing_random_ratio);

    printf("Meta-Meta:\n");
    printf("  meta_mutation_rate:          %.4f\n", params->meta_mutation_rate);
    printf("  meta_learning_rate:          %.4f\n", params->meta_learning_rate);
    printf("  meta_convergence_threshold:  %.4f\n", params->meta_convergence_threshold);
}

typedef struct {
    const char *name;
    size_t offset;
} meta_param_entry_t;

#define OFFSET(field) offsetof(evocore_meta_params_t, field)

static const meta_param_entry_t param_table[] = {
    {"optimization_mutation_rate", OFFSET(optimization_mutation_rate)},
    {"variance_mutation_rate", OFFSET(variance_mutation_rate)},
    {"experimentation_rate", OFFSET(experimentation_rate)},
    {"elite_protection_ratio", OFFSET(elite_protection_ratio)},
    {"culling_ratio", OFFSET(culling_ratio)},
    {"fitness_threshold_for_breeding", OFFSET(fitness_threshold_for_breeding)},
    {"target_population_size", OFFSET(target_population_size)},
    {"min_population_size", OFFSET(min_population_size)},
    {"max_population_size", OFFSET(max_population_size)},
    {"learning_rate", OFFSET(learning_rate)},
    {"exploration_factor", OFFSET(exploration_factor)},
    {"confidence_threshold", OFFSET(confidence_threshold)},
    {"profitable_optimization_ratio", OFFSET(profitable_optimization_ratio)},
    {"profitable_random_ratio", OFFSET(profitable_random_ratio)},
    {"losing_optimization_ratio", OFFSET(losing_optimization_ratio)},
    {"losing_random_ratio", OFFSET(losing_random_ratio)},
    {"meta_mutation_rate", OFFSET(meta_mutation_rate)},
    {"meta_learning_rate", OFFSET(meta_learning_rate)},
    {"meta_convergence_threshold", OFFSET(meta_convergence_threshold)},
    {NULL, 0}
};

double evocore_meta_params_get(const evocore_meta_params_t *params,
                              const char *name) {
    if (params == NULL || name == NULL) {
        return 0.0;
    }

    for (int i = 0; param_table[i].name != NULL; i++) {
        if (strcmp(name, param_table[i].name) == 0) {
            const double *ptr = (const double*)((const char*)params + param_table[i].offset);
            return *ptr;
        }
    }

    return 0.0;
}

evocore_error_t evocore_meta_params_set(evocore_meta_params_t *params,
                                      const char *name,
                                      double value) {
    if (params == NULL || name == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    for (int i = 0; param_table[i].name != NULL; i++) {
        if (strcmp(name, param_table[i].name) == 0) {
            double *ptr = (double*)((char*)params + param_table[i].offset);
            *ptr = value;
            return EVOCORE_OK;
        }
    }

    return EVOCORE_ERR_INVALID_ARG;
}

#include <stddef.h>  /* For offsetof */
