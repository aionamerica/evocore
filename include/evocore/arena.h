#ifndef EVOCORE_ARENA_H
#define EVOCORE_ARENA_H

#include "evocore/error.h"
#include <stddef.h>
#include <stdbool.h>

/*========================================================================
 * Arena Allocator
 * ========================================================================
 *
 * Arena allocator for efficient bulk memory allocation.
 * All allocations can be freed at once by resetting the arena.
 *
 * Use cases:
 * - Bulk genome allocation for new generations
 * - Temporary scratch memory during evaluation
 * - Per-generation allocations that can be bulk-freed
 */

/**
 * Arena alignment for allocations
 */
#define EVOCORE_ARENA_ALIGNMENT (2 * sizeof(void*))

/**
 * Default arena capacity
 */
#define EVOCORE_ARENA_DEFAULT_CAPACITY (1024 * 1024)  /* 1 MB */

/**
 * Arena allocator
 *
 * Manages a contiguous block of memory for fast allocations.
 * All allocations are aligned to EVOCORE_ARENA_ALIGNMENT.
 */
typedef struct {
    char *buffer;
    size_t capacity;
    size_t offset;
    size_t alignment;
} evocore_arena_t;

/**
 * Arena allocation result
 */
typedef struct {
    void *ptr;
    size_t size;
    size_t offset;
} evocore_arena_alloc_t;

/*========================================================================
 * Arena Management
 *========================================================================*/

/**
 * Initialize an arena allocator
 *
 * @param arena    Arena to initialize
 * @param capacity Initial capacity in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_arena_init(evocore_arena_t *arena, size_t capacity);

/**
 * Initialize an arena with a specific buffer
 *
 * @param arena    Arena to initialize
 * @param buffer   Pre-allocated buffer (must remain valid for arena lifetime)
 * @param capacity Size of the buffer
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_arena_init_with_buffer(evocore_arena_t *arena,
                                                void *buffer,
                                                size_t capacity);

/**
 * Cleanup an arena allocator
 *
 * Frees the arena's buffer. If arena was initialized with
 * arena_init_with_buffer(), the buffer is NOT freed.
 *
 * @param arena    Arena to cleanup
 */
void evocore_arena_cleanup(evocore_arena_t *arena);

/**
 * Allocate memory from arena
 *
 * @param arena    Arena to allocate from
 * @param size     Size to allocate
 * @return Allocated pointer, or NULL if out of space
 */
void* evocore_arena_alloc(evocore_arena_t *arena, size_t size);

/**
 * Allocate zeroed memory from arena
 *
 * @param arena    Arena to allocate from
 * @param size     Size to allocate
 * @return Allocated pointer, or NULL if out of space
 */
void* evocore_arena_calloc(evocore_arena_t *arena, size_t size);

/**
 * Allocate array from arena
 *
 * @param arena    Arena to allocate from
 * @param num      Number of elements
 * @param size     Size of each element
 * @return Allocated pointer, or NULL if out of space
 */
void* evocore_arena_alloc_array(evocore_arena_t *arena, size_t num, size_t size);

/**
 * Reset arena to initial state
 *
 * All previous allocations are invalidated. The buffer is not freed.
 *
 * @param arena    Arena to reset
 */
void evocore_arena_reset(evocore_arena_t *arena);

/**
 * Get remaining space in arena
 *
 * @param arena    Arena to check
 * @return Remaining bytes
 */
size_t evocore_arena_remaining(const evocore_arena_t *arena);

/**
 * Get used space in arena
 *
 * @param arena    Arena to check
 * @return Used bytes
 */
size_t evocore_arena_used(const evocore_arena_t *arena);

/**
 * Check if allocation would fit
 *
 * @param arena    Arena to check
 * @param size     Size to check
 * @return true if allocation would succeed
 */
bool evocore_arena_can_alloc(const evocore_arena_t *arena, size_t size);

/**
 * Create a snapshot of current arena state
 *
 * @param arena    Arena to snapshot
 * @return Current offset, can be used with arena_rewind
 */
size_t evocore_arena_snapshot(evocore_arena_t *arena);

/**
 * Rewind arena to a previous snapshot
 *
 * All allocations made after the snapshot are invalidated.
 *
 * @param arena    Arena to rewind
 * @param offset   Offset from evocore_arena_snapshot()
 */
void evocore_arena_rewind(evocore_arena_t *arena, size_t offset);

/**
 * Get arena statistics
 *
 * @param arena        Arena to query
 * @param total_size    Total capacity
 * @param used_size     Currently used
 * @param remaining     Remaining space
 * @param alloc_count   Approximate number of allocations
 */
void evocore_arena_get_stats(const evocore_arena_t *arena,
                             size_t *total_size,
                             size_t *used_size,
                             size_t *remaining,
                             size_t *alloc_count);

#endif /* EVOCORE_ARENA_H */
