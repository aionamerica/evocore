#ifndef EVOCORE_FITNESS_H
#define EVOCORE_FITNESS_H

#include "evocore/genome.h"

/**
 * Fitness evaluation callback
 *
 * Domains implement this to evaluate how good a genome is.
 * Higher fitness = better. Can return NaN to indicate invalid.
 *
 * @param genome    Genome to evaluate
 * @param context   User-provided context pointer
 * @return Fitness value (higher is better, or NaN for invalid)
 */
typedef double (*evocore_fitness_func_t)(const evocore_genome_t *genome,
                                       void *context);

#endif /* EVOCORE_FITNESS_H */
