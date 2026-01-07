/**
 * Traveling Salesperson Problem (TSP) Domain Example for evocore
 *
 * This domain demonstrates using evocore to solve the classic TSP:
 * Given N cities, find the shortest route that visits each city
 * exactly once and returns to the origin.
 *
 * The genome is a permutation of city indices [0, 1, 2, ..., N-1].
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*========================================================================
 * TSP Problem Definition
 * ======================================================================*/

#define MAX_CITIES 50

typedef struct {
    int num_cities;
    double x[MAX_CITIES];
    double y[MAX_CITIES];
    /* Precomputed distance matrix for efficiency */
    double distances[MAX_CITIES][MAX_CITIES];
} tsp_problem_t;

typedef struct {
    int permutation[MAX_CITIES];
} tsp_genome_t;

/*========================================================================
 * Utility Functions
 * ======================================================================*/

static double distance(double x1, double y1, double x2, double y2) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    return sqrt(dx * dx + dy * dy);
}

static void precompute_distances(tsp_problem_t *problem) {
    for (int i = 0; i < problem->num_cities; i++) {
        for (int j = 0; j < problem->num_cities; j++) {
            problem->distances[i][j] = distance(
                problem->x[i], problem->y[i],
                problem->x[j], problem->y[j]
            );
        }
    }
}

/* Calculate tour length for a permutation */
static double tour_length(const tsp_genome_t *genome,
                          const tsp_problem_t *problem) {
    double total = 0.0;
    int n = problem->num_cities;

    for (int i = 0; i < n - 1; i++) {
        int from = genome->permutation[i];
        int to = genome->permutation[i + 1];
        total += problem->distances[from][to];
    }

    /* Return to start */
    total += problem->distances[genome->permutation[n - 1]]
                               [genome->permutation[0]];

    return total;
}

/* Fisher-Yates shuffle */
static void shuffle(int *array, int n, unsigned int *seed) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand_r(seed) % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

/*========================================================================
 * Domain Callbacks
 * ======================================================================*/

static void tsp_random_init(evocore_genome_t *genome, void *context) {
    tsp_problem_t *problem = (tsp_problem_t*)context;
    tsp_genome_t tsp_genome;

    /* Initialize with identity permutation */
    for (int i = 0; i < problem->num_cities; i++) {
        tsp_genome.permutation[i] = i;
    }

    /* Shuffle */
    unsigned int seed = (unsigned int)rand();
    shuffle(tsp_genome.permutation, problem->num_cities, &seed);

    evocore_genome_write(genome, 0, &tsp_genome,
                       sizeof(int) * problem->num_cities);
    evocore_genome_set_size(genome, sizeof(int) * problem->num_cities);
}

static void tsp_mutate(evocore_genome_t *genome, double rate, void *context) {
    tsp_problem_t *problem = (tsp_problem_t*)context;
    tsp_genome_t tsp_genome;
    evocore_genome_read(genome, 0, &tsp_genome,
                      sizeof(int) * problem->num_cities);

    int n = problem->num_cities;
    int num_mutations = (int)(n * rate);

    if (num_mutations < 1) num_mutations = 1;

    for (int m = 0; m < num_mutations; m++) {
        /* 2-opt mutation: swap two random cities */
        int i = rand() % n;
        int j = rand() % n;

        int temp = tsp_genome.permutation[i];
        tsp_genome.permutation[i] = tsp_genome.permutation[j];
        tsp_genome.permutation[j] = temp;
    }

    evocore_genome_write(genome, 0, &tsp_genome,
                       sizeof(int) * problem->num_cities);
}

