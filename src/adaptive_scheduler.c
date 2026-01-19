#define _GNU_SOURCE
#include "evocore/adaptive_scheduler.h"
#include "internal.h"
#include "evocore/log.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <float.h>

/*========================================================================
 * Default Parameters
 *========================================================================*/

#define DEFAULT_HISTORY_WINDOW 50
#define DEFAULT_STAGNATION_THRESHOLD 20
#define DEFAULT_MIN_DIVERSITY 0.1
#define DEFAULT_MIN_MUTATION_RATE 0.001
#define DEFAULT_DECAY_ALPHA 0.01
#define DEFAULT_STAGNATION_BOOST 3.0
#define DEFAULT_DIVERSITY_BOOST 1.5
#define DEFAULT_HIGH_VAR_KILL 0.15
#define DEFAULT_MEDIUM_VAR_KILL 0.25
#define DEFAULT_LOW_VAR_KILL 0.40
#define DEFAULT_HIGH_VAR_THRESHOLD 0.15
#define DEFAULT_LOW_VAR_THRESHOLD 0.05

/*========================================================================
 * Helper Functions
 *========================================================================*/

static double calculate_mean(const double *values, size_t count) {
    if (count == 0) return 0.0;

    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        if (!isnan(values[i]) && !isinf(values[i])) {
            sum += values[i];
        }
    }
    return sum / count;
}

static double calculate_stddev(const double *values, size_t count, double mean) {
    if (count <= 1) return 0.0;

    double sum_sq = 0.0;
    for (size_t i = 0; i < count; i++) {
        if (!isnan(values[i]) && !isinf(values[i])) {
            double diff = values[i] - mean;
            sum_sq += diff * diff;
        }
    }
    return sqrt(sum_sq / count);
}

static double calculate_linear_trend(const double *values, size_t count) {
    if (count < 2) return 0.0;

    /* Simple linear regression slope */
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
    size_t valid_count = 0;

    for (size_t i = 0; i < count; i++) {
        if (!isnan(values[i]) && !isinf(values[i])) {
            double x = (double)i;
            double y = values[i];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
            valid_count++;
        }
    }

    if (valid_count < 2) return 0.0;

    double n = (double)valid_count;
    double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);

    return slope;
}

/*========================================================================
 * Scheduler Lifecycle
 *========================================================================*/

