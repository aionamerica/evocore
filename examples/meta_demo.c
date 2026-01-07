/**
 * Meta-Evolution Demo for evocore
 *
 * This example demonstrates the meta-evolution layer, where
 * the evolutionary parameters themselves are evolved.
 *
 * We use a simple optimization problem (maximizing sum of bytes)
 * as the base domain, and evolve the meta-parameters that control
 * how evolution happens.
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifndef NAN
#define NAN (0.0 / 0.0)
#endif

/*========================================================================
 * Simple Test Domain
 *========================================================================*/

typedef struct {
    double target_sum;
} simple_context_t;

static void simple_random_init(evocore_genome_t *genome, void *context) {
    (void)context;
    evocore_genome_randomize(genome);
    evocore_genome_set_size(genome, genome->capacity);
}

static void simple_mutate(evocore_genome_t *genome, double rate, void *context) {
    (void)context;
    /* Flip bytes based on mutation rate */
    unsigned char *data = (unsigned char*)genome->data;
    size_t num_flips = (size_t)(genome->size * rate);
    for (size_t i = 0; i < num_flips && i < genome->size; i++) {
        size_t idx = rand() % genome->size;
        data[idx] = (unsigned char)rand();
    }
}

static void simple_crossover(const evocore_genome_t *parent1,
                             const evocore_genome_t *parent2,
                             evocore_genome_t *child1,
                             evocore_genome_t *child2,
                             void *context) {
    (void)context;
    size_t point = parent1->size / 2;

    /* Child 1 */
    memcpy(child1->data, parent1->data, point);
    memcpy((char*)child1->data + point, (char*)parent2->data + point,
           parent2->size - point);
    child1->size = parent1->size;

    /* Child 2 */
    memcpy(child2->data, parent2->data, point);
    memcpy((char*)child2->data + point, (char*)parent1->data + point,
           parent1->size - point);
    child2->size = parent2->size;
}

static double simple_diversity(const evocore_genome_t *a,
                                const evocore_genome_t *b,
                                void *context) {
    (void)context;
    size_t distance = 0;
    evocore_genome_distance(a, b, &distance);
    size_t min_size = a->size < b->size ? a->size : b->size;
    if (min_size == 0) return 0.0;
    return (double)distance / (double)min_size;
}

static double simple_fitness(const evocore_genome_t *genome, void *context) {
    simple_context_t *ctx = (simple_context_t*)context;

    /* Fitness is sum of bytes (higher is better) */
    unsigned char *data = (unsigned char*)genome->data;
    double sum = 0.0;
    for (size_t i = 0; i < genome->size; i++) {
        sum += data[i];
    }

    return sum;
}

/*========================================================================
 * Meta-Evaluation
 *========================================================================*/

/**
 * Run a short evolution with given meta-parameters
 * and return the meta-fitness
 */
