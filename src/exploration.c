/**
 * Evocore Exploration Control Implementation
 *
 * Implements dynamic exploration vs exploitation strategies.
 */

#define _GNU_SOURCE
#include "evocore/exploration.h"
#include "evocore/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*========================================================================
 * Constants
 *========================================================================*/

#define DEFAULT_MIN_RATE 0.01
#define DEFAULT_MAX_RATE 1.0
#define DEFAULT_DECAY_RATE 0.98
#define DEFAULT_TEMPERATURE 100.0
#define DEFAULT_COOLING_RATE 0.95
#define DEFAULT_UCB_C 1.4142  /* sqrt(2) */

#define STAGNATION_THRESHOLD 50
#define BOOST_FACTOR 2.0
#define BOOST_DURATION 10

/*========================================================================
 * Exploration Management
 *========================================================================*/

evocore_exploration_t* evocore_exploration_create(
    evocore_explore_strategy_t strategy,
    double base_rate
) {
    if (base_rate < 0.0 || base_rate > 1.0) return NULL;

    evocore_exploration_t *exp = calloc(1, sizeof(evocore_exploration_t));
    if (!exp) return NULL;

    exp->strategy = strategy;
    exp->base_rate = base_rate;
    exp->current_rate = base_rate;
    exp->min_rate = DEFAULT_MIN_RATE;
    exp->max_rate = DEFAULT_MAX_RATE;
    exp->decay_rate = DEFAULT_DECAY_RATE;
    exp->temperature = DEFAULT_TEMPERATURE;
    exp->cooling_rate = DEFAULT_COOLING_RATE;
    exp->ucb_c = DEFAULT_UCB_C;

    exp->best_fitness = -INFINITY;
    exp->recent_best = -INFINITY;
    exp->stagnation_count = 0;
    exp->total_evaluations = 0;

    exp->start_time = time(NULL);

    return exp;
}

void evocore_exploration_free(evocore_exploration_t *exp) {
    free(exp);
}

void evocore_exploration_reset(evocore_exploration_t *exp) {
    if (!exp) return;

    exp->current_rate = exp->base_rate;
    exp->best_fitness = -INFINITY;
    exp->recent_best = -INFINITY;
    exp->stagnation_count = 0;
    exp->total_evaluations = 0;
    exp->temperature = DEFAULT_TEMPERATURE;
    exp->start_time = time(NULL);
}

void evocore_exploration_set_bounds(
    evocore_exploration_t *exp,
    double min_rate,
    double max_rate
) {
    if (!exp) return;
    exp->min_rate = fmin(1.0, fmax(0.0, min_rate));
    exp->max_rate = fmin(1.0, fmax(0.0, max_rate));
}

void evocore_exploration_set_decay_rate(
    evocore_exploration_t *exp,
    double decay_rate
) {
    if (!exp) return;
    exp->decay_rate = fmin(1.0, fmax(0.0, decay_rate));
}

void evocore_exploration_set_temperature(
    evocore_exploration_t *exp,
    double temperature,
    double cooling_rate
) {
    if (!exp) return;
    exp->temperature = fmax(0.001, temperature);
    exp->cooling_rate = fmin(1.0, fmax(0.0, cooling_rate));
}

void evocore_exploration_set_ucb_c(
    evocore_exploration_t *exp,
    double ucb_c
) {
    if (!exp) return;
    exp->ucb_c = fmax(0.0, ucb_c);
}

/*========================================================================
 * Rate Updates
 *========================================================================*/