evocore_adaptive_scheduler_t* evocore_adaptive_scheduler_create(
    size_t max_generations,
    const evocore_meta_params_t *initial_params
) {
    if (max_generations == 0) {
        evocore_set_error(EVOCORE_ERR_INVALID_ARG, "max_generations must be > 0");
        return NULL;
    }

    evocore_adaptive_scheduler_t *scheduler = evocore_malloc(sizeof(evocore_adaptive_scheduler_t));
    if (!scheduler) {
        evocore_set_error(EVOCORE_ERR_OUT_OF_MEMORY, "Failed to allocate scheduler");
        return NULL;
    }

    memset(scheduler, 0, sizeof(evocore_adaptive_scheduler_t));

    /* Initialize progress tracking */
    scheduler->current_generation = 0;
    scheduler->max_generations = max_generations;
    scheduler->current_phase = EVOCORE_PHASE_EARLY;

    /* Allocate fitness history */
    scheduler->history_window_size = DEFAULT_HISTORY_WINDOW;
    scheduler->fitness_history = evocore_calloc(scheduler->history_window_size, sizeof(double));
    if (!scheduler->fitness_history) {
        evocore_free(scheduler);
        evocore_set_error(EVOCORE_ERR_OUT_OF_MEMORY, "Failed to allocate fitness history");
        return NULL;
    }

    /* Initialize history with NaN */
    for (size_t i = 0; i < scheduler->history_window_size; i++) {
        scheduler->fitness_history[i] = NAN;
    }

    scheduler->history_position = 0;
    scheduler->best_fitness_ever = -INFINITY;
    scheduler->generations_since_improvement = 0;
    scheduler->stagnation_threshold = DEFAULT_STAGNATION_THRESHOLD;

    /* Initialize diversity tracking */
    scheduler->current_diversity = 0.5;
    scheduler->min_diversity_threshold = DEFAULT_MIN_DIVERSITY;
    scheduler->avg_diversity = 0.5;

    /* Initialize adaptive parameters */
    if (initial_params) {
        scheduler->initial_mutation_rate = initial_params->optimization_mutation_rate;
        scheduler->current_mutation_rate = initial_params->optimization_mutation_rate;
        scheduler->current_kill_percentage = initial_params->culling_ratio;
        scheduler->current_breed_percentage = initial_params->profitable_optimization_ratio;
        scheduler->initial_population_size = initial_params->target_population_size;
        scheduler->current_population_size = initial_params->target_population_size;
        scheduler->final_population_size = initial_params->min_population_size;
    } else {
        /* Defaults */
        scheduler->initial_mutation_rate = 0.20;
        scheduler->current_mutation_rate = 0.20;
        scheduler->current_kill_percentage = 0.25;
        scheduler->current_breed_percentage = 0.70;
        scheduler->initial_population_size = 1000;
        scheduler->current_population_size = 1000;
        scheduler->final_population_size = 200;
    }

    scheduler->min_mutation_rate = DEFAULT_MIN_MUTATION_RATE;

    /* Initialize scheduling parameters */
    scheduler->decay_alpha = DEFAULT_DECAY_ALPHA;
    scheduler->stagnation_boost_factor = DEFAULT_STAGNATION_BOOST;
    scheduler->diversity_boost_factor = DEFAULT_DIVERSITY_BOOST;
    scheduler->high_variance_kill_pct = DEFAULT_HIGH_VAR_KILL;
    scheduler->medium_variance_kill_pct = DEFAULT_MEDIUM_VAR_KILL;
    scheduler->low_variance_kill_pct = DEFAULT_LOW_VAR_KILL;
    scheduler->high_variance_threshold = DEFAULT_HIGH_VAR_THRESHOLD;
    scheduler->low_variance_threshold = DEFAULT_LOW_VAR_THRESHOLD;

    /* Population sizing */
    scheduler->stagnation_expansion_factor = 1.5;
    scheduler->enable_population_contraction = true;

    evocore_log(LOG_INFO, "Adaptive scheduler created: max_gen=%zu, init_mut=%.3f",
                max_generations, scheduler->initial_mutation_rate);

    return scheduler;
}

void evocore_adaptive_scheduler_free(evocore_adaptive_scheduler_t *scheduler) {
    if (!scheduler) return;

    if (scheduler->fitness_history) {
        evocore_free(scheduler->fitness_history);
    }

    evocore_free(scheduler);
}

/*========================================================================
 * Phase Detection
 *========================================================================*/

evocore_evolution_phase_t evocore_adaptive_scheduler_get_phase(
    const evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return EVOCORE_PHASE_EARLY;

    double progress = evocore_adaptive_scheduler_get_progress(scheduler);

    if (progress < 0.30) {
        return EVOCORE_PHASE_EARLY;
    } else if (progress < 0.70) {
        return EVOCORE_PHASE_MID;
    } else {
        return EVOCORE_PHASE_LATE;
    }
}

double evocore_adaptive_scheduler_get_progress(
    const evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler || scheduler->max_generations == 0) return 0.0;

    return (double)scheduler->current_generation / (double)scheduler->max_generations;
}

/*========================================================================
 * Convergence Tracking
 *========================================================================*/

