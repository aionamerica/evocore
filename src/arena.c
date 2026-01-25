/**
 * Arena Allocator Implementation
 *
 * Fast bulk allocation with cleanup-by-reset semantics.
 */

#define _GNU_SOURCE
#include "evocore/arena.h"
#include "evocore/log.h"
#include "internal.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*========================================================================
 * Arena Management
 *========================================================================*/

evocore_error_t evocore_arena_init(evocore_arena_t *arena, size_t capacity) {
    if (!arena) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (capacity == 0) {
        capacity = EVOCORE_ARENA_DEFAULT_CAPACITY;
    }

    /* Align capacity */
    capacity = (capacity + EVOCORE_ARENA_ALIGNMENT - 1) &
               ~(EVOCORE_ARENA_ALIGNMENT - 1);

    char *buffer = (char*)evocore_malloc(capacity);
    if (!buffer) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    arena->buffer = buffer;
    arena->capacity = capacity;
    arena->offset = 0;
    arena->alignment = EVOCORE_ARENA_ALIGNMENT;
    arena->owns_buffer = true;  /* We allocated the buffer */

    return EVOCORE_OK;
}

evocore_error_t evocore_arena_init_with_buffer(evocore_arena_t *arena,
                                                void *buffer,
                                                size_t capacity) {
    if (!arena || !buffer) {
        return EVOCORE_ERR_NULL_PTR;
    }

    arena->buffer = (char*)buffer;
    arena->capacity = capacity;
    arena->offset = 0;
    arena->alignment = EVOCORE_ARENA_ALIGNMENT;
    arena->owns_buffer = false;  /* External buffer, do not free */

    return EVOCORE_OK;
}

void evocore_arena_cleanup(evocore_arena_t *arena) {
    if (!arena) {
        return;
    }

    /* Only free if we own the buffer (allocated via arena_init) */
    if (arena->owns_buffer && arena->buffer) {
        evocore_free(arena->buffer);
    }

    arena->buffer = NULL;
    arena->capacity = 0;
    arena->offset = 0;
    arena->owns_buffer = false;
}

void* evocore_arena_alloc(evocore_arena_t *arena, size_t size) {
    if (!arena || !arena->buffer) {
        return NULL;
    }

    /* Align size */
    size = (size + arena->alignment - 1) & ~(arena->alignment - 1);

    /* Check capacity */
    if (arena->offset + size > arena->capacity) {
        evocore_log_error("Arena out of memory: need %zu, have %zu",
                         arena->offset + size, arena->capacity);
        return NULL;
    }

    void *ptr = arena->buffer + arena->offset;
    arena->offset += size;

    return ptr;
}

void* evocore_arena_calloc(evocore_arena_t *arena, size_t size) {
    void *ptr = evocore_arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void* evocore_arena_alloc_array(evocore_arena_t *arena, size_t num, size_t size) {
    /* Check for overflow */
    if (num > 0 && size > SIZE_MAX / num) {
        return NULL;
    }

    return evocore_arena_alloc(arena, num * size);
}

void evocore_arena_reset(evocore_arena_t *arena) {
    if (!arena) {
        return;
    }

    arena->offset = 0;
}

size_t evocore_arena_remaining(const evocore_arena_t *arena) {
    if (!arena) {
        return 0;
    }

    if (arena->offset >= arena->capacity) {
        return 0;
    }

    return arena->capacity - arena->offset;
}

size_t evocore_arena_used(const evocore_arena_t *arena) {
    if (!arena) {
        return 0;
    }

    return arena->offset;
}

bool evocore_arena_can_alloc(const evocore_arena_t *arena, size_t size) {
    if (!arena) {
        return false;
    }

    /* Align size */
    size = (size + arena->alignment - 1) & ~(arena->alignment - 1);

    return (arena->offset + size <= arena->capacity);
}

size_t evocore_arena_snapshot(evocore_arena_t *arena) {
    if (!arena) {
        return 0;
    }

    return arena->offset;
}

void evocore_arena_rewind(evocore_arena_t *arena, size_t offset) {
    if (!arena) {
        return;
    }

    if (offset <= arena->offset) {
        arena->offset = offset;
    }
}

void evocore_arena_get_stats(const evocore_arena_t *arena,
                             size_t *total_size,
                             size_t *used_size,
                             size_t *remaining,
                             size_t *alloc_count) {
    if (!arena) {
        if (total_size) *total_size = 0;
        if (used_size) *used_size = 0;
        if (remaining) *remaining = 0;
        if (alloc_count) *alloc_count = 0;
        return;
    }

    if (total_size) *total_size = arena->capacity;
    if (used_size) *used_size = arena->offset;
    if (remaining) *remaining = evocore_arena_remaining(arena);

    /* We don't track alloc count, estimate */
    if (alloc_count) {
        *alloc_count = 0;  /* Would need counter to track accurately */
    }
}