double evocore_exploration_update(
    evocore_exploration_t *exp,
    size_t generation,
    double best_fitness
) {
    if (!exp) return 0.0;

    exp->total_evaluations++;

    /* Update best fitness tracking */
    bool improved = false;
    if (best_fitness > exp->best_fitness) {
        exp->best_fitness = best_fitness;
        improved = true;
    }

    /* Update strategy-specific rate */
    switch (exp->strategy) {
        case EVOCORE_EXPLORE_FIXED:
            /* Constant rate */
            exp->current_rate = exp->base_rate;
            break;

        case EVOCORE_EXPLORE_DECAY: {
            /* Exponential decay: rate = base * decay^gen */
            double decayed = exp->base_rate * pow(exp->decay_rate, (double)generation);
            exp->current_rate = fmax(exp->min_rate, decayed);
            break;
        }

        case EVOCORE_EXPLORE_ADAPTIVE: {
            /* Increase on stagnation, decrease on improvement */
            if (improved) {
                exp->stagnation_count = 0;
                /* Decrease exploration when improving */
                exp->current_rate *= 0.9;
            } else {
                exp->stagnation_count++;
                /* Increase exploration when stagnant */
                if (exp->stagnation_count > STAGNATION_THRESHOLD / 2) {
                    exp->current_rate *= 1.1;
                }
            }
            break;
        }

        case EVOCORE_EXPLORE_UCB1:
            /* Rate decreases as we gather more data */
            if (exp->total_evaluations > 0) {
                exp->current_rate = exp->ucb_c / sqrt((double)exp->total_evaluations);
            }
            break;

        case EVOCORE_EXPLORE_BOLTZMANN: {
            /* Based on temperature */
            exp->current_rate = exp->temperature / DEFAULT_TEMPERATURE;
            break;
        }
    }

    /* Clamp to bounds */
    exp->current_rate = fmax(exp->min_rate,
                            fmin(exp->max_rate, exp->current_rate));

    /* Cool temperature for Boltzmann */
    if (exp->strategy == EVOCORE_EXPLORE_BOLTZMANN) {
        exp->temperature = evocore_cool_temperature(exp->temperature, exp->cooling_rate);
    }

    return exp->current_rate;
}

double evocore_exploration_get_rate(const evocore_exploration_t *exp) {
    return exp ? exp->current_rate : 0.0;
}

bool evocore_exploration_should_explore(
    const evocore_exploration_t *exp,
    unsigned int *seed
) {
    if (!exp) return false;
    if (exp->current_rate <= 0.0) return false;
    if (exp->current_rate >= 1.0) return true;

    double r = (double)rand_r(seed) / (double)RAND_MAX;
    return r < exp->current_rate;
}

/*========================================================================
 * Bandit Selection (UCB1)
 *========================================================================*/

evocore_bandit_t* evocore_bandit_create(size_t arm_count, double ucb_c) {
    if (arm_count == 0) return NULL;

    evocore_bandit_t *bandit = calloc(1, sizeof(evocore_bandit_t));
    if (!bandit) return NULL;

    bandit->arms = calloc(arm_count, sizeof(evocore_bandit_arm_t));
    if (!bandit->arms) {
        free(bandit);
        return NULL;
    }

    bandit->count = arm_count;
    bandit->ucb_c = ucb_c;
    bandit->total_pulls = 0;

    return bandit;
}

void evocore_bandit_free(evocore_bandit_t *bandit) {
    if (!bandit) return;
    free(bandit->arms);
    free(bandit);
}

size_t evocore_bandit_select_ucb(const evocore_bandit_t *bandit) {
    if (!bandit || bandit->count == 0) return 0;

    size_t best_arm = 0;
    double best_ucb = -INFINITY;

    for (size_t i = 0; i < bandit->count; i++) {
        const evocore_bandit_arm_t *arm = &bandit->arms[i];

        double ucb;
        if (arm->count == 0) {
            /* Never pulled, select it */
            ucb = INFINITY;
        } else {
            /* UCB1 formula: mean + c * sqrt(ln(n) / n_i) */
            double exploration = bandit->ucb_c *
                               sqrt(log((double)bandit->total_pulls) / (double)arm->count);
            ucb = arm->mean_reward + exploration;
        }

        if (ucb > best_ucb) {
            best_ucb = ucb;
            best_arm = i;
        }
    }

    return best_arm;
}

