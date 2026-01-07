/**
 * Sphere Function Optimization Example
 *
 * Demonstrates the evocore framework by optimizing the sphere function:
 *   f(x) = sum(x_i^2)
 *
 * The minimum is at x_i = 0 for all i, with f(x) = 0.
 * Since the framework maximizes fitness, we return -f(x).
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DIMENSIONS 10
#define GENE_BYTES (DIMENSIONS * sizeof(double))

typedef struct {
    double mins[DIMENSIONS];
    double maxs[DIMENSIONS];
    size_t eval_count;
} sphere_context_t;

/**
 * Sphere function fitness evaluation
 *
 * Since evocore maximizes fitness, we return the negative of the sphere value.
 */
double sphere_fitness(const evocore_genome_t *genome, void *context) {
    sphere_context_t *ctx = (sphere_context_t *)context;
    ctx->eval_count++;

    double values[DIMENSIONS];
    evocore_error_t err = evocore_genome_read(genome, 0, values, sizeof(values));
    if (err != EVOCORE_OK) {
        return NAN;
    }

    /* Calculate sphere function: sum of squares */
    double sum = 0.0;
    for (int i = 0; i < DIMENSIONS; i++) {
        sum += values[i] * values[i];
    }

    /* Return negative because we minimize, framework maximizes */
    return -sum;
}

/**
 * Initialize a random genome within bounds
 */
void init_genome(evocore_genome_t *genome, sphere_context_t *ctx,
                 unsigned int *seed) {
    double values[DIMENSIONS];
    for (int i = 0; i < DIMENSIONS; i++) {
        double range = ctx->maxs[i] - ctx->mins[i];
        values[i] = ctx->mins[i] + ((double)rand_r(seed) / RAND_MAX) * range;
    }
    evocore_genome_write(genome, 0, values, sizeof(values));
}

/**
 * Main evolution loop
 */