static void tsp_crossover(const evocore_genome_t *parent1,
                          const evocore_genome_t *parent2,
                          evocore_genome_t *child1,
                          evocore_genome_t *child2,
                          void *context) {
    tsp_problem_t *problem = (tsp_problem_t*)context;
    int n = problem->num_cities;

    tsp_genome_t p1, p2;
    evocore_genome_read(parent1, 0, &p1, sizeof(int) * n);
    evocore_genome_read(parent2, 0, &p2, sizeof(int) * n);

    tsp_genome_t c1, c2;

    /* Order crossover (OX) */
    int start = rand() % n;
    int end = start + rand() % (n - start);
    if (end == start) end = start + 1;

    /* Child 1: get segment from p1, rest from p2 */
    int used[MAX_CITIES] = {0};
    for (int i = start; i <= end; i++) {
        c1.permutation[i] = p1.permutation[i];
        used[c1.permutation[i]] = 1;
    }

    int idx = (end + 1) % n;
    for (int i = 0; i < n; i++) {
        int city = p2.permutation[(end + 1 + i) % n];
        if (!used[city]) {
            c1.permutation[idx] = city;
            idx = (idx + 1) % n;
        }
    }

    /* Child 2: get segment from p2, rest from p1 */
    memset(used, 0, sizeof(used));
    for (int i = start; i <= end; i++) {
        c2.permutation[i] = p2.permutation[i];
        used[c2.permutation[i]] = 1;
    }

    idx = (end + 1) % n;
    for (int i = 0; i < n; i++) {
        int city = p1.permutation[(end + 1 + i) % n];
        if (!used[city]) {
            c2.permutation[idx] = city;
            idx = (idx + 1) % n;
        }
    }

    evocore_genome_write(child1, 0, &c1, sizeof(int) * n);
    evocore_genome_set_size(child1, sizeof(int) * n);
    evocore_genome_write(child2, 0, &c2, sizeof(int) * n);
    evocore_genome_set_size(child2, sizeof(int) * n);
}

static double tsp_diversity(const evocore_genome_t *a,
                            const evocore_genome_t *b,
                            void *context) {
    tsp_problem_t *problem = (tsp_problem_t*)context;
    int n = problem->num_cities;

    tsp_genome_t ga, gb;
    evocore_genome_read(a, 0, &ga, sizeof(int) * n);
    evocore_genome_read(b, 0, &gb, sizeof(int) * n);

    /* Count positions that differ */
    int diff = 0;
    for (int i = 0; i < n; i++) {
        if (ga.permutation[i] != gb.permutation[i]) {
            diff++;
        }
    }

    return (double)diff / (double)n;
}

static double tsp_fitness(const evocore_genome_t *genome, void *context) {
    tsp_problem_t *problem = (tsp_problem_t*)context;
    tsp_genome_t tsp_genome;
    evocore_genome_read(genome, 0, &tsp_genome,
                      sizeof(int) * problem->num_cities);

    /* Fitness is inverse of tour length (shorter is better) */
    double length = tour_length(&tsp_genome, problem);
    return 10000.0 / length;  /* Scale for readability */
}

static int tsp_serialize_genome(const evocore_genome_t *genome,
                                char *buffer,
                                size_t size,
                                void *context) {
    tsp_problem_t *problem = (tsp_problem_t*)context;
    tsp_genome_t tsp_genome;
    evocore_genome_read(genome, 0, &tsp_genome,
                      sizeof(int) * problem->num_cities);

    int offset = 0;
    offset += snprintf(buffer + offset, size - offset, "[");
    for (int i = 0; i < problem->num_cities && offset < (int)size - 2; i++) {
        offset += snprintf(buffer + offset, size - offset,
                          "%s%d", i > 0 ? " " : "", tsp_genome.permutation[i]);
    }
    offset += snprintf(buffer + offset, size - offset, "]");

    return offset;
}

/*========================================================================
 * Main Example
 * ======================================================================*/

