/**
 * GPU Benchmark Example for evocore
 *
 * Demonstrates batch fitness evaluation performance,
 * comparing GPU vs CPU serial vs CPU parallel (pthread).
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

/*========================================================================
 * Benchmark Fitness Function
 *======================================================================== */

/**
 * Sphere function: f(x) = sum(x_i^2)
 * Genome is interpreted as array of doubles
 */
static double sphere_fitness(const evocore_genome_t *genome,
                             void *context) {
    (void)context;

    if (genome == NULL || genome->data == NULL) {
        return 0.0;
    }

    const double *values = (const double*)genome->data;
    size_t num_values = genome->size / sizeof(double);

    double sum = 0.0;
    for (size_t i = 0; i < num_values; i++) {
        double v = values[i];
        sum += v * v;
    }

    return -sum;  /* Negative for maximization */
}

/**
 * Rastrigin function: more expensive computation
 * f(x) = 10*n + sum(x_i^2 - 10*cos(2*pi*x_i))
 */
static double rastrigin_fitness(const evocore_genome_t *genome,
                               void *context) {
    (void)context;

    if (genome == NULL || genome->data == NULL) {
        return 0.0;
    }

    const double *values = (const double*)genome->data;
    size_t num_values = genome->size / sizeof(double);

    const double PI = 3.14159265358979323846;
    double sum = 10.0 * (double)num_values;

    for (size_t i = 0; i < num_values; i++) {
        double v = values[i];
        sum += v * v - 10.0 * cos(2.0 * PI * v);
    }

    return -sum;
}

/*========================================================================
 * Benchmark Runner
 *======================================================================== */

static void run_benchmark(const char *name, int genome_count,
                           int genome_size, int iterations,
                           evocore_fitness_func_t fitness_func) {
    printf("\n=== %s ===\n", name);
    printf("Genomes: %d, Size: %d bytes, Iterations: %d\n",
           genome_count, genome_size, iterations);

    /* Create genomes */
    evocore_genome_t *genomes = calloc(genome_count, sizeof(evocore_genome_t));
    double *fitnesses = calloc(genome_count, sizeof(double));

    for (int i = 0; i < genome_count; i++) {
        evocore_genome_init(&genomes[i], genome_size);
        evocore_genome_randomize(&genomes[i]);
    }

    /* Build genome pointer array for batch */
    const evocore_genome_t **genome_ptrs = calloc(genome_count,
                                                  sizeof(evocore_genome_t*));
    for (int i = 0; i < genome_count; i++) {
        genome_ptrs[i] = &genomes[i];
    }

    evocore_eval_batch_t batch = {
        .genomes = genome_ptrs,
        .fitnesses = fitnesses,
        .count = genome_count,
        .genome_size = genome_size,
    };

    /* Warmup */
    evocore_eval_result_t result;
    evocore_cpu_evaluate_batch(&batch, fitness_func, NULL, 1, &result);

    /* Benchmark 1: Serial CPU evaluation */
    double serial_time = 0.0;
    for (int iter = 0; iter < iterations; iter++) {
        evocore_eval_result_t res;
        evocore_cpu_evaluate_batch(&batch, fitness_func, NULL, 1, &res);
        serial_time += res.cpu_time_ms;
    }

    /* Benchmark 2: Parallel CPU evaluation (pthread) */
    double parallel_time = 0.0;
    for (int iter = 0; iter < iterations; iter++) {
        evocore_eval_result_t res;
        evocore_cpu_evaluate_batch(&batch, fitness_func, NULL, 0, &res);
        parallel_time += res.cpu_time_ms;
    }

    /* Benchmark 3: GPU path (may fall back to CPU) */
    double gpu_path_time = 0.0;
    int used_gpu_count = 0;

    evocore_gpu_context_t *gpu_ctx = evocore_gpu_init();
    if (gpu_ctx != NULL) {
        for (int iter = 0; iter < iterations; iter++) {
            evocore_eval_result_t res;
            evocore_gpu_evaluate_batch(gpu_ctx, &batch, fitness_func,
                                     NULL, &res);
            gpu_path_time += res.used_gpu ? res.gpu_time_ms : res.cpu_time_ms;
            if (res.used_gpu) used_gpu_count++;
        }
        evocore_gpu_shutdown(gpu_ctx);
    }

    /* Calculate results */
    double avg_serial = serial_time / iterations;
    double avg_parallel = parallel_time / iterations;
    double avg_gpu = gpu_path_time / iterations;

    printf("\nResults:\n");
    printf("  Serial CPU:     %.3f ms/batch (%.0f evals/sec)\n",
           avg_serial, 1000.0 * genome_count / avg_serial);
    printf("  Parallel CPU:   %.3f ms/batch (%.0f evals/sec) [%.1fx speedup]\n",
           avg_parallel, 1000.0 * genome_count / avg_parallel,
           avg_serial / avg_parallel);
    printf("  GPU Path:       %.3f ms/batch (%.0f evals/sec) %s\n",
           avg_gpu, 1000.0 * genome_count / avg_gpu,
           used_gpu_count > 0 ? "[GPU used]" : "[CPU fallback]");

    if (used_gpu_count > 0) {
        printf("  GPU Speedup:    %.1fx vs Serial, %.1fx vs Parallel\n",
               avg_serial / avg_gpu, avg_parallel / avg_gpu);
    }

    /* Print CPU info */
    int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    printf("\nSystem:\n");
    printf("  CPU Cores: %d (parallel evaluation uses up to %d threads)\n",
           cpu_count, cpu_count > 16 ? 16 : cpu_count);

    /* Cleanup */
    for (int i = 0; i < genome_count; i++) {
        evocore_genome_cleanup(&genomes[i]);
    }
    free(genomes);
    free(fitnesses);
    free(genome_ptrs);
}

/*========================================================================
 * Main
 *======================================================================== */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("=== evocore GPU & Parallel CPU Benchmark ===\n");
    printf("Tests batch evaluation performance\n\n");

    srand((unsigned int)time(NULL));
    evocore_log_set_level(EVOCORE_LOG_ERROR);

    /* Detect build options */
#ifdef EVOCORE_HAVE_PTHREADS
    printf("Build: pthread support enabled\n");
#endif
#ifdef EVOCORE_HAVE_CUDA
    printf("Build: CUDA support enabled\n");
#endif
    printf("\n");

    /* Small batch benchmark - Sphere function */
    run_benchmark("Small Batch (Sphere)", 10, 64, 100, sphere_fitness);

    /* Medium batch benchmark - Sphere function */
    run_benchmark("Medium Batch (Sphere)", 100, 64, 50, sphere_fitness);

    /* Large batch benchmark - Sphere function */
    run_benchmark("Large Batch (Sphere)", 1000, 64, 10, sphere_fitness);

    /* Large genomes - Rastrigin (more expensive) */
    run_benchmark("Large Genomes (Rastrigin)", 100, 1024, 20, rastrigin_fitness);

    /* Very large batch - stress test */
    run_benchmark("Very Large Batch", 5000, 64, 5, sphere_fitness);

    printf("\n=== Benchmark Complete ===\n");
    printf("\nBuild options:\n");
    printf("  make                  # Build without CUDA\n");
    printf("  make CUDA=yes         # Build with CUDA support\n");
    printf("  make OMP=yes          # Build with OpenMP (future)\n");
    printf("  make CUDA=yes OMP=yes # Build with both\n");

    return 0;
}
