/**
 * Trading Domain Example for evocore
 *
 * This is a stub trading domain demonstrating how to use the domain
 * registration system for a trading strategy optimization problem.
 *
 * In a real trading system, this would integrate with backtesting
 * infrastructure to evaluate trading strategies.
 */

#define _GNU_SOURCE
#include "evocore/evocore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

/*========================================================================
 * Trading Genome Structure
 * ======================================================================
 *
 * The trading genome encodes strategy parameters:
 * - Entry threshold (double)
 * - Exit threshold (double)
 * - Stop loss percentage (double)
 * - Take profit percentage (double)
 * - Position size (double)
 * - Strategy flags (uint8_t)
 */

typedef struct {
    double entry_threshold;
    double exit_threshold;
    double stop_loss_pct;
    double take_profit_pct;
    double position_size;
    uint8_t flags;  /* Bit field for strategy options */
} trading_params_t;

/*========================================================================
 * Trading Context
 * ======================================================================*/

typedef struct {
    int num_candles;
    double *prices;
    double initial_capital;
} trading_context_t;

/*========================================================================
 * Domain Callbacks
 * ======================================================================*/

static void trading_random_init(evocore_genome_t *genome, void *context) {
    (void)context;

    trading_params_t params = {
        .entry_threshold = 0.01 + ((double)rand() / RAND_MAX) * 0.1,
        .exit_threshold = 0.005 + ((double)rand() / RAND_MAX) * 0.05,
        .stop_loss_pct = 0.01 + ((double)rand() / RAND_MAX) * 0.1,
        .take_profit_pct = 0.02 + ((double)rand() / RAND_MAX) * 0.2,
        .position_size = 0.1 + ((double)rand() / RAND_MAX) * 0.9,
        .flags = (uint8_t)(rand() % 256),
    };

    evocore_genome_write(genome, 0, &params, sizeof(params));
    evocore_genome_set_size(genome, sizeof(params));
}

static void trading_mutate(evocore_genome_t *genome, double rate, void *context) {
    (void)context;

    trading_params_t params;
    evocore_genome_read(genome, 0, &params, sizeof(params));

    /* Mutate each parameter with probability rate */
    if ((double)rand() / RAND_MAX < rate) {
        params.entry_threshold *= (0.9 + ((double)rand() / RAND_MAX) * 0.2);
        params.entry_threshold = fmax(0.001, fmin(0.5, params.entry_threshold));
    }

    if ((double)rand() / RAND_MAX < rate) {
        params.exit_threshold *= (0.9 + ((double)rand() / RAND_MAX) * 0.2);
        params.exit_threshold = fmax(0.001, fmin(0.3, params.exit_threshold));
    }

    if ((double)rand() / RAND_MAX < rate) {
        params.stop_loss_pct *= (0.9 + ((double)rand() / RAND_MAX) * 0.2);
        params.stop_loss_pct = fmax(0.005, fmin(0.2, params.stop_loss_pct));
    }

    if ((double)rand() / RAND_MAX < rate) {
        params.take_profit_pct *= (0.9 + ((double)rand() / RAND_MAX) * 0.2);
        params.take_profit_pct = fmax(0.01, fmin(0.5, params.take_profit_pct));
    }

    if ((double)rand() / RAND_MAX < rate) {
        params.position_size *= (0.9 + ((double)rand() / RAND_MAX) * 0.2);
        params.position_size = fmax(0.01, fmin(1.0, params.position_size));
    }

    if ((double)rand() / RAND_MAX < rate * 0.5) {
        params.flags ^= (1 << (rand() % 8));
    }

    evocore_genome_write(genome, 0, &params, sizeof(params));
}

