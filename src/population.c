#define _GNU_SOURCE
#include "evocore/population.h"
#include "internal.h"
#include "evocore/log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

/*========================================================================
 * Internal Helpers
 *========================================================================*/

static int compare_individuals_desc(const void *a, const void *b) {
    const evocore_individual_t *ia = (const evocore_individual_t*)a;
    const evocore_individual_t *ib = (const evocore_individual_t*)b;

    if (isnan(ia->fitness) && isnan(ib->fitness)) return 0;
    if (isnan(ia->fitness)) return 1;
    if (isnan(ib->fitness)) return -1;

    if (ia->fitness < ib->fitness) return 1;
    if (ia->fitness > ib->fitness) return -1;
    return 0;
}

/*========================================================================
 * Population Lifecycle
 *========================================================================*/

evocore_error_t evocore_population_init(evocore_population_t *pop,
                                     size_t capacity) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (capacity == 0) return EVOCORE_ERR_POP_SIZE;

    memset(pop, 0, sizeof(evocore_population_t));

    pop->individuals = evocore_calloc(capacity, sizeof(evocore_individual_t));
    if (!pop->individuals) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    pop->capacity = capacity;
    pop->size = 0;
    pop->generation = 0;
    pop->best_fitness = -INFINITY;
    pop->worst_fitness = INFINITY;

    return EVOCORE_OK;
}

void evocore_population_cleanup(evocore_population_t *pop) {
    if (!pop) return;

    evocore_population_clear(pop);
    evocore_free(pop->individuals);

    pop->individuals = NULL;
    pop->capacity = 0;
}

void evocore_population_clear(evocore_population_t *pop) {
    if (!pop) return;

    for (size_t i = 0; i < pop->size; i++) {
        if (pop->individuals[i].genome) {
            evocore_genome_cleanup(pop->individuals[i].genome);
            evocore_free(pop->individuals[i].genome);
            pop->individuals[i].genome = NULL;
        }
        pop->individuals[i].fitness = NAN;
    }

    pop->size = 0;
    pop->generation = 0;
    pop->best_fitness = -INFINITY;
    pop->worst_fitness = INFINITY;
    pop->avg_fitness = NAN;
    pop->best_index = 0;
}

/*========================================================================
 * Population Manipulation
 *========================================================================*/

evocore_error_t evocore_population_add(evocore_population_t *pop,
                                    const evocore_genome_t *genome,
                                    double fitness) {
    if (!pop || !genome) return EVOCORE_ERR_NULL_PTR;

    if (pop->size >= pop->capacity) {
        return EVOCORE_ERR_POP_FULL;
    }

    /* Allocate new genome */
    evocore_genome_t *new_genome = evocore_malloc(sizeof(evocore_genome_t));
    if (!new_genome) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    evocore_error_t err = evocore_genome_clone(genome, new_genome);
    if (err != EVOCORE_OK) {
        evocore_free(new_genome);
        return err;
    }

    pop->individuals[pop->size].genome = new_genome;
    pop->individuals[pop->size].fitness = fitness;
    pop->size++;

    return EVOCORE_OK;
}

evocore_error_t evocore_population_remove(evocore_population_t *pop,
                                       size_t index) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (index >= pop->size) return EVOCORE_ERR_INVALID_ARG;

    /* Free genome */
    if (pop->individuals[index].genome) {
        evocore_genome_cleanup(pop->individuals[index].genome);
        evocore_free(pop->individuals[index].genome);
    }

    /* Shift remaining individuals */
    for (size_t i = index; i < pop->size - 1; i++) {
        pop->individuals[i] = pop->individuals[i + 1];
    }

    pop->size--;
    pop->individuals[pop->size].genome = NULL;
    pop->individuals[pop->size].fitness = NAN;

    return EVOCORE_OK;
}