evocore_error_t evocore_adaptive_scheduler_update(
    evocore_adaptive_scheduler_t *scheduler,
    double best_fitness,
    double avg_fitness,
    double diversity
) {
    if (!scheduler) return EVOCORE_ERR_NULL_PTR;

    /* Update generation counter */
    scheduler->current_generation++;

    /* Update phase */
    scheduler->current_phase = evocore_adaptive_scheduler_get_phase(scheduler);

    /* Add to fitness history (circular buffer) */
    scheduler->fitness_history[scheduler->history_position] = best_fitness;
    scheduler->history_position = (scheduler->history_position + 1) % scheduler->history_window_size;

    /* Update best fitness tracking */
    if (best_fitness > scheduler->best_fitness_ever) {
        scheduler->best_fitness_ever = best_fitness;
        scheduler->generations_since_improvement = 0;
    } else {
        scheduler->generations_since_improvement++;
    }

    /* Update diversity tracking */
    scheduler->current_diversity = diversity;

    /* Update rolling average diversity */
    double alpha = 0.1;  /* Smoothing factor */
    scheduler->avg_diversity = alpha * diversity + (1.0 - alpha) * scheduler->avg_diversity;

    return EVOCORE_OK;
}

bool evocore_adaptive_scheduler_is_stagnant(
    const evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return false;

    return scheduler->generations_since_improvement >= scheduler->stagnation_threshold;
}

double evocore_adaptive_scheduler_get_improvement_rate(
    const evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return 0.0;

    /* Calculate linear trend (slope) of fitness history */
    return calculate_linear_trend(scheduler->fitness_history, scheduler->history_window_size);
}

double evocore_adaptive_scheduler_get_fitness_variance(
    const evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return 0.0;

    /* Calculate coefficient of variation (CV = stddev / mean) */
    size_t count = scheduler->history_window_size;
    double mean = calculate_mean(scheduler->fitness_history, count);
    double stddev = calculate_stddev(scheduler->fitness_history, count, mean);

    if (fabs(mean) < 1e-9) return 0.0;

    return stddev / fabs(mean);
}

/*========================================================================
 * Adaptive Parameter Scheduling
 *========================================================================*/

double evocore_adaptive_scheduler_get_mutation_rate(
    evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return 0.20;

    /* Base rate with exponential decay */
    double progress = evocore_adaptive_scheduler_get_progress(scheduler);
    double base_rate = scheduler->initial_mutation_rate * exp(-scheduler->decay_alpha * progress);

    /* Apply stagnation boost */
    if (evocore_adaptive_scheduler_is_stagnant(scheduler)) {
        base_rate *= scheduler->stagnation_boost_factor;
    }

    /* Apply diversity boost */
    if (scheduler->current_diversity < scheduler->min_diversity_threshold) {
        base_rate *= scheduler->diversity_boost_factor;
    }

    /* Enforce minimum */
    if (base_rate < scheduler->min_mutation_rate) {
        base_rate = scheduler->min_mutation_rate;
    }

    /* Update current rate */
    scheduler->current_mutation_rate = base_rate;

    return base_rate;
}

double evocore_adaptive_scheduler_get_selection_pressure(
    evocore_adaptive_scheduler_t *scheduler,
    double fitness_variance
) {
    if (!scheduler) return 0.25;

    double kill_pct;

    /* High variance → Low pressure (gentle selection) */
    if (fitness_variance > scheduler->high_variance_threshold) {
        kill_pct = scheduler->high_variance_kill_pct;
    }
    /* Low variance → High pressure (strong selection) */
    else if (fitness_variance < scheduler->low_variance_threshold) {
        kill_pct = scheduler->low_variance_kill_pct;
    }
    /* Medium variance → Medium pressure */
    else {
        kill_pct = scheduler->medium_variance_kill_pct;
    }

    scheduler->current_kill_percentage = kill_pct;

    return kill_pct;
}