static double evaluate_meta_params(const evocore_meta_params_t *params,
                                    int generations) {
    /* Setup domain */
    simple_context_t ctx = { .target_sum = 255.0 * 64 };

    /* Create population */
    evocore_population_t pop;
    evocore_population_init(&pop, 50);

    /* Initialize with random genomes */
    evocore_genome_t temp;
    evocore_genome_init(&temp, 64);
    for (int i = 0; i < 50; i++) {
        simple_random_init(&temp, NULL);
        /* Use NaN for unevaluated fitness */
        evocore_population_add(&pop, &temp, NAN);
    }
    evocore_genome_cleanup(&temp);

    /* Run evolution with given meta-parameters */
    double best_fitness = 0.0;
    double fitness_sum = 0.0;
    double diversity_sum = 0.0;

    for (int gen = 0; gen < generations; gen++) {
        /* Evaluate fitness */
        evocore_population_evaluate(&pop, simple_fitness, &ctx);
        evocore_population_update_stats(&pop);
        evocore_population_sort(&pop);

        /* Track stats */
        size_t pop_size = evocore_population_size(&pop);
        if (pop_size > 0) {
            double gen_best = pop.best_fitness;
            if (gen_best > best_fitness) best_fitness = gen_best;

            /* Average fitness */
            fitness_sum += pop.avg_fitness;

            /* Calculate diversity */
            double gen_diversity = 0.0;
            int comparisons = 0;
            for (size_t i = 0; i < pop_size && i < 10; i++) {
                evocore_individual_t *ind1 = evocore_population_get(&pop, i);
                for (size_t j = i + 1; j < pop_size && j < 10; j++) {
                    evocore_individual_t *ind2 = evocore_population_get(&pop, j);
                    if (ind1 && ind2 && ind1->genome && ind2->genome) {
                        gen_diversity += simple_diversity(
                            ind1->genome, ind2->genome, NULL);
                        comparisons++;
                    }
                }
            }
            if (comparisons > 0) {
                diversity_sum += gen_diversity / comparisons;
            }
        }

        /* Apply meta-parameters for evolution */
        int elite = (int)(pop_size * params->elite_protection_ratio);
        int cull = (int)(pop_size * params->culling_ratio);

        /* Remove worst */
        for (int i = 0; i < cull && (int)pop_size > params->min_population_size; i++) {
            size_t last_idx = evocore_population_size(&pop) - 1;
            evocore_population_remove(&pop, last_idx);
            pop_size = evocore_population_size(&pop);
        }

        /* Create offspring */
        int target = params->target_population_size;
        if (target > params->max_population_size) target = params->max_population_size;
        if (target < params->min_population_size) target = params->min_population_size;

        pop_size = evocore_population_size(&pop);

        while ((int)pop_size < target) {
            /* Tournament selection */
            pop_size = evocore_population_size(&pop);
            if (pop_size <= elite) break;

            int i1 = rand() % (pop_size - elite);
            int i2 = rand() % (pop_size - elite);
            evocore_individual_t *ind1 = evocore_population_get(&pop, i1);
            evocore_individual_t *ind2 = evocore_population_get(&pop, i2);
            if (!ind1 || !ind2) break;

            int p1 = (ind1->fitness > ind2->fitness) ? i1 : i2;

            i1 = rand() % (pop_size - elite);
            i2 = rand() % (pop_size - elite);
            ind1 = evocore_population_get(&pop, i1);
            ind2 = evocore_population_get(&pop, i2);
            if (!ind1 || !ind2) break;

            int p2 = (ind1->fitness > ind2->fitness) ? i1 : i2;

            /* Create child genome */
            evocore_individual_t *parent = evocore_population_get(&pop, p1);
            if (!parent || !parent->genome) break;

            evocore_genome_t child_genome;
            evocore_genome_init(&child_genome, 64);
            evocore_genome_clone(parent->genome, &child_genome);

            /* Mutate based on params */
            double mutation_choice = (double)rand() / RAND_MAX;
            if (mutation_choice < params->experimentation_rate) {
                /* Random reinit */
                simple_random_init(&child_genome, NULL);
            } else if (mutation_choice < params->experimentation_rate +
                                   params->optimization_mutation_rate) {
                /* Optimize around existing */
                simple_mutate(&child_genome, params->variance_mutation_rate, NULL);
            } else {
                /* Standard mutation */
                simple_mutate(&child_genome, params->optimization_mutation_rate, NULL);
            }

            evocore_population_add(&pop, &child_genome, NAN);
            evocore_genome_cleanup(&child_genome);

            pop_size = evocore_population_size(&pop);
        }
    }

    evocore_population_cleanup(&pop);

    /* Return meta-fitness */
    return evocore_meta_evaluate(params, best_fitness,
                               fitness_sum / generations,
                               diversity_sum / generations,
                               generations);
}

/*========================================================================
 * Main
 *========================================================================*/

