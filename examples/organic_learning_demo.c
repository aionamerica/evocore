/**
 * Organic Learning Demo
 *
 * Demonstrates the organic learning capabilities of Evocore 1.0.0:
 * - Exploration Control: Balancing exploration vs exploitation
 * - Parameter Synthesis: Combining knowledge from multiple sources
 *
 * Use case: Trading strategy optimization across multiple cryptocurrencies
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

/*========================================================================
 * Simulation Setup
 *========================================================================*/

#define NUM_CONTEXTS 4
#define NUM_PARAMS 5

/* Context identifiers for different cryptocurrencies */
static const char* CONTEXTS[NUM_CONTEXTS] = {
    "BTC",  /* Bitcoin - high volume, low volatility */
    "ETH",  /* Ethereum - medium volume, medium volatility */
    "SOL",  /* Solana - lower volume, high volatility */
    "DOGE"  /* Dogecoin - speculative, very high volatility */
};

/* Optimal parameters for each context (simulated ground truth) */
static const double OPTIMAL_PARAMS[NUM_CONTEXTS][NUM_PARAMS] = {
    /* BTC: Conservative settings */
    {0.02, 0.5, 100.0, 0.8, 0.1},
    /* ETH: Moderate settings */
    {0.03, 0.6, 80.0, 0.7, 0.15},
    /* SOL: Aggressive settings */
    {0.05, 0.7, 50.0, 0.6, 0.2},
    /* DOGE: Very aggressive settings */
    {0.08, 0.8, 30.0, 0.5, 0.25}
};

/*========================================================================
 * Fitness Function
 *========================================================================*/

/**
 * Simulated fitness function for trading strategy
 * Higher fitness = better strategy performance
 */
double simulate_trading_fitness(const double *params, int ctx_idx) {
    /* Calculate distance from optimal (inverse of distance = fitness) */
    double distance_sq = 0.0;
    for (int i = 0; i < NUM_PARAMS; i++) {
        double diff = params[i] - OPTIMAL_PARAMS[ctx_idx][i];
        distance_sq += diff * diff;
    }

    /* Convert to fitness: 1.0 at optimal, decreasing with distance */
    return exp(-sqrt(distance_sq) * 2.0);
}

/*========================================================================
 * Demo Sections
 *========================================================================*/

