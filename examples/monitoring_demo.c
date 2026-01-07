/**
 * Monitoring Demo - Example of using evocore statistics and monitoring
 *
 * This example demonstrates:
 * 1. Tracking evolutionary statistics across generations
 * 2. Using progress callbacks for monitoring
 * 3. Convergence and stagnation detection
 * 4. Generating diagnostic reports
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

/*========================================================================
 * Test Optimization Problem - Rastrigin Function
 *======================================================================== */

typedef struct {
    int dimensions;
    double A;  /* Rastrigin constant (usually 10) */
} rastrigin_context_t;

/**
 * Rastrigin function - A challenging multimodal optimization problem
 * f(x) = A*n + sum(x[i]^2 - A*cos(2*pi*x[i]))
 * Minimum is 0 at x[i] = 0 for all i
 */
static double rastrigin_fitness(const evocore_genome_t *genome, void *context) {
    rastrigin_context_t *ctx = (rastrigin_context_t*)context;

    if (!genome->data || genome->size < ctx->dimensions * sizeof(double)) {
        return 1000000.0;  /* Penalty for invalid genome */
    }

    const double *values = (const double*)genome->data;

    double sum = 0.0;
    for (int i = 0; i < ctx->dimensions; i++) {
        double x = values[i];
        /* Scale x to [-5.12, 5.12] range (Rastrigin standard domain) */
        /* Assuming genome bytes are interpreted as [-1, 1] doubles */
        x = x * 5.12;

        sum += (x * x - ctx->A * cos(2.0 * M_PI * x));
    }

    double result = ctx->A * ctx->dimensions + sum;
    return -result;  /* Negative for maximization */
}

/*========================================================================
 * Custom Progress Callback
 *======================================================================== */

static int g_progress_count = 0;

static void my_progress_callback(const evocore_stats_t *stats, void *user_data) {
    g_progress_count++;
    int *verbose = (int*)user_data;

    if (*verbose) {
        printf("\n--- Progress Update #%d ---\n", g_progress_count);
        printf("Generation: %zu\n", stats->current_generation);
        printf("Fitness:    best=%.6f  avg=%.6f  worst=%.6f\n",
               stats->best_fitness_current,
               stats->avg_fitness_current,
               stats->worst_fitness_current);
        printf("Best Ever:  %.6f (improvement rate: %.8f)\n",
               stats->best_fitness_ever,
               stats->fitness_improvement_rate);
        printf("Diversity:  variance=%.6f  %s\n",
               stats->fitness_variance,
               stats->diverse ? "diverse" : "converged");

        if (stats->convergence_streak > 0) {
            printf("Stagnation: %d generations without improvement\n",
                   stats->convergence_streak);
        }

        printf("Status:     %s %s %s\n",
               evocore_stats_is_converged(stats) ? "[CONVERGED]" : "",
               evocore_stats_is_stagnant(stats) ? "[STAGNANT]" : "",
               stats->diverse ? "[DIVERSE]" : "");
    }
}

/*========================================================================
 * Evolution with Monitoring
 *======================================================================== */

