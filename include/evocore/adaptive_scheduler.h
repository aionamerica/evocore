#ifndef EVOCORE_ADAPTIVE_SCHEDULER_H
#define EVOCORE_ADAPTIVE_SCHEDULER_H

#include "evocore/error.h"
#include "evocore/meta.h"
#include <stddef.h>
#include <stdbool.h>

/*========================================================================
 * Adaptive Parameter Scheduler
 * ======================================================================
 *
 * Phase 2 Improvement: Adaptive Evolution Parameters
 *
 * This module implements dynamic parameter scheduling that adapts evolution
 * parameters based on convergence state, population diversity, and stagnation
 * detection.
 *
 * Key Features:
 * - Evolution phase detection (Early/Mid/Late)
 * - Convergence metrics tracking
 * - Adaptive mutation scheduling (exponential decay + stagnation boost)
 * - Dynamic selection pressure based on fitness variance
 * - Adaptive population sizing
 *
 * Expected Results:
 * - 30-50% faster convergence to target fitness
 * - Automatic recovery from stagnation (3x mutation boost)
 * - Better late-stage fine-tuning (adaptive jitter: 20% → 0.1%)
 *
 * Integration: Works with existing meta-evolution system to provide
 * real-time parameter adaptation.
 */

/*========================================================================
 * Evolution Phase Enumeration
 *========================================================================*/

/**
 * Evolution phase categorization
 *
 * Phases guide parameter adaptation strategy:
 * - EARLY: High exploration, broad search (0-30% progress)
 * - MID: Balanced exploration-exploitation (30-70% progress)
 * - LATE: Fine-tuning, exploitation (70-100% progress)
 */
typedef enum {
    EVOCORE_PHASE_EARLY = 0,   /* 0-30%: Exploration phase */
    EVOCORE_PHASE_MID = 1,      /* 30-70%: Transition phase */
    EVOCORE_PHASE_LATE = 2,     /* 70-100%: Exploitation phase */
} evocore_evolution_phase_t;

/*========================================================================
 * Adaptive Scheduler State
 *========================================================================*/

/**
 * Adaptive parameter scheduler state
 *
 * Tracks evolution progress and adjusts parameters dynamically.
 */
typedef struct {
    /* ========================================================================
     * Progress Tracking
     * ======================================================================== */

    /** Current generation number */
    size_t current_generation;

    /** Maximum generations (for phase detection) */
    size_t max_generations;

    /** Current evolution phase */
    evocore_evolution_phase_t current_phase;

    /* ========================================================================
     * Convergence Metrics
     * ======================================================================== */

    /** Rolling window of fitness values (for trend detection) */
    double *fitness_history;

    /** Size of fitness history window (default: 50) */
    size_t history_window_size;

    /** Current position in circular buffer */
    size_t history_position;

    /** Best fitness ever seen */
    double best_fitness_ever;

    /** Generations since last improvement */
    size_t generations_since_improvement;

    /** Stagnation detection threshold (default: 20 generations) */
    size_t stagnation_threshold;

    /* ========================================================================
     * Diversity Tracking
     * ======================================================================== */

    /** Current population diversity (0.0 to 1.0) */
    double current_diversity;

    /** Minimum acceptable diversity (default: 0.1) */
    double min_diversity_threshold;

    /** Rolling average of diversity */
    double avg_diversity;

    /* ========================================================================
     * Adaptive Parameters (Modified Meta-Parameters)
     * ======================================================================== */

    /** Current mutation rate (adapts over time) */
    double current_mutation_rate;

    /** Initial mutation rate (baseline) */
    double initial_mutation_rate;

    /** Minimum mutation rate floor (default: 0.001) */
    double min_mutation_rate;

    /** Current selection pressure (kill percentage) */
    double current_kill_percentage;

    /** Current breeding percentage */
    double current_breed_percentage;

    /** Current population size target */
    size_t current_population_size;

    /* ========================================================================
     * Scheduling Parameters
     * ======================================================================== */

    /** Exponential decay factor for mutation rate (default: 0.01) */
    double decay_alpha;

    /** Stagnation mutation boost multiplier (default: 3.0) */
    double stagnation_boost_factor;

    /** Low diversity mutation boost multiplier (default: 1.5) */
    double diversity_boost_factor;

    /** High variance kill percentage (default: 0.15) */
    double high_variance_kill_pct;

    /** Medium variance kill percentage (default: 0.25) */
    double medium_variance_kill_pct;

    /** Low variance kill percentage (default: 0.40) */
    double low_variance_kill_pct;

    /** Fitness variance thresholds for selection pressure */
    double high_variance_threshold;   /* CV > 0.15 */
    double low_variance_threshold;    /* CV < 0.05 */

    /* ========================================================================
     * Population Sizing
     * ======================================================================== */

    /** Initial population size (exploration) */
    size_t initial_population_size;

    /** Final population size (exploitation) */
    size_t final_population_size;

    /** Stagnation expansion multiplier (default: 1.5) */
    double stagnation_expansion_factor;

    /** Convergence contraction enabled */
    bool enable_population_contraction;

} evocore_adaptive_scheduler_t;

