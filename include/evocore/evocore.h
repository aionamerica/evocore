#ifndef EVOCORE_H
#define EVOCORE_H

/**
 * evocore - Meta-Evolutionary Framework
 *
 * A domain-agnostic framework for evolutionary and meta-evolutionary
 * optimization.
 */

/* Version information */
#define EVOCORE_VERSION_MAJOR 1
#define EVOCORE_VERSION_MINOR 0
#define EVOCORE_VERSION_PATCH 0
#define EVOCORE_VERSION_STRING "1.0.0"

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

/* Memory & Statistics */
#include "evocore/arena.h"
#include "evocore/memory.h"
#include "evocore/weighted.h"
#include "evocore/context.h"   // Includes negative.h
#include "evocore/temporal.h"
#include "evocore/exploration.h"
#include "evocore/synthesis.h"

#endif /* EVOCORE_H */