void demo_exploration_strategies(void) {
    printf("\n=== Exploration Control Demo ===\n");
    printf("Comparing exploration strategies for multi-armed bandit problem\n\n");

    /* Simulate a 3-armed bandit (different parameter settings) */
    double arm_rewards[3] = {0.3, 0.5, 0.7};  /* True reward probabilities */
    const int ROUNDS = 100;
    unsigned int seed = (unsigned int)time(NULL);

    /* Test Fixed strategy */
    printf("Fixed Strategy (epsilon=0.2):\n");
    evocore_exploration_t *fixed = evocore_exploration_create(
        EVOCORE_EXPLORE_FIXED, 0.2
    );
    int fixed_counts[3] = {0};
    double fixed_total = 0.0;
    double best_fitness = 0.0;

    for (int r = 0; r < ROUNDS; r++) {
        int exploit = !evocore_exploration_should_explore(fixed, &seed);
        int arm = exploit ? 2 : (rand() % 3);  /* Exploit best arm, or random */

        double reward = ((double)rand() / RAND_MAX) < arm_rewards[arm] ? 1.0 : 0.0;
        fixed_counts[arm]++;
        fixed_total += reward;

        /* Update exploration state */
        if (reward > best_fitness) best_fitness = reward;
        evocore_exploration_update(fixed, r, best_fitness);
    }
    printf("  Arm selections: [%d, %d, %d]\n", fixed_counts[0], fixed_counts[1], fixed_counts[2]);
    printf("  Total reward: %.1f / %d (%.1f%%)\n\n",
           fixed_total, ROUNDS, fixed_total / ROUNDS * 100);

    evocore_exploration_free(fixed);

    /* Test UCB1 bandit */
    printf("UCB1 Bandit (c=2.0):\n");
    evocore_bandit_t *ucb1 = evocore_bandit_create(3, 2.0);
    int ucb1_counts[3] = {0};
    double ucb1_total = 0.0;

    for (int r = 0; r < ROUNDS; r++) {
        size_t arm = evocore_bandit_select_ucb(ucb1);
        double reward = ((double)rand() / RAND_MAX) < arm_rewards[arm] ? 1.0 : 0.0;
        evocore_bandit_update(ucb1, arm, reward);
        ucb1_counts[(int)arm]++;
        ucb1_total += reward;
    }
    printf("  Arm selections: [%d, %d, %d]\n", ucb1_counts[0], ucb1_counts[1], ucb1_counts[2]);
    printf("  Total reward: %.1f / %d (%.1f%%)\n\n",
           ucb1_total, ROUNDS, ucb1_total / ROUNDS * 100);

    evocore_bandit_free(ucb1);

    /* Test Boltzmann strategy */
    printf("Boltzmann Strategy (temperature=1.0):\n");
    evocore_exploration_t *boltz = evocore_exploration_create(
        EVOCORE_EXPLORE_BOLTZMANN, 0.5
    );
    evocore_exploration_set_temperature(boltz, 1.0, 0.99);

    int boltz_counts[3] = {0};
    double boltz_total = 0.0;
    best_fitness = 0.0;

    for (int r = 0; r < ROUNDS; r++) {
        /* Use boltzmann selection on arm values */
        double temperature = boltz->temperature;
        size_t arm = evocore_boltzmann_select(arm_rewards, 3, temperature, &seed);

        double reward = ((double)rand() / RAND_MAX) < arm_rewards[arm] ? 1.0 : 0.0;
        boltz_counts[(int)arm]++;
        boltz_total += reward;

        if (reward > best_fitness) best_fitness = reward;
        evocore_exploration_update(boltz, r, best_fitness);
    }
    printf("  Arm selections: [%d, %d, %d]\n", boltz_counts[0], boltz_counts[1], boltz_counts[2]);
    printf("  Total reward: %.1f / %d (%.1f%%)\n",
           boltz_total, ROUNDS, boltz_total / ROUNDS * 100);
    printf("  Final temperature: %.3f\n\n", boltz->temperature);

    evocore_exploration_free(boltz);

    /* Test Adaptive strategy */
    printf("Adaptive Strategy (performance-based):\n");
    evocore_exploration_t *adaptive = evocore_exploration_create(
        EVOCORE_EXPLORE_ADAPTIVE, 0.5
    );
    int adaptive_counts[3] = {0};
    double adaptive_total = 0.0;
    best_fitness = 0.0;

    for (int r = 0; r < ROUNDS; r++) {
        int exploit = !evocore_exploration_should_explore(adaptive, &seed);
        int arm = exploit ? 2 : (rand() % 3);

        double reward = ((double)rand() / RAND_MAX) < arm_rewards[arm] ? 1.0 : 0.0;
        adaptive_counts[arm]++;
        adaptive_total += reward;

        if (reward > best_fitness) best_fitness = reward;
        evocore_exploration_update(adaptive, r, best_fitness);
    }
    printf("  Arm selections: [%d, %d, %d]\n", adaptive_counts[0], adaptive_counts[1], adaptive_counts[2]);
    printf("  Final exploration rate: %.3f\n", adaptive->current_rate);
    printf("  Total reward: %.1f / %d (%.1f%%)\n\n",
           adaptive_total, ROUNDS, adaptive_total / ROUNDS * 100);

    evocore_exploration_free(adaptive);

    /* Test Decay strategy */
    printf("Decay Strategy (starts at 0.5, decays to 0.05):\n");
    evocore_exploration_t *decay = evocore_exploration_create(
        EVOCORE_EXPLORE_DECAY, 0.5
    );
    evocore_exploration_set_bounds(decay, 0.05, 0.5);
    evocore_exploration_set_decay_rate(decay, 0.02);

    int decay_counts[3] = {0};
    double decay_total = 0.0;
    best_fitness = 0.0;

    for (int r = 0; r < ROUNDS; r++) {
        int exploit = !evocore_exploration_should_explore(decay, &seed);
        int arm = exploit ? 2 : (rand() % 3);

        double reward = ((double)rand() / RAND_MAX) < arm_rewards[arm] ? 1.0 : 0.0;
        decay_counts[arm]++;
        decay_total += reward;

        if (reward > best_fitness) best_fitness = reward;
        double rate = evocore_exploration_update(decay, r, best_fitness);
        if (r % 20 == 0) {
            printf("  Round %d: exploration rate = %.3f\n", r, rate);
        }
    }
    printf("  Final exploration rate: %.3f\n", decay->current_rate);
    printf("  Total reward: %.1f / %d (%.1f%%)\n\n",
           decay_total, ROUNDS, decay_total / ROUNDS * 100);

    evocore_exploration_free(decay);
}