size_t evocore_adaptive_scheduler_get_population_size(
    evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return 1000;

    size_t target_size;

    /* Phase-based sizing */
    switch (scheduler->current_phase) {
        case EVOCORE_PHASE_EARLY:
            /* Large population for exploration */
            target_size = scheduler->initial_population_size;
            break;

        case EVOCORE_PHASE_MID:
            /* Gradual transition */
            {
                double progress = evocore_adaptive_scheduler_get_progress(scheduler);
                double mid_progress = (progress - 0.30) / 0.40;  /* 0.0 to 1.0 within mid phase */
                target_size = (size_t)(scheduler->initial_population_size -
                                      (scheduler->initial_population_size - scheduler->final_population_size) * mid_progress);
            }
            break;

        case EVOCORE_PHASE_LATE:
            /* Small population for exploitation */
            target_size = scheduler->final_population_size;
            break;

        default:
            target_size = scheduler->initial_population_size;
            break;
    }

    /* Stagnation expansion */
    if (evocore_adaptive_scheduler_is_stagnant(scheduler)) {
        target_size = (size_t)(target_size * scheduler->stagnation_expansion_factor);
    }

    scheduler->current_population_size = target_size;

    return target_size;
}

evocore_error_t evocore_adaptive_scheduler_apply_to_meta(
    evocore_adaptive_scheduler_t *scheduler,
    evocore_meta_params_t *params
) {
    if (!scheduler || !params) return EVOCORE_ERR_NULL_PTR;

    /* Get adapted parameters */
    double mutation_rate = evocore_adaptive_scheduler_get_mutation_rate(scheduler);
    double fitness_variance = evocore_adaptive_scheduler_get_fitness_variance(scheduler);
    double selection_pressure = evocore_adaptive_scheduler_get_selection_pressure(scheduler, fitness_variance);
    size_t population_size = evocore_adaptive_scheduler_get_population_size(scheduler);

    /* Apply to meta-parameters */
    params->optimization_mutation_rate = mutation_rate;
    params->variance_mutation_rate = mutation_rate * 1.2;  /* Slightly higher for variance */
    params->culling_ratio = selection_pressure;
    params->target_population_size = population_size;

    /* Adjust exploration based on phase */
    switch (scheduler->current_phase) {
        case EVOCORE_PHASE_EARLY:
            params->exploration_factor = 0.7;  /* High exploration */
            break;
        case EVOCORE_PHASE_MID:
            params->exploration_factor = 0.5;  /* Balanced */
            break;
        case EVOCORE_PHASE_LATE:
            params->exploration_factor = 0.2;  /* Low exploration */
            break;
    }

    return EVOCORE_OK;
}

/*========================================================================
 * Stagnation Recovery
 *========================================================================*/

evocore_error_t evocore_adaptive_scheduler_trigger_recovery(
    evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return EVOCORE_ERR_NULL_PTR;

    evocore_log(LOG_INFO, "Stagnation recovery triggered at generation %zu", scheduler->current_generation);

    /* Boost mutation rate */
    scheduler->current_mutation_rate *= scheduler->stagnation_boost_factor;

    /* Expand population */
    scheduler->current_population_size = (size_t)(scheduler->current_population_size * scheduler->stagnation_expansion_factor);

    /* Reset stagnation counter */
    scheduler->generations_since_improvement = 0;

    return EVOCORE_OK;
}

evocore_error_t evocore_adaptive_scheduler_diversity_intervention(
    evocore_adaptive_scheduler_t *scheduler,
    double diversity,
    char *out_action,
    size_t action_size
) {
    if (!scheduler || !out_action) return EVOCORE_ERR_NULL_PTR;

    if (diversity < 0.1) {
        /* Aggressive intervention */
        snprintf(out_action, action_size, "ADD_RANDOM_20PCT");
        evocore_log(LOG_WARN, "Diversity critical (%.3f): Adding 20%% random individuals", diversity);
    } else if (diversity < 0.2) {
        /* Moderate intervention */
        snprintf(out_action, action_size, "ADD_RANDOM_10PCT");
        evocore_log(LOG_WARN, "Diversity low (%.3f): Adding 10%% random individuals", diversity);
    } else if (diversity < 0.3) {
        /* Mild intervention */
        snprintf(out_action, action_size, "INCREASE_MUTATION");
        evocore_log(LOG_INFO, "Diversity below target (%.3f): Increasing mutation rate", diversity);
        scheduler->current_mutation_rate *= scheduler->diversity_boost_factor;
    } else {
        /* No intervention needed */
        snprintf(out_action, action_size, "NONE");
    }

    return EVOCORE_OK;
}