evocore_error_t evocore_population_resize(evocore_population_t *pop,
                                      size_t new_capacity) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (new_capacity == 0) return EVOCORE_ERR_POP_SIZE;

    evocore_individual_t *new_individuals = evocore_realloc(
        pop->individuals,
        new_capacity * sizeof(evocore_individual_t)
    );

    if (!new_individuals) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    /* Zero out new memory */
    if (new_capacity > pop->capacity) {
        size_t extra = new_capacity - pop->capacity;
        memset(new_individuals + pop->capacity, 0,
               extra * sizeof(evocore_individual_t));
    } else if (pop->size > new_capacity) {
        /* Truncate if shrinking */
        for (size_t i = new_capacity; i < pop->size; i++) {
            if (pop->individuals[i].genome) {
                evocore_genome_cleanup(pop->individuals[i].genome);
                evocore_free(pop->individuals[i].genome);
            }
        }
        if (pop->size > new_capacity) {
            pop->size = new_capacity;
        }
    }

    pop->individuals = new_individuals;
    pop->capacity = new_capacity;

    return EVOCORE_OK;
}

evocore_error_t evocore_population_set_size(evocore_population_t *pop,
                                        size_t size) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (size > pop->capacity) return EVOCORE_ERR_INVALID_ARG;
    pop->size = size;
    return EVOCORE_OK;
}

/*========================================================================
 * Population Queries
 *========================================================================*/

evocore_individual_t* evocore_population_get(evocore_population_t *pop,
                                         size_t index) {
    if (!pop || index >= pop->size) return NULL;
    return &pop->individuals[index];
}

evocore_individual_t* evocore_population_get_best(evocore_population_t *pop) {
    if (!pop || pop->size == 0) return NULL;
    if (pop->best_index < pop->size) {
        return &pop->individuals[pop->best_index];
    }
    return NULL;
}

size_t evocore_population_size(const evocore_population_t *pop) {
    return pop ? pop->size : 0;
}

size_t evocore_population_capacity(const evocore_population_t *pop) {
    return pop ? pop->capacity : 0;
}

size_t evocore_population_generation(const evocore_population_t *pop) {
    return pop ? pop->generation : 0;
}

void evocore_population_increment_generation(evocore_population_t *pop) {
    if (pop) pop->generation++;
}

/*========================================================================
 * Population Statistics
 *========================================================================*/

evocore_error_t evocore_population_update_stats(evocore_population_t *pop) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (pop->size == 0) {
        pop->best_fitness = -INFINITY;
        pop->worst_fitness = INFINITY;
        pop->avg_fitness = NAN;
        pop->best_index = 0;
        return EVOCORE_OK;
    }

    double sum = 0.0;
    double best = -INFINITY;
    double worst = INFINITY;
    size_t best_idx = 0;

    for (size_t i = 0; i < pop->size; i++) {
        double f = pop->individuals[i].fitness;

        /* Skip NaN values */
        if (isnan(f)) continue;

        sum += f;

        if (f > best) {
            best = f;
            best_idx = i;
        }

        if (f < worst) {
            worst = f;
        }
    }

    pop->best_fitness = best;
    pop->worst_fitness = (worst == INFINITY) ? -INFINITY : worst;
    pop->best_index = best_idx;

    /* Count valid individuals for average */
    size_t valid_count = 0;
    for (size_t i = 0; i < pop->size; i++) {
        if (!isnan(pop->individuals[i].fitness)) {
            valid_count++;
        }
    }

    pop->avg_fitness = (valid_count > 0) ? (sum / valid_count) : NAN;

    return EVOCORE_OK;
}

evocore_error_t evocore_population_sort(evocore_population_t *pop) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (pop->size < 2) return EVOCORE_OK;

    qsort(pop->individuals, pop->size,
          sizeof(evocore_individual_t), compare_individuals_desc);

    /* Update best index after sort */
    evocore_population_update_stats(pop);

    return EVOCORE_OK;
}

/*========================================================================
 * Evolution Operations
 *========================================================================*/