int main(void) {
    printf("=== TSP Domain Example ===\n\n");

    /* Initialize systems */
    evocore_domain_registry_init();
    evocore_log_set_level(EVOCORE_LOG_INFO);

    /* Create a random TSP problem (cities in a unit square) */
    tsp_problem_t problem = {0};
    problem.num_cities = 15;  /* Manageable size for demo */

    printf("Generating %d cities in unit square...\n", problem.num_cities);
    srand(42);

    for (int i = 0; i < problem.num_cities; i++) {
        problem.x[i] = (double)rand() / RAND_MAX;
        problem.y[i] = (double)rand() / RAND_MAX;
    }

    precompute_distances(&problem);

    printf("City coordinates:\n");
    for (int i = 0; i < problem.num_cities; i++) {
        printf("  City %2d: (%.3f, %.3f)\n", i, problem.x[i], problem.y[i]);
    }
    printf("\n");

    /* Register the TSP domain */
    evocore_domain_t tsp_domain = {
        .name = "tsp",
        .version = "1.0.0",
        .genome_size = sizeof(int) * problem.num_cities,
        .genome_ops = {
            .random_init = tsp_random_init,
            .mutate = tsp_mutate,
            .crossover = tsp_crossover,
            .diversity = tsp_diversity,
        },
        .fitness = tsp_fitness,
        .user_context = &problem,
        .serialize_genome = tsp_serialize_genome,
    };

    evocore_error_t err = evocore_register_domain(&tsp_domain);
    if (err != EVOCORE_OK) {
        printf("Failed to register TSP domain: %d\n", err);
        return 1;
    }

    /* Create and evaluate initial population */
    printf("Creating initial population of 10 random tours:\n\n");

    evocore_genome_t genomes[10];
    char buffer[512];
    double best_fitness = 0.0;
    int best_idx = -1;

    for (int i = 0; i < 10; i++) {
        evocore_domain_create_genome("tsp", &genomes[i]);

        tsp_serialize_genome(&genomes[i], buffer, sizeof(buffer), &problem);
        double fitness = evocore_domain_evaluate_fitness(&genomes[i], &tsp_domain);

        tsp_genome_t g;
        evocore_genome_read(&genomes[i], 0, &g, sizeof(int) * problem.num_cities);
        double length = tour_length(&g, &problem);

        printf("  [%2d] Fitness: %.2f  Length: %.3f  %s\n",
               i + 1, fitness, length, buffer);

        if (fitness > best_fitness) {
            best_fitness = fitness;
            best_idx = i;
        }
    }

    printf("\nBest initial tour: #%d (fitness %.2f)\n\n",
           best_idx + 1, best_fitness);

    /* Run a simple evolution */
    printf("Running 50 generations of simple evolution...\n");

    for (int gen = 0; gen < 50; gen++) {
        /* Tournament selection and replacement */
        for (int i = 0; i < 5; i++) {
            /* Select worst 5 to replace */
            int worst_idx = i;

            /* Find a better parent */
            int parent1 = rand() % 10;
            int parent2 = rand() % 10;

            double f1 = evocore_domain_evaluate_fitness(&genomes[parent1], &tsp_domain);
            double f2 = evocore_domain_evaluate_fitness(&genomes[parent2], &tsp_domain);

            /* Clone better parent to worst position */
            int better = (f1 > f2) ? parent1 : parent2;
            evocore_genome_clone(&genomes[better], &genomes[worst_idx]);

            /* Mutate */
            evocore_domain_mutate_genome(&genomes[worst_idx], &tsp_domain, 0.1);
        }

        /* Track best */
        best_fitness = 0.0;
        for (int i = 0; i < 10; i++) {
            double f = evocore_domain_evaluate_fitness(&genomes[i], &tsp_domain);
            if (f > best_fitness) {
                best_fitness = f;
                best_idx = i;
            }
        }

        if (gen % 10 == 9) {
            printf("  Generation %d: Best fitness = %.2f\n", gen + 1, best_fitness);
        }
    }

    printf("\nFinal best tour:\n");

    tsp_serialize_genome(&genomes[best_idx], buffer, sizeof(buffer), &problem);
    tsp_genome_t best;
    evocore_genome_read(&genomes[best_idx], 0, &best, sizeof(int) * problem.num_cities);
    double best_length = tour_length(&best, &problem);

    printf("  Tour: %s\n", buffer);
    printf("  Length: %.3f\n", best_length);
    printf("  Fitness: %.2f\n", best_fitness);

    printf("\nTour path:\n");
    for (int i = 0; i < problem.num_cities; i++) {
        int city = best.permutation[i];
        printf("  %2d. City %2d: (%.3f, %.3f)\n",
               i + 1, city, problem.x[city], problem.y[city]);
    }
    printf("  Return to City %d: (%.3f, %.3f)\n",
           best.permutation[0],
           problem.x[best.permutation[0]],
           problem.y[best.permutation[0]]);

    /* Cleanup */
    for (int i = 0; i < 10; i++) {
        evocore_genome_cleanup(&genomes[i]);
    }
    evocore_domain_registry_shutdown();

    printf("\n=== Example Complete ===\n");
    return 0;
}
