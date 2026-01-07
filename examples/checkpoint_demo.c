/**
 * Checkpoint Demo - Example of using evocore persistence
 *
 * This example demonstrates:
 * 1. Running an evolutionary optimization
 * 2. Saving checkpoints periodically
 * 3. Loading and resuming from a checkpoint
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <unistd.h>

/* Global flag for graceful shutdown */
static volatile int g_running = 1;

/* Signal handler for Ctrl+C */
static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
    printf("\nReceived shutdown signal, saving checkpoint...\n");
}

/*========================================================================
 * Sphere Function - Simple Optimization Problem
 *========================================================================*/

typedef struct {
    int dimensions;
    double target;
} sphere_context_t;

static double sphere_fitness(const evocore_genome_t *genome, void *context) {
    sphere_context_t *ctx = (sphere_context_t*)context;

    if (!genome->data || genome->size < ctx->dimensions * sizeof(double)) {
        return 1000000.0;  /* Penalty for invalid genome */
    }

    const double *values = (const double*)genome->data;

    /* Sphere function: sum((x - target)^2) */
    double sum = 0.0;
    for (int i = 0; i < ctx->dimensions; i++) {
        double diff = values[i] - ctx->target;
        sum += diff * diff;
    }

    return -sum;  /* Negative for maximization */
}

/*========================================================================
 * Demo Functions
 *========================================================================*/

static int run_optimization(const char *checkpoint_dir, int max_generations) {
    printf("=== Starting Optimization ===\n");

    /* Setup problem */
    sphere_context_t ctx = {
        .dimensions = 10,
        .target = 42.0
    };

    evocore_domain_t domain = {
        .name = "sphere",
        .genome_size = ctx.dimensions * sizeof(double)
    };

    /* Setup auto-checkpoint */
    evocore_auto_checkpoint_config_t checkpoint_config = {
        .enabled = true,
        .every_n_generations = 5,
        .max_checkpoints = 3
    };
    snprintf(checkpoint_config.directory, sizeof(checkpoint_config.directory),
             "%s", checkpoint_dir);

    evocore_checkpoint_manager_t *checkpoint_mgr =
        evocore_checkpoint_manager_create(&checkpoint_config);

    if (!checkpoint_mgr) {
        fprintf(stderr, "Failed to create checkpoint manager\n");
        return 1;
    }

    /* Try to load from existing checkpoint */
    char latest_checkpoint[512];
    latest_checkpoint[0] = '\0';

    int count = 0;
    char **checkpoints = evocore_checkpoint_list(checkpoint_dir, &count);
    if (checkpoints && count > 0) {
        /* Use the most recent checkpoint */
        snprintf(latest_checkpoint, sizeof(latest_checkpoint),
                "%s", checkpoints[count - 1]);
        evocore_checkpoint_list_free(checkpoints, count);
    }

    evocore_population_t pop;
    int start_generation = 0;

    if (latest_checkpoint[0]) {
        printf("Loading checkpoint: %s\n", latest_checkpoint);

        evocore_checkpoint_t checkpoint;
        if (evocore_checkpoint_load(latest_checkpoint, &checkpoint) == EVOCORE_OK) {
            printf("  Generation: %zu\n", checkpoint.generation);
            printf("  Best Fitness: %.6f\n", checkpoint.best_fitness);

            /* Restore population */
            evocore_population_init(&pop, 50);
            evocore_checkpoint_restore(&checkpoint, &pop, &domain, NULL);

            start_generation = (int)checkpoint.generation;
            evocore_checkpoint_free(&checkpoint);
        } else {
            printf("  Failed to load checkpoint, starting fresh\n");
            evocore_population_init(&pop, 50);
        }
    } else {
        printf("No existing checkpoint found, starting fresh\n");
        evocore_population_init(&pop, 50);
    }

    /* Initialize population if needed */
    if (pop.size == 0) {
        printf("Initializing population...\n");
        for (size_t i = 0; i < 50; i++) {
            evocore_genome_t genome;
            evocore_genome_init(&genome, domain.genome_size);
            evocore_genome_randomize(&genome);
            genome.size = genome.capacity;
            double fitness = sphere_fitness(&genome, &ctx);
            evocore_population_add(&pop, &genome, fitness);
            evocore_genome_cleanup(&genome);
        }
        evocore_population_update_stats(&pop);
        printf("  Initial best fitness: %.6f\n", pop.best_fitness);
    }

    /* Run evolution */
    printf("\nRunning evolution (generations %d to %d)...\n",
           start_generation, max_generations);

    for (int gen = start_generation; gen < max_generations && g_running; gen++) {
        pop.generation = gen + 1;

        /* Simple selection and reproduction */
        for (size_t i = 0; i < pop.size / 2; i++) {
            /* Tournament selection */
            int i1 = rand() % (int)pop.size;
            int i2 = rand() % (int)pop.size;

            int winner = (pop.individuals[i1].fitness > pop.individuals[i2].fitness) ? i1 : i2;

            /* Clone winner with mutation */
            evocore_genome_t *parent = pop.individuals[pop.size - 1 - i].genome;

            /* Mutate in-place */
            unsigned char *data = (unsigned char*)parent->data;
            size_t mutations = (size_t)(parent->size * 0.1);
            for (size_t j = 0; j < mutations; j++) {
                size_t pos = rand() % parent->size;
                data[pos] ^= (rand() % 256);
            }

            pop.individuals[pop.size - 1 - i].fitness =
                sphere_fitness(parent, &ctx);
        }

        evocore_population_update_stats(&pop);

        /* Progress every 10 generations */
        if ((gen + 1) % 10 == 0 || gen == start_generation) {
            printf("  Gen %3d: best=%.6f avg=%.6f\n",
                   gen + 1, pop.best_fitness, pop.avg_fitness);
        }

        /* Update checkpoint manager */
        evocore_checkpoint_manager_update(checkpoint_mgr, &pop, &domain, NULL);
    }

    /* Final results */
    printf("\n=== Results ===\n");
    printf("Final generation: %zu\n", pop.generation);
    printf("Best fitness: %.6f\n", pop.best_fitness);

    /* Save final checkpoint */
    char final_checkpoint[512];
    snprintf(final_checkpoint, sizeof(final_checkpoint),
             "%s/checkpoint_final.json", checkpoint_dir);

    evocore_checkpoint_t checkpoint;
    if (evocore_checkpoint_create(&checkpoint, &pop, &domain, NULL) == EVOCORE_OK) {
        evocore_checkpoint_save(&checkpoint, final_checkpoint, NULL);
        evocore_checkpoint_free(&checkpoint);
        printf("Final checkpoint saved: %s\n", final_checkpoint);
    }

    /* Cleanup */
    evocore_checkpoint_manager_destroy(checkpoint_mgr);
    evocore_population_cleanup(&pop);

    return 0;
}

/*========================================================================
 * Main
 *========================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("Evocore Checkpoint Demo\n");
    printf("======================\n\n");

    /* Setup signal handler */
    signal(SIGINT, handle_signal);

    /* Create checkpoint directory */
    const char *checkpoint_dir = "/tmp/evocore_demo_checkpoints";
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", checkpoint_dir);
    system(cmd);

    /* Run optimization with checkpointing */
    int result = run_optimization(checkpoint_dir, 50);

    printf("\nDemo complete.\n");
    printf("Checkpoints saved in: %s\n", checkpoint_dir);

    return result;
}
