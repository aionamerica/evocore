/**
 * Unified Memory API Implementation
 */

#define _GNU_SOURCE
#include "evocore/memory.h"
#include "evocore/log.h"
#include <stdio.h>
#include <string.h>

/*========================================================================
 * Memory Tracking State
 *========================================================================*/

static struct {
    bool enabled;
    size_t total_allocated;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
} g_memory_tracking = {
    .enabled = false,
    .total_allocated = 0,
    .current_allocated = 0,
    .peak_allocated = 0,
    .allocation_count = 0,
    .free_count = 0
};

/*========================================================================
 * Memory Management Functions
 *========================================================================*/

void evocore_memory_get_stats(evocore_memory_stats_t *stats) {
    if (!stats) {
        return;
    }

    stats->total_allocated = g_memory_tracking.total_allocated;
    stats->current_allocated = g_memory_tracking.current_allocated;
    stats->peak_allocated = g_memory_tracking.peak_allocated;
    stats->allocation_count = g_memory_tracking.allocation_count;
    stats->free_count = g_memory_tracking.free_count;
}

void evocore_memory_reset_stats(void) {
    g_memory_tracking.total_allocated = 0;
    g_memory_tracking.current_allocated = 0;
    g_memory_tracking.peak_allocated = 0;
    g_memory_tracking.allocation_count = 0;
    g_memory_tracking.free_count = 0;
}

void evocore_memory_set_tracking(bool enable) {
    g_memory_tracking.enabled = enable;
}

size_t evocore_memory_check_leaks(void) {
    /* Current allocations that haven't been freed */
    if (g_memory_tracking.enabled) {
        size_t leaked = g_memory_tracking.allocation_count - g_memory_tracking.free_count;
        return leaked;
    }
    return 0;
}

void evocore_memory_dump_stats(void) {
    evocore_log_info("=== Memory Statistics ===");
    evocore_log_info("Tracking: %s", g_memory_tracking.enabled ? "enabled" : "disabled");
    evocore_log_info("Total Allocated: %zu bytes", g_memory_tracking.total_allocated);
    evocore_log_info("Current Allocated: %zu bytes", g_memory_tracking.current_allocated);
    evocore_log_info("Peak Allocated: %zu bytes", g_memory_tracking.peak_allocated);
    evocore_log_info("Allocations: %zu", g_memory_tracking.allocation_count);
    evocore_log_info("Frees: %zu", g_memory_tracking.free_count);

    if (g_memory_tracking.enabled && g_memory_tracking.allocation_count > g_memory_tracking.free_count) {
        evocore_log_warn("Potential leaks: %zu allocations",
                        g_memory_tracking.allocation_count - g_memory_tracking.free_count);
    }
}