/*========================================================================
 * Diagnostics
 *========================================================================*/

evocore_error_t evocore_adaptive_scheduler_get_state(
    const evocore_adaptive_scheduler_t *scheduler,
    char *out_state,
    size_t size
) {
    if (!scheduler || !out_state) return EVOCORE_ERR_NULL_PTR;

    const char *phase_str;
    switch (scheduler->current_phase) {
        case EVOCORE_PHASE_EARLY: phase_str = "EARLY"; break;
        case EVOCORE_PHASE_MID:   phase_str = "MID";   break;
        case EVOCORE_PHASE_LATE:  phase_str = "LATE";  break;
        default:                  phase_str = "UNKNOWN"; break;
    }

    snprintf(out_state, size,
             "Gen=%zu/%zu Phase=%s Mut=%.4f Kill=%.2f Pop=%zu Div=%.3f Stag=%zu",
             scheduler->current_generation,
             scheduler->max_generations,
             phase_str,
             scheduler->current_mutation_rate,
             scheduler->current_kill_percentage,
             scheduler->current_population_size,
             scheduler->current_diversity,
             scheduler->generations_since_improvement);

    return EVOCORE_OK;
}

void evocore_adaptive_scheduler_print_stats(
    const evocore_adaptive_scheduler_t *scheduler
) {
    if (!scheduler) return;

    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("ADAPTIVE SCHEDULER STATISTICS\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    const char *phase_str;
    switch (scheduler->current_phase) {
        case EVOCORE_PHASE_EARLY: phase_str = "EARLY (Exploration)"; break;
        case EVOCORE_PHASE_MID:   phase_str = "MID (Transition)";   break;
        case EVOCORE_PHASE_LATE:  phase_str = "LATE (Exploitation)"; break;
        default:                  phase_str = "UNKNOWN"; break;
    }

    double progress = evocore_adaptive_scheduler_get_progress(scheduler);
    double improvement_rate = evocore_adaptive_scheduler_get_improvement_rate(scheduler);
    double fitness_variance = evocore_adaptive_scheduler_get_fitness_variance(scheduler);

    printf("\nProgress & Phase:\n");
    printf("  Generation:           %zu / %zu\n", scheduler->current_generation, scheduler->max_generations);
    printf("  Progress:             %.1f%%\n", progress * 100.0);
    printf("  Current Phase:        %s\n", phase_str);

    printf("\nConvergence Metrics:\n");
    printf("  Best Fitness Ever:    %.6f\n", scheduler->best_fitness_ever);
    printf("  Gens Since Improve:   %zu\n", scheduler->generations_since_improvement);
    printf("  Improvement Rate:     %.6f\n", improvement_rate);
    printf("  Fitness Variance:     %.4f (CV)\n", fitness_variance);
    printf("  Stagnant:             %s\n", evocore_adaptive_scheduler_is_stagnant(scheduler) ? "YES" : "NO");

    printf("\nAdaptive Parameters:\n");
    printf("  Mutation Rate:        %.4f (init: %.4f)\n", scheduler->current_mutation_rate, scheduler->initial_mutation_rate);
    printf("  Kill Percentage:      %.2f%%\n", scheduler->current_kill_percentage * 100.0);
    printf("  Breed Percentage:     %.2f%%\n", scheduler->current_breed_percentage * 100.0);
    printf("  Population Size:      %zu (init: %zu, final: %zu)\n",
           scheduler->current_population_size,
           scheduler->initial_population_size,
           scheduler->final_population_size);

    printf("\nDiversity:\n");
    printf("  Current:              %.4f\n", scheduler->current_diversity);
    printf("  Average:              %.4f\n", scheduler->avg_diversity);
    printf("  Threshold:            %.4f\n", scheduler->min_diversity_threshold);

    printf("\n═══════════════════════════════════════════════════════════════\n\n");
}
