/**
 * Evocore Exploration Control Module
 *
 * Dynamic exploration vs exploitation strategies for evolutionary optimization.
 *
 * This module provides multiple strategies for balancing exploration (trying new
 * solutions) with exploitation (refining known good solutions), which is critical
 * for adaptive optimization in changing environments.
 *
 * Strategies:
 * - Fixed: Constant exploration rate
 * - Decay: Exploration decreases over time
 * - Adaptive: Adjusts based on recent performance
 * - UCB1: Upper Confidence Bound from bandit theory
 * - Boltzmann: Temperature-based simulated annealing
 */

#ifndef EVOCORE_EXPLORATION_H
#define EVOCORE_EXPLORATION_H

#include <stddef.h>
#include <stdbool.h>
#include <time.h>

/*========================================================================
 * Data Structures
 *========================================================================*/

/**
 * Exploration strategies
 */
typedef enum {
    EVOCORE_EXPLORE_FIXED,      /* Constant exploration rate */
    EVOCORE_EXPLORE_DECAY,      /* Exponential decay over generations */
    EVOCORE_EXPLORE_ADAPTIVE,   /* Adjusts based on fitness improvements */
    EVOCORE_EXPLORE_UCB1,       /* Upper Confidence Bound */
    EVOCORE_EXPLORE_BOLTZMANN   /* Simulated annealing */
} evocore_explore_strategy_t;

/**
 * Exploration state
 *
 * Tracks the current exploration parameters and history.
 */
typedef struct {
    evocore_explore_strategy_t strategy;
    double base_rate;           /* Base exploration rate (0-1) */
    double current_rate;        /* Current exploration rate (0-1) */
    double min_rate;            /* Minimum exploration rate */
    double max_rate;            /* Maximum exploration rate */

    /* Strategy-specific parameters */
    double decay_rate;          /* For DECAY: exponential decay factor */
    double temperature;         /* For BOLTZMANN: current temperature */
    double cooling_rate;        /* For BOLTZMANN: cooling factor */
    double ucb_c;               /* For UCB1: exploration constant */

    /* Adaptive tracking */
    double best_fitness;        /* Best fitness seen */
    double recent_best;         /* Recent best fitness */
    size_t stagnation_count;    /* Generations without improvement */
    size_t total_evaluations;   /* Total evaluations performed */

    time_t start_time;          /* When exploration started */
} evocore_exploration_t;

/**
 * Bandit arm for UCB1
 *
 * Tracks statistics for a single option (e.g., parameter setting).
 */
typedef struct {
    size_t count;               /* Number of times selected */
    double total_reward;        /* Cumulative reward */
    double mean_reward;         /* Average reward */
} evocore_bandit_arm_t;

/**
 * Multi-armed bandit
 *
 * Tracks multiple options with UCB1 selection.
 */
typedef struct {
    evocore_bandit_arm_t *arms; /* Array of bandit arms */
    size_t count;               /* Number of arms */
    size_t total_pulls;         /* Total selections across all arms */
    double ucb_c;               /* Exploration constant */
} evocore_bandit_t;

/*========================================================================
 * Exploration Management
 *========================================================================*/

/**
 * Create exploration controller
 *
 * @param strategy Exploration strategy to use
 * @param base_rate Initial exploration rate (0-1)
 * @return New exploration controller, or NULL on error
 */
evocore_exploration_t* evocore_exploration_create(
    evocore_explore_strategy_t strategy,
    double base_rate
);

/**
 * Free exploration controller
 *
 * @param exp Exploration controller to free
 */
void evocore_exploration_free(evocore_exploration_t *exp);

/**
 * Reset exploration state
 *
 * Resets to initial state but preserves strategy.
 *
 * @param exp Exploration controller
 */
void evocore_exploration_reset(evocore_exploration_t *exp);

/**
 * Set exploration bounds
 *
 * @param exp Exploration controller
 * @param min_rate Minimum exploration rate (0-1)
 * @param max_rate Maximum exploration rate (0-1)
 */
void evocore_exploration_set_bounds(
    evocore_exploration_t *exp,
    double min_rate,
    double max_rate
);

/**
 * Set decay rate for DECAY strategy
 *
 * @param exp Exploration controller
 * @param decay_rate Decay factor per generation (0-1)
 */
void evocore_exploration_set_decay_rate(
    evocore_exploration_t *exp,
    double decay_rate
);

/**
 * Set temperature for BOLTZMANN strategy
 *
 * @param exp Exploration controller
 * @param temperature Initial temperature
 * @param cooling_rate Cooling factor (0-1)
 */
void evocore_exploration_set_temperature(
    evocore_exploration_t *exp,
    double temperature,
    double cooling_rate
);