static void trading_crossover(const evocore_genome_t *parent1,
                              const evocore_genome_t *parent2,
                              evocore_genome_t *child1,
                              evocore_genome_t *child2,
                              void *context) {
    (void)context;

    trading_params_t p1, p2;
    evocore_genome_read(parent1, 0, &p1, sizeof(p1));
    evocore_genome_read(parent2, 0, &p2, sizeof(p2));

    trading_params_t c1, c2;

    /* Uniform crossover - take each parameter from either parent */
    c1.entry_threshold = (rand() % 2) ? p1.entry_threshold : p2.entry_threshold;
    c1.exit_threshold = (rand() % 2) ? p1.exit_threshold : p2.exit_threshold;
    c1.stop_loss_pct = (rand() % 2) ? p1.stop_loss_pct : p2.stop_loss_pct;
    c1.take_profit_pct = (rand() % 2) ? p1.take_profit_pct : p2.take_profit_pct;
    c1.position_size = (rand() % 2) ? p1.position_size : p2.position_size;
    c1.flags = (rand() % 2) ? p1.flags : p2.flags;

    /* Child 2 gets the opposite parameters */
    c2.entry_threshold = (c1.entry_threshold == p1.entry_threshold) ?
        p2.entry_threshold : p1.entry_threshold;
    c2.exit_threshold = (c1.exit_threshold == p1.exit_threshold) ?
        p2.exit_threshold : p1.exit_threshold;
    c2.stop_loss_pct = (c1.stop_loss_pct == p1.stop_loss_pct) ?
        p2.stop_loss_pct : p1.stop_loss_pct;
    c2.take_profit_pct = (c1.take_profit_pct == p1.take_profit_pct) ?
        p2.take_profit_pct : p1.take_profit_pct;
    c2.position_size = (c1.position_size == p1.position_size) ?
        p2.position_size : p1.position_size;
    c2.flags = (c1.flags == p1.flags) ? p2.flags : p1.flags;

    evocore_genome_write(child1, 0, &c1, sizeof(c1));
    evocore_genome_set_size(child1, sizeof(c1));
    evocore_genome_write(child2, 0, &c2, sizeof(c2));
    evocore_genome_set_size(child2, sizeof(c2));
}

static double trading_diversity(const evocore_genome_t *a,
                                const evocore_genome_t *b,
                                void *context) {
    (void)context;

    trading_params_t pa, pb;
    evocore_genome_read(a, 0, &pa, sizeof(pa));
    evocore_genome_read(b, 0, &pb, sizeof(pb));

    /* Calculate normalized difference across parameters */
    double diff = 0.0;
    diff += fabs(pa.entry_threshold - pb.entry_threshold) / 0.5;
    diff += fabs(pa.exit_threshold - pb.exit_threshold) / 0.3;
    diff += fabs(pa.stop_loss_pct - pb.stop_loss_pct) / 0.2;
    diff += fabs(pa.take_profit_pct - pb.take_profit_pct) / 0.5;
    diff += fabs(pa.position_size - pb.position_size);
    diff += (pa.flags != pb.flags) ? 1.0 : 0.0;

    return diff / 6.0;  /* Normalize to 0-1 range */
}

static double trading_fitness(const evocore_genome_t *genome, void *context) {
    trading_context_t *trading_ctx = (trading_context_t*)context;
    trading_params_t params;

    evocore_genome_read(genome, 0, &params, sizeof(params));

    /* Stub fitness calculation */
    /* In a real implementation, this would run a backtest */

    double score = 0.0;

    /* Favor moderate entry thresholds */
    double entry_score = 1.0 - fabs(params.entry_threshold - 0.03);
    score += fmax(0.0, entry_score) * 20.0;

    /* Favor tight exit thresholds */
    double exit_score = 1.0 - fabs(params.exit_threshold - 0.02);
    score += fmax(0.0, exit_score) * 15.0;

    /* Favor reasonable stop loss */
    double sl_score = 1.0 - fabs(params.stop_loss_pct - 0.02);
    score += fmax(0.0, sl_score) * 15.0;

    /* Favor reasonable take profit (2:1 reward:risk ratio) */
    double tp_score = 1.0 - fabs(params.take_profit_pct - 0.04);
    score += fmax(0.0, tp_score) * 20.0;

    /* Favor conservative position sizing */
    double size_score = 1.0 - fabs(params.position_size - 0.25);
    score += fmax(0.0, size_score) * 10.0;

    /* Penalize extreme flag combinations */
    if (params.flags == 0 || params.flags == 255) {
        score -= 10.0;
    }

    return score;
}