void demo_parameter_synthesis(void) {
    printf("\n=== Parameter Synthesis Demo ===\n");
    printf("Combining knowledge from multiple sources\n\n");

    /* Create synthesis request */
    evocore_synthesis_request_t *req = evocore_synthesis_request_create(
        EVOCORE_SYNTHESIS_ENSEMBLE, NUM_PARAMS, NUM_CONTEXTS
    );

    /* Add sources from each context */
    for (int c = 0; c < NUM_CONTEXTS; c++) {
        double fitness = 0.6 + (double)c / NUM_PARAMS;  /* Varying fitness */
        double confidence = 0.7 + (double)(NUM_CONTEXTS - c) / 20.0;

        evocore_synthesis_add_source(req, (size_t)c, OPTIMAL_PARAMS[c],
                                    confidence, fitness, CONTEXTS[c]);
        printf("Added source %s: fitness=%.2f, confidence=%.2f\n",
               CONTEXTS[c], fitness, confidence);
    }
    printf("\n");

    /* Test different synthesis strategies */
    const char *strategy_names[] = {
        "Average", "Weighted", "Trend", "Regime", "Ensemble"
    };

    for (int s = EVOCORE_SYNTHESIS_AVERAGE; s <= EVOCORE_SYNTHESIS_ENSEMBLE; s++) {
        req->strategy = (evocore_synthesis_strategy_t)s;
        req->exploration_factor = 0.0;  /* Deterministic for demo */

        double result[NUM_PARAMS];
        double confidence;
        unsigned int seed = 42;

        if (evocore_synthesis_execute(req, result, &confidence, &seed)) {
            printf("%s Strategy:\n", strategy_names[s - EVOCORE_SYNTHESIS_AVERAGE]);
            printf("  Result:     [%.2f, %.2f, %.2f, %.2f, %.2f]\n",
                   result[0], result[1], result[2], result[3], result[4]);
            printf("  Confidence: %.3f\n", confidence);

            /* Evaluate fitness on each context */
            printf("  Cross-context fitness:\n");
            for (int c = 0; c < NUM_CONTEXTS; c++) {
                double fit = simulate_trading_fitness(result, c);
                printf("    %s: %.3f", CONTEXTS[c], fit);
                if (fit > 0.7) printf(" *");
                printf("\n");
            }
            printf("\n");
        }
    }

    /* Demonstrate with exploration */
    printf("With 20%% exploration (adding controlled randomness):\n");
    req->strategy = EVOCORE_SYNTHESIS_AVERAGE;
    req->exploration_factor = 0.2;

    for (int i = 0; i < 3; i++) {
        double result[NUM_PARAMS];
        unsigned int seed = (unsigned int)time(NULL) + i;
        evocore_synthesis_execute(req, result, NULL, &seed);
        printf("  Sample %d: [%.2f, %.2f, %.2f, ...]\n",
               i + 1, result[0], result[1], result[2]);
    }

    evocore_synthesis_request_free(req);
}

void demo_parameter_distance(void) {
    printf("\n=== Parameter Distance & Similarity Demo ===\n");
    printf("Measuring similarity between parameter vectors\n\n");

    /* Calculate distances between all contexts */
    printf("Pairwise parameter distances (Euclidean):\n");
    for (int i = 0; i < NUM_CONTEXTS; i++) {
        for (int j = i + 1; j < NUM_CONTEXTS; j++) {
            double dist = evocore_param_distance(
                OPTIMAL_PARAMS[i], OPTIMAL_PARAMS[j], NUM_PARAMS
            );
            double sim = evocore_param_similarity(
                OPTIMAL_PARAMS[i], OPTIMAL_PARAMS[j], NUM_PARAMS, 1.0
            );
            printf("  %s <-> %s: distance=%.3f, similarity=%.3f\n",
                   CONTEXTS[i], CONTEXTS[j], dist, sim);
        }
    }
    printf("\n");

    /* Create similarity matrix */
    printf("Context Similarity Matrix:\n");
    printf("         ");
    for (int j = 0; j < NUM_CONTEXTS; j++) {
        printf("%6s", CONTEXTS[j]);
    }
    printf("\n");

    for (int i = 0; i < NUM_CONTEXTS; i++) {
        printf("%6s  ", CONTEXTS[i]);
        for (int j = 0; j < NUM_CONTEXTS; j++) {
            if (i == j) {
                printf(" 1.000");
            } else {
                double sim = evocore_param_similarity(
                    OPTIMAL_PARAMS[i], OPTIMAL_PARAMS[j], NUM_PARAMS, 1.0
                );
                printf(" %5.3f", sim);
            }
        }
        printf("\n");
    }
    printf("\n");
}

