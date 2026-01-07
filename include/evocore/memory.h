#ifndef EVOCORE_MEMORY_H
#define EVOCORE_MEMORY_H

/**
 * Unified Memory API
 *
 * Provides memory management utilities including:
 * - Arena allocators for bulk allocation
 * - Memory pools for reusable fixed-size blocks
 * - Memory tracking and debugging
 */

#include "evocore/arena.h"

/*========================================================================
 * Memory Configuration
 *========================================================================*/

/**
 * Memory statistics
 */
typedef struct {
    size_t total_allocated;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
} evocore_memory_stats_t;

/**
 * Memory tracking configuration
 */
typedef struct {
    bool enable_tracking;
    bool enable_leak_detection;
    size_t allocation_limit;
    const char *label;
} evocore_memory_config_t;

/*========================================================================
 * Memory Management Functions
 *========================================================================*/

/**
 * Get global memory statistics
 *
 * @param stats    Output statistics structure
 */
void evocore_memory_get_stats(evocore_memory_stats_t *stats);

/**
 * Reset memory statistics
 */
void evocore_memory_reset_stats(void);

/**
 * Enable memory tracking
 *
 * @param enable   true to enable, false to disable
 */
void evocore_memory_set_tracking(bool enable);

/**
 * Check for memory leaks
 *
 * @return Number of leaked allocations, or 0 if no leaks
 */
size_t evocore_memory_check_leaks(void);

/**
 * Dump memory statistics to log
 */
void evocore_memory_dump_stats(void);

/*========================================================================
 * Convenience Macros
 *========================================================================*/

/**
 * Scoped arena - automatically cleans up when leaving scope
 */
#define EVOCORE_SCOPED_ARENA(name, capacity) \
    evocore_arena_t name; \
    evocore_arena_init(&name, capacity); \
    __attribute__((cleanup(evocore_arena_cleanup))) evocore_arena_t* name##_cleanup_ptr = &name

#endif /* EVOCORE_MEMORY_H */