int main(int argc, char **argv) {
    /* Parse command line */
    const char *config_path = "sphere_config.ini";
    if (argc > 1) {
        config_path = argv[1];
    }

    /* Load configuration */
    evocore_config_t *config = NULL;
    evocore_error_t err = evocore_config_load(config_path, &config);
    if (err != EVOCORE_OK) {
        fprintf(stderr, "Failed to load config: %s\n",
                evocore_error_string(err));
        return 1;
    }

    /* Get configuration values */
    size_t population_size = (size_t)evocore_config_get_int(config, NULL,
                                                          "population_size", 100);
    int max_generations = evocore_config_get_int(config, "evolution",
                                               "max_generations", 100);
    double mutation_rate = evocore_config_get_double(config, "mutation",
                                                    "rate", 0.1);
    int tournament_size = evocore_config_get_int(config, "selection",
                                                "tournament_size", 3);
    size_t elite_count = (size_t)evocore_config_get_int(config, "selection",
                                                       "elite_count", 5);
    const char *log_level_str = evocore_config_get_string(config, "logging",
                                                         "level", "INFO");

    /* Set log level */
    evocore_log_level_t log_level = EVOCORE_LOG_INFO;
    if (strcmp(log_level_str, "TRACE") == 0) log_level = EVOCORE_LOG_TRACE;
    else if (strcmp(log_level_str, "DEBUG") == 0) log_level = EVOCORE_LOG_DEBUG;
    else if (strcmp(log_level_str, "WARN") == 0) log_level = EVOCORE_LOG_WARN;
    evocore_log_set_level(log_level);

    evocore_log_set_file(true, evocore_config_get_string(config, "logging",
                                                      "file", "sphere.log"));

    evocore_log_info("Starting sphere function optimization");
    evocore_log_info("Population size: %d", population_size);
    evocore_log_info("Max generations: %d", max_generations);
    evocore_log_info("Mutation rate: %.3f", mutation_rate);

    /* Initialize context */
    sphere_context_t ctx;
    for (int i = 0; i < DIMENSIONS; i++) {
        ctx.mins[i] = evocore_config_get_double(config, "problem",
                                              "min_value", -10.0);
        ctx.maxs[i] = evocore_config_get_double(config, "problem",
                                              "max_value", 10.0);
    }
    ctx.eval_count = 0;

    /* Initialize population */
    evocore_population_t pop;
    err = evocore_population_init(&pop, population_size * 2);
    if (err != EVOCORE_OK) {
        evocore_log_error("Failed to initialize population: %s",
                        evocore_error_string(err));
        evocore_config_free(config);
        return 1;
    }

    unsigned int seed = (unsigned int)time(NULL);
    if (argc > 2) {
        seed = (unsigned int)atoi(argv[2]);
    }

    /* Create initial population */
    evocore_log_info("Creating initial population...");
    for (size_t i = 0; i < population_size; i++) {
        evocore_genome_t genome;
        evocore_genome_init(&genome, GENE_BYTES);
        init_genome(&genome, &ctx, &seed);
        evocore_population_add(&pop, &genome, NAN);
        evocore_genome_cleanup(&genome);
    }

    /* Evaluate initial population */
    evocore_population_evaluate(&pop, sphere_fitness, &ctx);
    evocore_population_sort(&pop);
    evocore_population_update_stats(&pop);

    evocore_log_info("Generation 0: best=%.6f avg=%.6f",
                   pop.best_fitness, pop.avg_fitness);

    /* Evolution loop */
    for (int gen = 1; gen <= max_generations; gen++) {
        /* Clear population for new generation */
        for (size_t i = population_size; i < pop.size; i++) {
            evocore_population_remove(&pop, population_size);
        }
        evocore_population_set_size(&pop, elite_count);

        /* Create offspring */
        while (evocore_population_size(&pop) < population_size) {
            /* Select parents */
            size_t p1_idx = evocore_population_tournament_select(&pop,
                                                                tournament_size,
                                                                &seed);
            size_t p2_idx = evocore_population_tournament_select(&pop,
                                                                tournament_size,
                                                                &seed);
            evocore_individual_t *p1 = evocore_population_get(&pop, p1_idx);
            evocore_individual_t *p2 = evocore_population_get(&pop, p2_idx);

            /* Crossover */
            evocore_genome_t child1, child2;
            err = evocore_genome_crossover(p1->genome, p2->genome,
                                         &child1, &child2, &seed);
            if (err != EVOCORE_OK) continue;

            /* Mutate */
            evocore_genome_mutate(&child1, mutation_rate, &seed);
            evocore_genome_mutate(&child2, mutation_rate, &seed);

            /* Add to population (fitness will be calculated later) */
            evocore_population_add(&pop, &child1, NAN);
            if (evocore_population_size(&pop) < population_size) {
                evocore_population_add(&pop, &child2, NAN);
            }

            evocore_genome_cleanup(&child1);
            evocore_genome_cleanup(&child2);
        }

        /* Evaluate new population */
        evocore_population_evaluate(&pop, sphere_fitness, &ctx);
        evocore_population_sort(&pop);
        evocore_population_update_stats(&pop);

        evocore_log_info("Generation %d: best=%.6f avg=%.6f evals=%zu",
                       gen, pop.best_fitness, pop.avg_fitness, ctx.eval_count);

        /* Check convergence */
        if (-pop.best_fitness < 1e-6) {
            evocore_log_info("Converged at generation %d", gen);
            break;
        }

        evocore_population_increment_generation(&pop);
    }

    /* Print final results */
    evocore_individual_t *best = evocore_population_get_best(&pop);
    if (best) {
        double values[DIMENSIONS];
        evocore_genome_read(best->genome, 0, values, sizeof(values));

        evocore_log_info("=== Final Results ===");
        evocore_log_info("Best fitness: %.10f", -best->fitness);
        evocore_log_info("Best solution:");
        for (int i = 0; i < DIMENSIONS; i++) {
            evocore_log_info("  x[%d] = %.10f", i, values[i]);
        }
        evocore_log_info("Total evaluations: %zu", ctx.eval_count);
    }

    /* Cleanup */
    evocore_population_cleanup(&pop);
    evocore_config_free(config);
    evocore_log_close();

    return 0;
}