void demo_integrated_workflow(void) {
    printf("\n=== Integrated Organic Learning Workflow ===\n");
    printf("Combining exploration and synthesis for adaptive optimization\n\n");

    const int GENERATIONS = 50;
    const int POPULATION_SIZE = 10;
    unsigned int seed = (unsigned int)time(NULL);

    /* Track best parameters per context */
    double best_params[NUM_CONTEXTS][NUM_PARAMS];
    double best_fitness[NUM_CONTEXTS] = {0};

    /* Initialize with random parameters */
    for (int c = 0; c < NUM_CONTEXTS; c++) {
        for (int p = 0; p < NUM_PARAMS; p++) {
            best_params[c][p] = (double)rand() / RAND_MAX;
        }
        best_fitness[c] = simulate_trading_fitness(best_params[c], c);
    }

    /* Create exploration controller */
    evocore_exploration_t *expl = evocore_exploration_create(
        EVOCORE_EXPLORE_ADAPTIVE, 0.3
    );

    printf("Evolution over %d generations:\n", GENERATIONS);

    for (int gen = 0; gen < GENERATIONS; gen++) {
        double gen_best = 0.0;

        for (int ctx = 0; ctx < NUM_CONTEXTS; ctx++) {
            /* Evaluate population */
            for (int ind = 0; ind < POPULATION_SIZE; ind++) {
                double params[NUM_PARAMS];
                double fitness;

                if (evocore_exploration_should_explore(expl, &seed)) {
                    /* Explore: mutate best parameters */
                    for (int p = 0; p < NUM_PARAMS; p++) {
                        params[p] = best_params[ctx][p] +
                                   ((double)rand() / RAND_MAX - 0.5) * 0.2;
                        params[p] = fmax(0.0, fmin(1.0, params[p]));
                    }
                } else {
                    /* Exploit: small refinement */
                    for (int p = 0; p < NUM_PARAMS; p++) {
                        params[p] = best_params[ctx][p] +
                                   ((double)rand() / RAND_MAX - 0.5) * 0.05;
                        params[p] = fmax(0.0, fmin(1.0, params[p]));
                    }
                }

                fitness = simulate_trading_fitness(params, ctx);

                if (fitness > best_fitness[ctx]) {
                    best_fitness[ctx] = fitness;
                    memcpy(best_params[ctx], params, sizeof(params));
                }

                if (fitness > gen_best) gen_best = fitness;
            }
        }

        /* Update exploration rate */
        evocore_exploration_update(expl, gen, gen_best);

        if (gen % 10 == 9 || gen == GENERATIONS - 1) {
            printf("  Gen %2d: exploration=%.3f, best_fitness=%.3f\n",
                   gen + 1, expl->current_rate, gen_best);
        }
    }

    printf("\nFinal Results:\n");
    for (int c = 0; c < NUM_CONTEXTS; c++) {
        printf("  %s: fitness=%.3f, params=[%.2f, %.2f, ...]\n",
               CONTEXTS[c], best_fitness[c],
               best_params[c][0], best_params[c][1]);
    }

    /* Synthesize cross-context strategy */
    printf("\nSynthesizing cross-context strategy...\n");

    evocore_synthesis_request_t *req = evocore_synthesis_request_create(
        EVOCORE_SYNTHESIS_WEIGHTED, NUM_PARAMS, NUM_CONTEXTS
    );

    for (int c = 0; c < NUM_CONTEXTS; c++) {
        double confidence = best_fitness[c];
        evocore_synthesis_add_source(req, (size_t)c, best_params[c],
                                    confidence, best_fitness[c], CONTEXTS[c]);
    }

    double synthesized[NUM_PARAMS];
    double synth_conf;

    if (evocore_synthesis_execute(req, synthesized, &synth_conf, &seed)) {
        printf("  Synthesized parameters: [%.2f, %.2f, %.2f, %.2f, %.2f]\n",
               synthesized[0], synthesized[1], synthesized[2],
               synthesized[3], synthesized[4]);
        printf("  Synthesis confidence: %.3f\n", synth_conf);

        printf("  Cross-context performance:\n");
        for (int c = 0; c < NUM_CONTEXTS; c++) {
            double fit = simulate_trading_fitness(synthesized, c);
            printf("    %s: %.3f (vs best %.3f)\n", CONTEXTS[c], fit, best_fitness[c]);
        }
    }

    printf("\nOrganic Learning Summary:\n");
    printf("  Generations: %d\n", GENERATIONS);
    printf("  Population per context: %d\n", POPULATION_SIZE);
    printf("  Final exploration rate: %.3f\n", expl->current_rate);
    printf("  Synthesis confidence: %.3f\n", synth_conf);

    /* Cleanup */
    evocore_synthesis_request_free(req);
    evocore_exploration_free(expl);
}

