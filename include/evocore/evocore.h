#ifndef EVOCORE_H
#define EVOCORE_H

/**
 * evocore - Meta-Evolutionary Framework
 *
 * A domain-agnostic framework for evolutionary and meta-evolutionary
 * optimization.
 */

/* Version information */
#define EVOCORE_VERSION_MAJOR 0
#define EVOCORE_VERSION_MINOR 1
#define EVOCORE_VERSION_PATCH 0
#define EVOCORE_VERSION_STRING "0.1.0"

/* Core abstractions */
#include "evocore/genome.h"
#include "evocore/fitness.h"
#include "evocore/population.h"
#include "evocore/domain.h"
#include "evocore/meta.h"

/* Acceleration */
#include "evocore/gpu.h"
#include "evocore/optimize.h"

/* Infrastructure */
#include "evocore/config.h"
#include "evocore/error.h"
#include "evocore/log.h"
#include "evocore/persist.h"
#include "evocore/stats.h"

#endif /* EVOCORE_H */