static int run_evolution_with_monitoring(void) {
    printf("=== Rastrigin Function Optimization ===\n");
    printf("A challenging multimodal optimization problem\n\n");

    /* Setup problem */
    rastrigin_context_t ctx = {
        .dimensions = 10,
        .A = 10.0
    };

    evocore_domain_t domain = {
        .name = "rastrigin",
        .genome_size = ctx.dimensions * sizeof(double)
    };

    /* Create statistics tracker */
    evocore_stats_config_t stats_config = EVOCORE_STATS_CONFIG_DEFAULTS;
    stats_config.improvement_threshold = 0.0001;
    stats_config.stagnation_generations = 30;
    stats_config.diversity_threshold = 0.5;

    evocore_stats_t *stats = evocore_stats_create(&stats_config);
    if (!stats) {
        fprintf(stderr, "Failed to create statistics tracker\n");
        return 1;
    }

    /* Setup progress reporter */
    evocore_progress_reporter_t reporter;
    int verbose = 1;  /* Enable detailed output */

    evocore_progress_reporter_init(&reporter,
                                  my_progress_callback,
                                  &verbose);
    reporter.report_every_n_generations = 5;

    /* Initialize population */
    evocore_population_t pop;
    evocore_population_init(&pop, 100);

    printf("Initializing population...\n");

    /* Create initial population with random genomes */
    for (size_t i = 0; i < 100; i++) {
        evocore_genome_t genome;
        evocore_genome_init(&genome, domain.genome_size);

        /* Initialize with random values in [-1, 1] */
        double *values = (double*)genome.data;
        for (int j = 0; j < ctx.dimensions; j++) {
            values[j] = (double)rand() / RAND_MAX * 2.0 - 1.0;
        }
        genome.size = genome.capacity;

        double fitness = rastrigin_fitness(&genome, &ctx);
        evocore_population_add(&pop, &genome, fitness);
        evocore_genome_cleanup(&genome);
    }

    evocore_population_update_stats(&pop);
    evocore_stats_update(stats, &pop);

    printf("Initial best fitness: %.6f\n", pop.best_fitness);
    printf("Target: 0.0 (global optimum)\n\n");

    /* Run evolution */
    printf("Running evolution...\n\n");

    int max_generations = 100;
    int eval_count = 0;
    int mutation_count = 0;

    for (int gen = 0; gen < max_generations; gen++) {
        pop.generation = gen + 1;

        /* Elitism: keep best 10 */
        evocore_population_sort(&pop);

        /* Evolve the remaining 90 */
        for (size_t i = 10; i < pop.size; i++) {
            /* Tournament selection */
            int i1 = rand() % 10;
            int i2 = rand() % 10;
            int winner = (pop.individuals[i1].fitness > pop.individuals[i2].fitness) ? i1 : i2;

            /* Clone winner */
            evocore_genome_t *parent = pop.individuals[winner].genome;
            evocore_genome_t *child = pop.individuals[i].genome;

            /* Copy parent genome to child */
            memcpy(child->data, parent->data,
                   parent->size < child->capacity ? parent->size : child->capacity);
            child->size = parent->size;

            /* Mutate */
            double *values = (double*)child->data;
            for (int j = 0; j < ctx.dimensions; j++) {
                if (rand() < (int)(0.2 * RAND_MAX)) {
                    /* Gaussian mutation */
                    double delta = ((double)rand() / RAND_MAX - 0.5) * 0.2;
                    values[j] += delta;
                    /* Clamp to [-1, 1] */
                    if (values[j] < -1.0) values[j] = -1.0;
                    if (values[j] > 1.0) values[j] = 1.0;
                }
            }
            mutation_count++;

            /* Evaluate */
            double fitness = rastrigin_fitness(child, &ctx);
            pop.individuals[i].fitness = fitness;
            eval_count++;
        }

        evocore_population_update_stats(&pop);
        evocore_stats_update(stats, &pop);
        evocore_stats_record_operations(stats, eval_count, mutation_count, 0);

        /* Check convergence */
        if (evocore_stats_is_converged(stats)) {
            printf("\n*** Converged at generation %d! ***\n", gen + 1);
            break;
        }

        /* Report progress */
        evocore_progress_report(&reporter, stats);

        /* Check stagnation */
        if (evocore_stats_is_stagnant(stats)) {
            printf("\n*** Stagnation detected - applying diversity injection ***\n");
            /* Re-randomize worst 50% */
            for (size_t i = pop.size / 2; i < pop.size; i++) {
                double *values = (double*)pop.individuals[i].genome->data;
                for (int j = 0; j < ctx.dimensions; j++) {
                    values[j] = (double)rand() / RAND_MAX * 2.0 - 1.0;
                }
                pop.individuals[i].fitness =
                    rastrigin_fitness(pop.individuals[i].genome, &ctx);
            }
            evocore_population_update_stats(&pop);
            evocore_stats_update(stats, &pop);
        }
    }

    /* Final report */
    printf("\n=== Final Results ===\n");
    printf("Generations: %zu\n", pop.generation);
    printf("Best Fitness: %.6f\n", pop.best_fitness);
    printf("Average Fitness: %.6f\n", pop.avg_fitness);
    printf("Worst Fitness: %.6f\n", pop.worst_fitness);
    printf("\nTotal Evaluations: %lld\n", stats->total_evaluations);
    printf("Mutations Performed: %lld\n", stats->mutations_performed);
    printf("Fitness Variance: %.6f\n", stats->fitness_variance);

    /* Print diversity metric */
    double diversity = evocore_stats_diversity(&pop);
    printf("Population Diversity: %.4f (0=identical, 1=very diverse)\n", diversity);

    /* Generate diagnostic report */
    printf("\n=== Diagnostic Report ===\n");

    evocore_diagnostic_report_t diag;
    evocore_diagnostic_generate(&pop, &diag);

    printf("Version: %s\n", diag.version);
    printf("CPU Cores: %d\n", diag.num_cores);
    printf("SIMD Available: %s\n", diag.simd_available ? "Yes" : "No");
    printf("OpenMP Available: %s\n", diag.openmp_available ? "Yes" : "No");

    printf("\nMemory:\n");
    printf("  Current: %zu bytes\n", diag.memory.current_usage);
    printf("  Peak: %zu bytes\n", diag.memory.peak_usage);

    printf("\nPerformance Counters:\n");
    for (int i = 0; i < diag.perf.count; i++) {
        const evocore_perf_counter_t *c = &diag.perf.counters[i];
        printf("  %s: %lld calls, %.2f ms\n", c->name, c->count, c->total_time_ms);
    }

    /* Cleanup */
    evocore_stats_free(stats);
    evocore_population_cleanup(&pop);

    return 0;
}

/*========================================================================
 * Main
 *========================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Evocore Monitoring Demo\n");
    printf("=====================\n\n");

    /* Seed random number generator */
    srand((unsigned int)time(NULL));

    /* Run evolution with monitoring */
    int result = run_evolution_with_monitoring();

    printf("\nDemo complete.\n");

    return result;
}