void demo_temporal_synthesis(void) {
    printf("\n=== Temporal Synthesis Demo ===\n");
    printf("Synthesizing parameters that evolve over time\n\n");

    /* Simulate parameters changing over time */
    const int TIME_STEPS = 10;
    double sources[TIME_STEPS][NUM_PARAMS];

    printf("Simulating parameter evolution over %d time steps:\n", TIME_STEPS);

    for (int t = 0; t < TIME_STEPS; t++) {
        /* Parameters drift upward over time */
        double trend = (double)t / TIME_STEPS;
        for (int p = 0; p < NUM_PARAMS; p++) {
            sources[t][p] = 0.3 + trend * 0.4 + ((double)rand() / RAND_MAX - 0.5) * 0.1;
        }
        printf("  t=%d: [%.2f, %.2f, %.2f, ...]\n",
               t, sources[t][0], sources[t][1], sources[t][2]);
    }

    /* Use trend synthesis to project forward */
    evocore_synthesis_request_t *req = evocore_synthesis_request_create(
        EVOCORE_SYNTHESIS_TREND, NUM_PARAMS, TIME_STEPS
    );

    for (int t = 0; t < TIME_STEPS; t++) {
        char time_label[32];
        snprintf(time_label, sizeof(time_label), "t%d", t);
        evocore_synthesis_add_source(req, (size_t)t, sources[t],
                                    1.0, 0.5, time_label);
    }

    req->trend_strength = 1.0;  /* Full trend projection */
    req->exploration_factor = 0.0;

    double projected[NUM_PARAMS];
    double confidence;
    unsigned int seed_val = 42;

    if (evocore_synthesis_execute(req, projected, &confidence, &seed_val)) {
        printf("\nTrend projection (trend_strength=1.0):\n");
        printf("  Projected: [%.2f, %.2f, %.2f, %.2f, %.2f]\n",
               projected[0], projected[1], projected[2], projected[3], projected[4]);
        printf("  Confidence: %.3f\n", confidence);
        printf("  (Continues the upward trend beyond last sample)\n");
    }

    evocore_synthesis_request_free(req);
}

/*========================================================================
 * Main
 *========================================================================*/

int main(void) {
    printf("========================================\n");
    printf("  Evocore %s Organic Learning Demo\n", EVOCORE_VERSION_STRING);
    printf("========================================\n");
    printf("\nThis demo showcases the organic learning capabilities:\n");
    printf("- Exploration Control: Adaptive exploration strategies\n");
    printf("- Parameter Synthesis: Cross-context knowledge transfer\n");
    printf("- Parameter Distance: Measuring parameter similarity\n");
    printf("- Temporal Synthesis: Projecting parameter trends\n");

    srand((unsigned int)time(NULL));
    evocore_log_set_level(EVOCORE_LOG_WARN);

    /* Run all demos */
    demo_exploration_strategies();
    demo_parameter_synthesis();
    demo_parameter_distance();
    demo_integrated_workflow();
    demo_temporal_synthesis();

    printf("\n========================================\n");
    printf("  Demo Complete\n");
    printf("========================================\n");

    return 0;
}