/*========================================================================
 * Scheduler Lifecycle
 *========================================================================*/

/**
 * Create and initialize adaptive scheduler
 *
 * @param max_generations   Maximum generations (for phase detection)
 * @param initial_params    Initial meta-parameters (optional, can be NULL)
 * @return Scheduler instance, or NULL on error
 */
evocore_adaptive_scheduler_t* evocore_adaptive_scheduler_create(
    size_t max_generations,
    const evocore_meta_params_t *initial_params
);

/**
 * Free scheduler resources
 *
 * @param scheduler Scheduler to free
 */
void evocore_adaptive_scheduler_free(evocore_adaptive_scheduler_t *scheduler);

/*========================================================================
 * Phase Detection
 *========================================================================*/

/**
 * Detect current evolution phase based on progress
 *
 * Phase thresholds:
 * - Early: 0-30% of max_generations
 * - Mid:   30-70% of max_generations
 * - Late:  70-100% of max_generations
 *
 * @param scheduler Scheduler instance
 * @return Current evolution phase
 */
evocore_evolution_phase_t evocore_adaptive_scheduler_get_phase(
    const evocore_adaptive_scheduler_t *scheduler
);

/**
 * Get phase progress (0.0 to 1.0)
 *
 * @param scheduler Scheduler instance
 * @return Progress within current generation range (0.0 = start, 1.0 = end)
 */
double evocore_adaptive_scheduler_get_progress(
    const evocore_adaptive_scheduler_t *scheduler
);

/*========================================================================
 * Convergence Tracking
 *========================================================================*/

/**
 * Update fitness history and convergence metrics
 *
 * Should be called once per generation with current best fitness.
 *
 * @param scheduler     Scheduler instance
 * @param best_fitness  Current generation best fitness
 * @param avg_fitness   Current generation average fitness
 * @param diversity     Current population diversity (0.0 to 1.0)
 * @return EVOCORE_OK on success
 */
evocore_error_t evocore_adaptive_scheduler_update(
    evocore_adaptive_scheduler_t *scheduler,
    double best_fitness,
    double avg_fitness,
    double diversity
);

/**
 * Detect if evolution is stagnant
 *
 * Stagnation = no improvement for N consecutive generations
 *
 * @param scheduler Scheduler instance
 * @return true if stagnant, false otherwise
 */
bool evocore_adaptive_scheduler_is_stagnant(
    const evocore_adaptive_scheduler_t *scheduler
);

/**
 * Calculate fitness improvement rate (slope of fitness history)
 *
 * Positive = improving, Negative = degrading, ~0 = stagnant
 *
 * @param scheduler Scheduler instance
 * @return Fitness improvement rate
 */
double evocore_adaptive_scheduler_get_improvement_rate(
    const evocore_adaptive_scheduler_t *scheduler
);

/**
 * Calculate fitness variance (coefficient of variation)
 *
 * Used for adaptive selection pressure:
 * - High variance (CV > 0.15) → Low selection pressure
 * - Medium variance (CV 0.05-0.15) → Medium pressure
 * - Low variance (CV < 0.05) → High pressure
 *
 * @param scheduler Scheduler instance
 * @return Coefficient of variation of fitness history
 */
