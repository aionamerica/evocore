/**
 * GPU Benchmark Example for evocore
 *
 * Demonstrates batch fitness evaluation performance,
 * comparing GPU vs CPU (when CUDA is available).
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*========================================================================
 * Benchmark Fitness Function
 *========================================================================*/

/**
 * Simulated expensive fitness calculation
 * Computes sum of bytes with some extra work
 */
static double benchmark_fitness(const evocore_genome_t *genome,
                                 void *context) {
    (void)context;

    if (genome == NULL || genome->data == NULL) {
        return 0.0;
    }

    unsigned char *data = (unsigned char*)genome->data;
    double sum = 0.0;

    /* Add some computational work */
    for (size_t i = 0; i < genome->size; i++) {
        sum += data[i];
        /* Extra work to simulate expensive computation */
        for (int j = 0; j < 10; j++) {
            sum += (double)data[i] * 0.001;
        }
    }

    return sum;
}

/*========================================================================
 * Benchmark Runner
 *========================================================================*/

static void run_benchmark(const char *name, int genome_count,
                           int genome_size, int iterations) {
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

    /* Initialize GPU context */
    evocore_gpu_context_t *gpu_ctx = evocore_gpu_init();
    if (gpu_ctx == NULL) {
        printf("Failed to initialize GPU context\n");
        return;
    }

    evocore_eval_batch_t batch = {
        .genomes = genome_ptrs,
        .fitnesses = fitnesses,
        .count = genome_count,
        .genome_size = genome_size,
    };

    /* Warmup */
    evocore_eval_result_t result;
    evocore_gpu_evaluate_batch(gpu_ctx, &batch, benchmark_fitness, NULL, &result);

    /* Benchmark */
    double total_gpu_time = 0.0;
    double total_cpu_time = 0.0;

    for (int iter = 0; iter < iterations; iter++) {
        evocore_eval_result_t gpu_result, cpu_result;

        /* GPU evaluation */
        evocore_gpu_evaluate_batch(gpu_ctx, &batch, benchmark_fitness,
                                 NULL, &gpu_result);
        total_gpu_time += gpu_result.cpu_time_ms;  /* Will be CPU time if no GPU */

        /* CPU evaluation for comparison */
        evocore_cpu_evaluate_batch(&batch, benchmark_fitness, NULL,
                                 0, &cpu_result);
        total_cpu_time += cpu_result.cpu_time_ms;
    }

    printf("\nResults:\n");
    printf("  Avg time per batch (GPU path):  %.3f ms\n", total_gpu_time / iterations);
    printf("  Avg time per batch (CPU only):  %.3f ms\n", total_cpu_time / iterations);
    printf("  Evaluations per second (GPU):  %.0f\n",
           1000.0 * genome_count / (total_gpu_time / iterations));
    printf("  Evaluations per second (CPU):  %.0f\n",
           1000.0 * genome_count / (total_cpu_time / iterations));

    if (total_gpu_time < total_cpu_time) {
        printf("  Speedup: %.2fx faster\n", total_cpu_time / total_gpu_time);
    } else if (total_gpu_time > total_cpu_time) {
        printf("  Overhead: %.2fx slower (CPU path)\n",
               total_gpu_time / total_cpu_time);
    }

    /* Print GPU info */
    evocore_gpu_print_info(gpu_ctx);

    /* Cleanup */
    evocore_gpu_shutdown(gpu_ctx);

    for (int i = 0; i < genome_count; i++) {
        evocore_genome_cleanup(&genomes[i]);
    }
    free(genomes);
    free(fitnesses);
    free(genome_ptrs);
}

/*========================================================================
 * Main
 *========================================================================*/

int main(void) {
    printf("=== evocore GPU Benchmark ===\n");
    printf("Tests batch evaluation performance\n");
    printf("Note: GPU path uses CPU fallback if CUDA is not available\n");

    srand((unsigned int)time(NULL));
    evocore_log_set_level(EVOCORE_LOG_WARN);

    /* Small batch benchmark */
    run_benchmark("Small Batch", 10, 64, 100);

    /* Medium batch benchmark */
    run_benchmark("Medium Batch", 100, 64, 50);

    /* Large batch benchmark */
    run_benchmark("Large Batch", 1000, 64, 10);

    /* Large genomes */
    run_benchmark("Large Genomes", 100, 1024, 20);

    printf("\n=== Benchmark Complete ===\n");
    printf("\nTo enable GPU acceleration, compile with:\n");
    printf("  make CFLAGS='-DEVOCORE_HAVE_CUDA -I/usr/local/cuda/include' \\\n");
    printf("       LDFLAGS='-L/usr/local/cuda/lib64 -lcudart'\n");

    return 0;
}