/**
 * Set UCB1 constant
 *
 * @param exp Exploration controller
 * @param ucb_c Exploration constant (default: sqrt(2))
 */
void evocore_exploration_set_ucb_c(
    evocore_exploration_t *exp,
    double ucb_c
);

/*========================================================================
 * Rate Updates
 *========================================================================*/

/**
 * Update exploration rate
 *
 * Updates the current exploration rate based on the strategy.
 *
 * @param exp Exploration controller
 * @param generation Current generation number
 * @param best_fitness Best fitness in current generation
 * @return Current exploration rate
 */
double evocore_exploration_update(
    evocore_exploration_t *exp,
    size_t generation,
    double best_fitness
);

/**
 * Get current exploration rate
 *
 * @param exp Exploration controller
 * @return Current exploration rate (0-1)
 */
double evocore_exploration_get_rate(const evocore_exploration_t *exp);

/**
 * Should explore?
 *
 * Returns true if exploration should happen based on current rate.
 *
 * @param exp Exploration controller
 * @param seed Random seed pointer
 * @return true if should explore
 */
bool evocore_exploration_should_explore(
    const evocore_exploration_t *exp,
    unsigned int *seed
);

/*========================================================================
 * Bandit Selection (UCB1)
 *========================================================================*/

/**
 * Create multi-armed bandit
 *
 * @param arm_count Number of bandit arms
 * @param ucb_c Exploration constant
 * @return New bandit, or NULL on error
 */
evocore_bandit_t* evocore_bandit_create(size_t arm_count, double ucb_c);

/**
 * Free bandit
 *
 * @param bandit Bandit to free
 */
void evocore_bandit_free(evocore_bandit_t *bandit);

/**
 * Select arm using UCB1
 *
 * Selects an arm based on Upper Confidence Bound.
 *
 * @param bandit Bandit
 * @return Index of selected arm
 */
size_t evocore_bandit_select_ucb(const evocore_bandit_t *bandit);

/**
 * Update arm with reward
 *
 * Updates the statistics for an arm after receiving a reward.
 *
 * @param bandit Bandit
 * @param arm Index of arm to update
 * @param reward Observed reward
 */
void evocore_bandit_update(evocore_bandit_t *bandit, size_t arm, double reward);

/**
 * Get arm count
 *
 * @param bandit Bandit
 * @return Number of arms
 */
size_t evocore_bandit_arm_count(const evocore_bandit_t *bandit);

/**
 * Get arm statistics
 *
 * @param bandit Bandit
 * @param arm Index of arm
 * @param out_count Output: number of pulls, or NULL
 * @param out_mean Output: mean reward, or NULL
 * @return true on success
 */
bool evocore_bandit_get_stats(
    const evocore_bandit_t *bandit,
    size_t arm,
    size_t *out_count,
    double *out_mean
);

/**
 * Reset bandit
 *
 * Clears all arm statistics.
 *
 * @param bandit Bandit
 */
void evocore_bandit_reset(evocore_bandit_t *bandit);

/*========================================================================
 * Boltzmann Selection
 *========================================================================*/

/**
 * Boltzmann selection
 *
 * Selects an option based on Boltzmann (softmax) distribution.
 * Higher values are more likely to be selected.
 *
 * @param values Array of values
 * @param count Number of values
 * @param temperature Temperature parameter (higher = more exploration)
 * @param seed Random seed pointer
 * @return Index of selected option
 */
size_t evocore_boltzmann_select(
    const double *values,
    size_t count,
    double temperature,
    unsigned int *seed
);

/**
 * Cool temperature
 *
 * Reduces temperature by cooling factor.
 *
 * @param temperature Current temperature
 * @param cooling_rate Cooling factor (0-1)
 * @return New temperature
 */
double evocore_cool_temperature(double temperature, double cooling_rate);

/*========================================================================
 * Adaptive Exploration
 *========================================================================*/

/**
 * Detect stagnation
 *
 * Returns true if fitness hasn't improved recently.
 *
 * @param exp Exploration controller
 * @param threshold Generations without improvement to consider stagnant
 * @return true if stagnant
 */
bool evocore_exploration_is_stagnant(
    const evocore_exploration_t *exp,
    size_t threshold
);

/**
 * Increase exploration on stagnation
 *
 * Temporarily increases exploration when stuck.
 *
 * @param exp Exploration controller
 * @param factor Multiplier for exploration rate
 */
void evocore_exploration_boost(
    evocore_exploration_t *exp,
    double factor
);

/**
 * Calculate improvement rate
 *
 * @param exp Exploration controller
 * @return Improvement rate per evaluation
 */
double evocore_exploration_improvement_rate(const evocore_exploration_t *exp);

#endif /* EVOCORE_EXPLORATION_H */