int main(void) {
    printf("=== Meta-Evolution Demo ===\n\n");

    evocore_log_set_level(EVOCORE_LOG_WARN);
    srand(42);

    /* Initialize meta-population */
    evocore_meta_population_t meta_pop;
    evocore_error_t err = evocore_meta_population_init(&meta_pop, 10, NULL);
    if (err != EVOCORE_OK) {
        printf("Failed to initialize meta-population: %d\n", err);
        return 1;
    }

    printf("Meta-population initialized with %d meta-individuals\n\n",
           meta_pop.count);

    /* Display initial meta-parameters */
    printf("Initial meta-parameters (first 3):\n");
    for (int i = 0; i < 3 && i < meta_pop.count; i++) {
        printf("  [%d] mutation_rate: %.3f, elite_ratio: %.2f, cull: %.2f\n",
               i, meta_pop.individuals[i].params.optimization_mutation_rate,
               meta_pop.individuals[i].params.elite_protection_ratio,
               meta_pop.individuals[i].params.culling_ratio);
    }
    printf("\n");

    /* Run meta-evolution */
    printf("Running meta-evolution (5 meta-generations)...\n");
    printf("Each meta-individual is evaluated by running 20 generations\n\n");

    for (int meta_gen = 0; meta_gen < 5; meta_gen++) {
        printf("--- Meta-generation %d ---\n", meta_gen + 1);

        /* Evaluate each meta-individual */
        for (int i = 0; i < meta_pop.count; i++) {
            double meta_fitness = evaluate_meta_params(
                &meta_pop.individuals[i].params, 20);

            evocore_meta_individual_record_fitness(&meta_pop.individuals[i],
                                                  meta_fitness);

            printf("  [%2d] Meta-fitness: %.2f  (mutation: %.3f, elite: %.2f)\n",
                   i, meta_fitness,
                   meta_pop.individuals[i].params.optimization_mutation_rate,
                   meta_pop.individuals[i].params.elite_protection_ratio);
        }

        /* Evolve meta-population */
        if (meta_gen < 4) {  /* Don't evolve after last eval */
            evocore_meta_population_evolve(&meta_pop, NULL);
        }

        printf("  Best meta-fitness so far: %.2f\n\n",
               meta_pop.best_meta_fitness);
    }

    /* Display best meta-parameters found */
    evocore_meta_individual_t *best = evocore_meta_population_best(&meta_pop);
    if (best) {
        printf("\n=== Best Meta-Parameters Found ===\n");
        evocore_meta_params_print(&best->params);
        printf("\nBest meta-fitness: %.2f\n", best->meta_fitness);
    }

    /* Show adaptive parameter suggestions */
    printf("\n=== Adaptive Suggestions ===\n");

    evocore_meta_params_t suggested;
    evocore_meta_params_init(&suggested);

    evocore_meta_suggest_mutation_rate(0.2, &suggested);
    printf("For diversity=0.20 (low):\n");
    printf("  mutation_rate: %.4f\n", suggested.optimization_mutation_rate);

    evocore_meta_suggest_mutation_rate(0.5, &suggested);
    printf("For diversity=0.50 (high):\n");
    printf("  mutation_rate: %.4f\n", suggested.optimization_mutation_rate);

    evocore_meta_suggest_selection_pressure(0.05, &suggested);
    printf("\nFor fitness_stddev=0.05 (tight):\n");
    printf("  elite_ratio: %.4f, cull_ratio: %.4f\n",
           suggested.elite_protection_ratio, suggested.culling_ratio);

    evocore_meta_suggest_selection_pressure(0.3, &suggested);
    printf("For fitness_stddev=0.30 (wide):\n");
    printf("  elite_ratio: %.4f, cull_ratio: %.4f\n",
           suggested.elite_protection_ratio, suggested.culling_ratio);

    /* Demonstrate online learning */
    printf("\n=== Online Learning Demo ===\n");
    evocore_meta_reset_learning();

    /* Simulate learning from outcomes */
    for (int i = 0; i < 50; i++) {
        double mut_rate = 0.01 + ((double)rand() / RAND_MAX) * 0.2;
        double expl = (double)rand() / RAND_MAX;
        double fitness = 100.0 + mut_rate * 500.0 +
                        (1.0 - fabs(expl - 0.3)) * 200.0;  /* Prefer ~0.3 exploration */
        evocore_meta_learn_outcome(mut_rate, expl, fitness, 0.1);
    }

    double learned_mut, learned_expl;
    if (evocore_meta_get_learned_params(&learned_mut, &learned_expl, 5)) {
        printf("Learned optimal parameters:\n");
        printf("  mutation_rate: %.4f\n", learned_mut);
        printf("  exploration:   %.4f\n", learned_expl);
    }

    /* Cleanup */
    evocore_meta_population_cleanup(&meta_pop);

    printf("\n=== Demo Complete ===\n");
    return 0;
}