void evocore_bandit_update(evocore_bandit_t *bandit, size_t arm_idx, double reward) {
    if (!bandit || arm_idx >= bandit->count) return;

    evocore_bandit_arm_t *arm = &bandit->arms[arm_idx];

    arm->count++;
    arm->total_reward += reward;
    arm->mean_reward = arm->total_reward / (double)arm->count;

    bandit->total_pulls++;
}

size_t evocore_bandit_arm_count(const evocore_bandit_t *bandit) {
    return bandit ? bandit->count : 0;
}

bool evocore_bandit_get_stats(
    const evocore_bandit_t *bandit,
    size_t arm_idx,
    size_t *out_count,
    double *out_mean
) {
    if (!bandit || arm_idx >= bandit->count) return false;

    const evocore_bandit_arm_t *arm = &bandit->arms[arm_idx];

    if (out_count) *out_count = arm->count;
    if (out_mean) *out_mean = arm->mean_reward;

    return true;
}

void evocore_bandit_reset(evocore_bandit_t *bandit) {
    if (!bandit) return;

    memset(bandit->arms, 0, bandit->count * sizeof(evocore_bandit_arm_t));
    bandit->total_pulls = 0;
}

/*========================================================================
 * Boltzmann Selection
 *========================================================================*/

size_t evocore_boltzmann_select(
    const double *values,
    size_t count,
    double temperature,
    unsigned int *seed
) {
    if (!values || count == 0) return 0;
    if (temperature < 0.001) {
        /* Very low temperature: select max */
        size_t best = 0;
        double best_val = values[0];
        for (size_t i = 1; i < count; i++) {
            if (values[i] > best_val) {
                best_val = values[i];
                best = i;
            }
        }
        return best;
    }

    /* Calculate Boltzmann probabilities: p_i = exp(v_i / T) / sum(exp(v_j / T)) */
    double *probs = calloc(count, sizeof(double));
    if (!probs) return 0;

    double sum = 0.0;
    double max_val = values[0];
    for (size_t i = 1; i < count; i++) {
        if (values[i] > max_val) max_val = values[i];
    }

    /* Subtract max for numerical stability */
    for (size_t i = 0; i < count; i++) {
        probs[i] = exp((values[i] - max_val) / temperature);
        sum += probs[i];
    }

    if (sum < 0.0001) {
        /* All values are the same or very low temperature */
        free(probs);
        return (size_t)((double)rand_r(seed) / (double)RAND_MAX * count);
    }

    /* Normalize and select */
    double r = (double)rand_r(seed) / (double)RAND_MAX * sum;
    double cumulative = 0.0;
    size_t selected = 0;

    for (size_t i = 0; i < count; i++) {
        cumulative += probs[i];
        if (r <= cumulative) {
            selected = i;
            break;
        }
    }

    free(probs);
    return selected;
}

double evocore_cool_temperature(double temperature, double cooling_rate) {
    return fmax(0.001, temperature * cooling_rate);
}

/*========================================================================
 * Adaptive Exploration
 *========================================================================*/

bool evocore_exploration_is_stagnant(
    const evocore_exploration_t *exp,
    size_t threshold
) {
    if (!exp) return false;
    return exp->stagnation_count >= threshold;
}

void evocore_exploration_boost(
    evocore_exploration_t *exp,
    double factor
) {
    if (!exp) return;

    exp->current_rate *= factor;
    exp->current_rate = fmin(exp->max_rate, exp->current_rate);
}

double evocore_exploration_improvement_rate(const evocore_exploration_t *exp) {
    if (!exp || exp->total_evaluations == 0) return 0.0;

    /* Improvement per evaluation (simplified) */
    if (isinf(exp->best_fitness) || exp->best_fitness == -INFINITY) {
        return 0.0;
    }

    return exp->best_fitness / (double)exp->total_evaluations;
}