size_t evocore_population_tournament_select(const evocore_population_t *pop,
                                          size_t tournament_size,
                                          unsigned int *seed) {
    if (!pop || !seed) return SIZE_MAX;
    if (pop->size == 0) return SIZE_MAX;

    size_t best_idx = rand_r(seed) % pop->size;
    double best_fitness = pop->individuals[best_idx].fitness;

    /* Adjust tournament size if population is smaller */
    if (tournament_size > pop->size) {
        tournament_size = pop->size;
    }

    for (size_t i = 1; i < tournament_size; i++) {
        size_t idx = rand_r(seed) % pop->size;
        double f = pop->individuals[idx].fitness;

        if (!isnan(f) && (isnan(best_fitness) || f > best_fitness)) {
            best_fitness = f;
            best_idx = idx;
        }
    }

    return best_idx;
}

evocore_error_t evocore_population_truncate(evocore_population_t *pop,
                                        size_t n) {
    if (!pop) return EVOCORE_ERR_NULL_PTR;
    if (n > pop->capacity) n = pop->capacity;

    /* Remove individuals from n to size */
    while (pop->size > n) {
        EVOCORE_CHECK(evocore_population_remove(pop, pop->size - 1));
    }

    return EVOCORE_OK;
}

size_t evocore_population_evaluate(evocore_population_t *pop,
                                  evocore_fitness_func_t fitness_func,
                                  void *context) {
    if (!pop || !fitness_func) return 0;

    size_t evaluated = 0;
    for (size_t i = 0; i < pop->size; i++) {
        if (isnan(pop->individuals[i].fitness)) {
            double fitness = fitness_func(pop->individuals[i].genome, context);
            pop->individuals[i].fitness = fitness;
            evaluated++;
        }
    }

    if (evaluated > 0) {
        evocore_population_update_stats(pop);
    }

    return evaluated;
}

/*========================================================================
 * Genetic Operators
 *========================================================================*/

evocore_error_t evocore_genome_crossover(const evocore_genome_t *parent1,
                                      const evocore_genome_t *parent2,
                                      evocore_genome_t *child1,
                                      evocore_genome_t *child2,
                                      unsigned int *seed) {
    if (!parent1 || !parent2 || !child1 || !child2 || !seed) {
        return EVOCORE_ERR_NULL_PTR;
    }

    size_t size = parent1->size < parent2->size ? parent1->size : parent2->size;

    /* Initialize children */
    EVOCORE_CHECK(evocore_genome_init(child1, size));
    EVOCORE_CHECK(evocore_genome_set_size(child1, size));
    EVOCORE_CHECK(evocore_genome_init(child2, size));
    EVOCORE_CHECK(evocore_genome_set_size(child2, size));

    const unsigned char *p1_data = (const unsigned char*)parent1->data;
    const unsigned char *p2_data = (const unsigned char*)parent2->data;
    unsigned char *c1_data = (unsigned char*)child1->data;
    unsigned char *c2_data = (unsigned char*)child2->data;

    /* Uniform crossover */
    for (size_t i = 0; i < size; i++) {
        if (rand_r(seed) & 1) {
            c1_data[i] = p1_data[i];
            c2_data[i] = p2_data[i];
        } else {
            c1_data[i] = p2_data[i];
            c2_data[i] = p1_data[i];
        }
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_mutate(evocore_genome_t *genome,
                                   double rate,
                                   unsigned int *seed) {
    if (!genome || !seed) return EVOCORE_ERR_NULL_PTR;
    if (!genome->data) return EVOCORE_ERR_GENOME_EMPTY;

    unsigned char *data = (unsigned char*)genome->data;
    for (size_t i = 0; i < genome->size; i++) {
        double r = (double)rand_r(seed) / (double)RAND_MAX;
        if (r < rate) {
            data[i] = (unsigned char)rand_r(seed);
        }
    }

    return EVOCORE_OK;
}