double evocore_adaptive_scheduler_get_fitness_variance(
    const evocore_adaptive_scheduler_t *scheduler
);

/*========================================================================
 * Adaptive Parameter Scheduling
 *========================================================================*/

/**
 * Get adapted mutation rate for current state
 *
 * Formula:
 *   base_rate = initial_rate * exp(-alpha * progress)
 *   if stagnant: rate *= stagnation_boost (3x)
 *   if low_diversity: rate *= diversity_boost (1.5x)
 *   rate = max(rate, min_mutation_rate)
 *
 * @param scheduler Scheduler instance
 * @return Adapted mutation rate
 */
double evocore_adaptive_scheduler_get_mutation_rate(
    evocore_adaptive_scheduler_t *scheduler
);

/**
 * Get adapted selection pressure (kill percentage)
 *
 * Based on fitness variance:
 * - High variance → 15% kill (gentle pressure)
 * - Medium variance → 25% kill (moderate pressure)
 * - Low variance → 40% kill (strong pressure)
 *
 * @param scheduler         Scheduler instance
 * @param fitness_variance  Current fitness coefficient of variation
 * @return Adapted kill percentage (0.0 to 1.0)
 */
double evocore_adaptive_scheduler_get_selection_pressure(
    evocore_adaptive_scheduler_t *scheduler,
    double fitness_variance
);

/**
 * Get adapted population size
 *
 * Strategy:
 * - Early phase: Large population (exploration)
 * - Late phase: Small population (exploitation)
 * - Stagnation: Expand by 1.5x (add diversity)
 * - Converged: Contract to minimum (efficiency)
 *
 * @param scheduler Scheduler instance
 * @return Target population size
 */
size_t evocore_adaptive_scheduler_get_population_size(
    evocore_adaptive_scheduler_t *scheduler
);

/**
 * Apply scheduled parameters to meta-parameters structure
 *
 * Updates the provided meta-parameters with scheduled values.
 *
 * @param scheduler Scheduler instance
 * @param params    Meta-parameters to update
 * @return EVOCORE_OK on success
 */
evocore_error_t evocore_adaptive_scheduler_apply_to_meta(
    evocore_adaptive_scheduler_t *scheduler,
    evocore_meta_params_t *params
);

/*========================================================================
 * Stagnation Recovery
 *========================================================================*/

/**
 * Trigger stagnation recovery intervention
 *
 * Actions:
 * - Boost mutation rate by stagnation_boost_factor (3x)
 * - Expand population if enabled
 * - Reset stagnation counter
 *
 * @param scheduler Scheduler instance
 * @return EVOCORE_OK on success
 */
evocore_error_t evocore_adaptive_scheduler_trigger_recovery(
    evocore_adaptive_scheduler_t *scheduler
);

/**
 * Trigger diversity intervention
 *
 * Called when diversity drops below threshold.
 *
 * Actions:
 * - diversity < 0.1: Aggressive (add 20% random individuals)
 * - diversity < 0.2: Moderate (add 10% random individuals)
 * - diversity < 0.3: Mild (increase mutation rate)
 *
 * @param scheduler     Scheduler instance
 * @param diversity     Current diversity
 * @param out_action    Output: recommended action string
 * @return EVOCORE_OK on success
 */
evocore_error_t evocore_adaptive_scheduler_diversity_intervention(
    evocore_adaptive_scheduler_t *scheduler,
    double diversity,
    char *out_action,
    size_t action_size
);

/*========================================================================
 * Diagnostics
 *========================================================================*/

/**
 * Get scheduler state for logging/debugging
 *
 * @param scheduler Scheduler instance
 * @param out_state Output buffer for state description
 * @param size      Buffer size
 * @return EVOCORE_OK on success
 */
evocore_error_t evocore_adaptive_scheduler_get_state(
    const evocore_adaptive_scheduler_t *scheduler,
    char *out_state,
    size_t size
);

/**
 * Print scheduler statistics to stdout
 *
 * @param scheduler Scheduler instance
 */
void evocore_adaptive_scheduler_print_stats(
    const evocore_adaptive_scheduler_t *scheduler
);

#endif /* EVOCORE_ADAPTIVE_SCHEDULER_H */
