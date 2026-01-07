/**
 * Statistics and Monitoring Module Implementation
 */

#define _GNU_SOURCE
#include "evocore/stats.h"
#include "evocore/evocore.h"
#include "evocore/log.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#ifdef OMP_SUPPORT
#include <omp.h>
#endif

/*========================================================================
 * Internal Helpers
 *======================================================================== */

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

static double calculate_variance(const evocore_population_t *pop, double mean) {
    if (pop->size == 0) return 0.0;

    double sum_sq_diff = 0.0;
    for (size_t i = 0; i < pop->size; i++) {
        double diff = pop->individuals[i].fitness - mean;
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / (double)pop->size;
}

/*========================================================================
 * Statistics API
 *======================================================================== */

evocore_stats_t* evocore_stats_create(const evocore_stats_config_t *config) {
    evocore_stats_t *stats = (evocore_stats_t*)evocore_calloc(1, sizeof(evocore_stats_t));
    if (!stats) {
        return NULL;
    }

    /* Apply config or defaults */
    if (config) {
        stats->convergence_streak = (int)config->improvement_threshold;
    }

    stats->current_generation = 0;
    stats->best_fitness_ever = -INFINITY;
    stats->worst_fitness_ever = INFINITY;

    return stats;
}

void evocore_stats_free(evocore_stats_t *stats) {
    if (stats) {
        evocore_free(stats);
    }
}

evocore_error_t evocore_stats_update(evocore_stats_t *stats,
                                const evocore_population_t *pop) {
    if (!stats || !pop) {
        return EVOCORE_ERR_NULL_PTR;
    }

    stats->current_generation = pop->generation;

    /* Update fitness tracking */
    stats->best_fitness_current = pop->best_fitness;
    stats->avg_fitness_current = pop->avg_fitness;
    stats->worst_fitness_current = pop->worst_fitness;

    if (pop->best_fitness > stats->best_fitness_ever) {
        double improvement = pop->best_fitness - stats->best_fitness_ever;
        stats->best_fitness_ever = pop->best_fitness;
        stats->convergence_streak = 0;  /* Reset on improvement */

        /* Track improvement rate */
        if (stats->current_generation > 0) {
            stats->fitness_improvement_rate =
                improvement / (double)stats->current_generation;
        }
    } else {
        stats->convergence_streak++;
    }

    if (pop->worst_fitness < stats->worst_fitness_ever) {
        stats->worst_fitness_ever = pop->worst_fitness;
    }

    /* Calculate fitness variance */
    stats->fitness_variance = calculate_variance(pop, pop->avg_fitness);

    /* Update diversity flag */
    stats->diverse = (stats->fitness_variance > 1.0);

    /* Track memory usage if enabled */
    if (stats->track_memory) {
        evocore_memory_stats_t mem_stats;
        evocore_memory_get_stats(&mem_stats);
        stats->memory_usage_bytes = mem_stats.current_usage;
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_stats_record_operations(evocore_stats_t *stats,
                                          long long eval_count,
                                          long long mutations,
                                          long long crossovers) {
    if (!stats) {
        return EVOCORE_ERR_NULL_PTR;
    }

    stats->total_evaluations += eval_count;
    stats->mutations_performed += mutations;
    stats->crossovers_performed += crossovers;

    return EVOCORE_OK;
}

bool evocore_stats_is_converged(const evocore_stats_t *stats) {
    if (!stats) return false;

    /* Converged if: low variance AND no recent improvement */
    bool low_variance = (stats->fitness_variance < 0.01);
    bool no_improvement = (stats->convergence_streak > 50);

    return low_variance && no_improvement;
}

bool evocore_stats_is_stagnant(const evocore_stats_t *stats) {
    if (!stats) return false;
    return stats->convergence_streak > 20;
}

double evocore_stats_diversity(const evocore_population_t *pop) {
    if (!pop || pop->size == 0) {
        return 0.0;
    }

    /* Calculate normalized diversity score */
    /* Based on average pairwise Hamming distance */

    /* For efficiency, sample pairs instead of checking all */
    const int samples = 100;
    double total_distance = 0.0;
    int sample_count = 0;

    unsigned int seed = (unsigned int)time(NULL);

    for (int i = 0; i < samples && (size_t)i < pop->size; i++) {
        int j = (int)((size_t)i + rand_r(&seed)) % pop->size;
        if (i != j) {
            const evocore_genome_t *g1 = pop->individuals[i].genome;
            const evocore_genome_t *g2 = pop->individuals[j].genome;

            if (g1 && g1->data && g2 && g2->data) {
                size_t min_size = g1->size < g2->size ? g1->size : g2->size;
                size_t distance = 0;
                for (size_t k = 0; k < min_size; k++) {
                    unsigned char b1 = ((unsigned char*)g1->data)[k];
                    unsigned char b2 = ((unsigned char*)g2->data)[k];
                    if (b1 != b2) distance++;
                }

                /* Normalize by genome size */
                total_distance += (double)distance / (double)g1->capacity;
                sample_count++;
            }
        }
    }

    if (sample_count == 0) return 0.0;
    return total_distance / (double)sample_count;
}

evocore_error_t evocore_stats_fitness_distribution(const evocore_population_t *pop,
                                             double *out_min,
                                             double *out_max,
                                             double *out_mean,
                                             double *out_stddev) {
    if (!pop || pop->size == 0) {
        return EVOCORE_ERR_POP_EMPTY;
    }

    double min = INFINITY;
    double max = -INFINITY;
    double sum = 0.0;
    size_t valid_count = 0;

    for (size_t i = 0; i < pop->size; i++) {
        double fitness = pop->individuals[i].fitness;
        if (!isnan(fitness)) {
            if (fitness < min) min = fitness;
            if (fitness > max) max = fitness;
            sum += fitness;
            valid_count++;
        }
    }

    if (valid_count == 0) {
        return EVOCORE_ERR_POP_EMPTY;
    }

    double mean = sum / (double)valid_count;

    /* Calculate standard deviation */
    double sum_sq_diff = 0.0;
    for (size_t i = 0; i < pop->size; i++) {
        double fitness = pop->individuals[i].fitness;
        if (!isnan(fitness)) {
            double diff = fitness - mean;
            sum_sq_diff += diff * diff;
        }
    }
    double stddev = sqrt(sum_sq_diff / (double)valid_count);

    if (out_min) *out_min = min;
    if (out_max) *out_max = max;
    if (out_mean) *out_mean = mean;
    if (out_stddev) *out_stddev = stddev;

    return EVOCORE_OK;
}

/*========================================================================
 * Progress Reporting
 *======================================================================== */

evocore_error_t evocore_progress_reporter_init(evocore_progress_reporter_t *reporter,
                                         evocore_progress_callback_t callback,
                                         void *user_data) {
    if (!reporter || !callback) {
        return EVOCORE_ERR_NULL_PTR;
    }

    reporter->callback = callback;
    reporter->user_data = user_data;
    reporter->report_every_n_generations = 10;
    reporter->verbose = false;

    return EVOCORE_OK;
}

evocore_error_t evocore_progress_report(const evocore_progress_reporter_t *reporter,
                                  const evocore_stats_t *stats) {
    if (!reporter || !stats) {
        return EVOCORE_ERR_NULL_PTR;
    }

    /* Report if at start, at interval, or at end */
    bool should_report = (stats->current_generation == 0) ||
                         (stats->current_generation % reporter->report_every_n_generations == 0) ||
                         evocore_stats_is_converged(stats);

    if (should_report && reporter->callback) {
        reporter->callback(stats, reporter->user_data);
    }

    return EVOCORE_OK;
}

void evocore_progress_print_console(const evocore_stats_t *stats, void *user_data) {
    (void)user_data;

    printf("\n=== Generation %zu ===\n", stats->current_generation);
    printf("Fitness:     best=%.6f  avg=%.6f  worst=%.6f\n",
           stats->best_fitness_current,
           stats->avg_fitness_current,
           stats->worst_fitness_current);
    printf("All-time:     best_ever=%.6f  improvement_rate=%.8f\n",
           stats->best_fitness_ever,
           stats->fitness_improvement_rate);
    printf("Diversity:    variance=%.6f  diverse=%s\n",
           stats->fitness_variance,
           stats->diverse ? "yes" : "no");

    if (stats->convergence_streak > 0) {
        printf("Stagnation:   %d generations without improvement\n",
               stats->convergence_streak);
    }

    if (stats->total_evaluations > 0) {
        printf("Operations:   %lld evaluations\n", stats->total_evaluations);
    }

    printf("Status:       %s %s %s\n",
           evocore_stats_is_converged(stats) ? "[CONVERGED]" : "",
           evocore_stats_is_stagnant(stats) ? "[STAGNANT]" : "",
           stats->diverse ? "[DIVERSE]" : "");
}

/*========================================================================
 * Diagnostic Info
 *======================================================================== */

evocore_error_t evocore_diagnostic_generate(const evocore_population_t *pop,
                                       evocore_diagnostic_report_t *report) {
    if (!report) {
        return EVOCORE_ERR_NULL_PTR;
    }

    memset(report, 0, sizeof(*report));

    /* Version info */
    snprintf(report->version, sizeof(report->version), "%s", EVOCORE_VERSION_STRING);
    snprintf(report->build_timestamp, sizeof(report->build_timestamp),
             "%s %s", __DATE__, __TIME__);

    /* System info */
    report->num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    report->simd_available = evocore_simd_available();

#ifdef OMP_SUPPORT
    report->openmp_available = true;
#else
    report->openmp_available = false;
#endif

    /* Runtime stats */
    evocore_memory_get_stats(&report->memory);
    evocore_perf_monitor_t *perf = evocore_perf_monitor_get();
    if (perf) {
        report->perf = *perf;
    }

    /* Population state */
    if (pop) {
        report->population_size = pop->size;
        report->population_capacity = pop->capacity;
        report->generation = pop->generation;
        report->best_fitness = pop->best_fitness;
        report->population_healthy = (pop->size > 0);
    } else {
        report->population_size = 0;
        report->population_capacity = 0;
        report->generation = 0;
        report->best_fitness = 0.0;
        report->population_healthy = true;  /* No population is considered healthy */
    }

    /* Health indicators */
    report->memory_healthy = (report->memory.current_usage <
                              (report->memory.peak_usage * 1.5));
    report->performance_healthy = true;

    return EVOCORE_OK;
}

void evocore_diagnostic_print(const evocore_diagnostic_report_t *report) {
    if (!report) return;

    printf("\n=== Evocore Diagnostic Report ===\n");
    printf("Version: %s\n", report->version);
    printf("Build: %s\n\n", report->build_timestamp);

    printf("System:\n");
    printf("  CPU cores: %d\n", report->num_cores);
    printf("  SIMD: %s\n", report->simd_available ? "available" : "not available");
    printf("  OpenMP: %s\n\n", report->openmp_available ? "available" : "not available");

    printf("Memory:\n");
    printf("  Current: %zu bytes\n", report->memory.current_usage);
    printf("  Peak: %zu bytes\n", report->memory.peak_usage);
    printf("  Allocations: %zu\n", report->memory.allocation_count);
    printf("  Status: %s\n\n", report->memory_healthy ? "OK" : "WARNING");

    if (report->population_size > 0) {
        printf("Population:\n");
        printf("  Size: %zu / %zu\n", report->population_size, report->population_capacity);
        printf("  Generation: %zu\n", report->generation);
        printf("  Best fitness: %.6f\n", report->best_fitness);
        printf("  Status: %s\n\n", report->population_healthy ? "OK" : "WARNING");
    }

    printf("Performance Counters:\n");
    for (int i = 0; i < report->perf.count; i++) {
        const evocore_perf_counter_t *c = &report->perf.counters[i];
        printf("  %s: %lld calls, %.2f ms\n", c->name, c->count, c->total_time_ms);
    }

    printf("Health: %s\n",
           (report->memory_healthy && report->performance_healthy &&
            (report->population_size == 0 || report->population_healthy)) ?
           "OK" : "CHECK RECOMMENDED");
    printf("==================================\n\n");
}

void evocore_diagnostic_log(const evocore_diagnostic_report_t *report) {
    if (!report) return;

    evocore_log_info("=== Diagnostic Report ===");
    evocore_log_info("Version: %s", report->version);
    evocore_log_info("CPU cores: %d, SIMD: %s, OpenMP: %s",
                    report->num_cores,
                    report->simd_available ? "yes" : "no",
                    report->openmp_available ? "yes" : "no");
    evocore_log_info("Memory: %zu / %zu bytes, %zu allocations",
                    report->memory.current_usage,
                    report->memory.peak_usage,
                    report->memory.allocation_count);

    if (report->population_size > 0) {
        evocore_log_info("Population: %zu/%zu, gen %zu, best %.6f",
                        report->population_size,
                        report->population_capacity,
                        report->generation,
                        report->best_fitness);
    }

    evocore_log_info("Health: %s",
                    (report->memory_healthy && report->performance_healthy &&
                     (report->population_size == 0 || report->population_healthy)) ?
                     "OK" : "WARNING");
}