static int trading_serialize_genome(const evocore_genome_t *genome,
                                    char *buffer,
                                    size_t size,
                                    void *context) {
    (void)context;
    trading_params_t params;
    evocore_genome_read(genome, 0, &params, sizeof(params));

    return snprintf(buffer, size,
        "entry=%.4f exit=%.4f sl=%.2f%% tp=%.2f%% size=%.2f flags=0x%02x",
        params.entry_threshold,
        params.exit_threshold,
        params.stop_loss_pct * 100.0,
        params.take_profit_pct * 100.0,
        params.position_size,
        params.flags);
}

/*========================================================================
 * Domain Registration
 * ======================================================================*/

int main(void) {
    printf("=== Trading Domain Example ===\n\n");

    /* Initialize systems */
    evocore_domain_registry_init();
    evocore_log_set_level(EVOCORE_LOG_INFO);

    /* Create trading context (stub) */
    trading_context_t trading_ctx = {
        .num_candles = 1000,
        .prices = NULL,
        .initial_capital = 10000.0,
    };

    /* Register the trading domain */
    evocore_domain_t trading_domain = {
        .name = "trading",
        .version = "1.0.0",
        .genome_size = sizeof(trading_params_t),
        .genome_ops = {
            .random_init = trading_random_init,
            .mutate = trading_mutate,
            .crossover = trading_crossover,
            .diversity = trading_diversity,
        },
        .fitness = trading_fitness,
        .user_context = &trading_ctx,
        .serialize_genome = trading_serialize_genome,
    };

    evocore_error_t err = evocore_register_domain(&trading_domain);
    if (err != EVOCORE_OK) {
        printf("Failed to register trading domain: %d\n", err);
        return 1;
    }

    /* Demonstrate creating trading genomes */
    printf("Creating sample trading genomes:\n\n");

    evocore_genome_t genomes[5];
    char buffer[256];

    for (int i = 0; i < 5; i++) {
        evocore_domain_create_genome("trading", &genomes[i]);

        trading_serialize_genome(&genomes[i], buffer, sizeof(buffer), NULL);
        double fitness = evocore_domain_evaluate_fitness(&genomes[i], &trading_domain);

        printf("  [%d] %s\n", i + 1, buffer);
        printf("       Fitness: %.2f\n", fitness);

        trading_params_t params;
        evocore_genome_read(&genomes[i], 0, &params, sizeof(params));

        /* Show mutation */
        evocore_domain_mutate_genome(&genomes[i], &trading_domain, 0.3);
        evocore_genome_read(&genomes[i], 0, &params, sizeof(params));

        trading_serialize_genome(&genomes[i], buffer, sizeof(buffer), NULL);
        printf("       After mutation: %s\n", buffer);
        printf("\n");
    }

    /* Show diversity */
    printf("Diversity matrix:\n");
    printf("     ");
    for (int i = 0; i < 5; i++) printf(" [%d]  ", i);
    printf("\n");

    for (int i = 0; i < 5; i++) {
        printf("[%d] ", i);
        for (int j = 0; j < 5; j++) {
            double div = evocore_domain_diversity(&genomes[i], &genomes[j],
                                                &trading_domain);
            printf(" %.2f ", div);
        }
        printf("\n");
    }

    /* Cleanup */
    for (int i = 0; i < 5; i++) {
        evocore_genome_cleanup(&genomes[i]);
    }
    evocore_domain_registry_shutdown();

    printf("\n=== Example Complete ===\n");
    return 0;
}
