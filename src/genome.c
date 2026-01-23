#define _GNU_SOURCE
#include "evocore/genome.h"
#include "internal.h"
#include "evocore/log.h"
#include <string.h>
#include <stdlib.h>

/*========================================================================
 * Genome Lifecycle
 *========================================================================*/

evocore_error_t evocore_genome_init(evocore_genome_t *genome, size_t capacity) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (capacity == 0) capacity = EVOCORE_MIN_CAPACITY;

    genome->data = evocore_calloc(1, capacity);
    if (!genome->data) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    genome->capacity = capacity;
    genome->size = 0;
    genome->owns_memory = true;

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_from_data(evocore_genome_t *genome,
                                     const void *data,
                                     size_t size) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (!data && size > 0) return EVOCORE_ERR_NULL_PTR;

    genome->data = evocore_malloc(size);
    if (!genome->data) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    if (data && size > 0) {
        memcpy(genome->data, data, size);
    }

    genome->capacity = size;
    genome->size = size;
    genome->owns_memory = true;

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_view(evocore_genome_t *genome,
                                const void *data,
                                size_t size) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (!data && size > 0) return EVOCORE_ERR_NULL_PTR;

    genome->data = (void*)data;
    genome->size = size;
    genome->capacity = size;
    genome->owns_memory = false;

    return EVOCORE_OK;
}

void evocore_genome_cleanup(evocore_genome_t *genome) {
    if (!genome) return;

    if (genome->owns_memory && genome->data) {
        evocore_free(genome->data);
    }

    genome->data = NULL;
    genome->size = 0;
    genome->capacity = 0;
    genome->owns_memory = false;
}

evocore_error_t evocore_genome_clone(const evocore_genome_t *src,
                                 evocore_genome_t *dst) {
    if (!src || !dst) return EVOCORE_ERR_NULL_PTR;

    return evocore_genome_from_data(dst, src->data, src->size);
}

/*========================================================================
 * Genome Manipulation
 *========================================================================*/

evocore_error_t evocore_genome_resize(evocore_genome_t *genome,
                                  size_t new_capacity) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (!genome->owns_memory) {
        return EVOCORE_ERR_INVALID_ARG;  /* Can't resize a view */
    }

    if (new_capacity <= genome->capacity) {
        genome->capacity = new_capacity;
        if (genome->size > new_capacity) {
            genome->size = new_capacity;
        }
        return EVOCORE_OK;
    }

    void *new_data = evocore_realloc(genome->data, new_capacity);
    if (!new_data) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    /* Zero out new memory */
    size_t extra = new_capacity - genome->capacity;
    memset((char*)new_data + genome->capacity, 0, extra);

    genome->data = new_data;
    genome->capacity = new_capacity;

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_set_size(evocore_genome_t *genome, size_t size) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (size > genome->capacity) {
        return EVOCORE_ERR_INVALID_ARG;
    }
    genome->size = size;
    return EVOCORE_OK;
}

evocore_error_t evocore_genome_write(evocore_genome_t *genome,
                                  size_t offset,
                                  const void *data,
                                  size_t size) {
    if (!genome || !data) return EVOCORE_ERR_NULL_PTR;

    if (offset + size > genome->capacity) {
        /* Auto-resize if owned */
        if (genome->owns_memory) {
            size_t new_cap = genome->capacity;
            while (new_cap < offset + size) {
                new_cap = new_cap ? new_cap * EVOCORE_GROWTH_FACTOR : EVOCORE_MIN_CAPACITY;
            }
            EVOCORE_CHECK(evocore_genome_resize(genome, new_cap));
        } else {
            return EVOCORE_ERR_INVALID_ARG;
        }
    }

    memcpy((char*)genome->data + offset, data, size);

    if (offset + size > genome->size) {
        genome->size = offset + size;
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_read(const evocore_genome_t *genome,
                                size_t offset,
                                void *data,
                                size_t size) {
    if (!genome || !data) return EVOCORE_ERR_NULL_PTR;
    if (!genome->data) return EVOCORE_ERR_GENOME_EMPTY;
    if (offset + size > genome->size) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    memcpy(data, (const char*)genome->data + offset, size);
    return EVOCORE_OK;
}

/*========================================================================
 * Genome Utilities
 *========================================================================*/

evocore_error_t evocore_genome_distance(const evocore_genome_t *a,
                                    const evocore_genome_t *b,
                                    size_t *distance) {
    if (!a || !b || !distance) return EVOCORE_ERR_NULL_PTR;
    if (!a->data || !b->data) return EVOCORE_ERR_GENOME_EMPTY;

    size_t min_size = a->size < b->size ? a->size : b->size;
    size_t max_size = a->size > b->size ? a->size : b->size;

    size_t diff = 0;
    const unsigned char *pa = (const unsigned char*)a->data;
    const unsigned char *pb = (const unsigned char*)b->data;

    for (size_t i = 0; i < min_size; i++) {
        if (pa[i] != pb[i]) diff++;
    }

    /* Account for size difference */
    diff += max_size - min_size;

    *distance = diff;
    return EVOCORE_OK;
}

evocore_error_t evocore_genome_zero(evocore_genome_t *genome) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (!genome->data) return EVOCORE_ERR_GENOME_EMPTY;

    memset(genome->data, 0, genome->size);
    return EVOCORE_OK;
}

evocore_error_t evocore_genome_randomize(evocore_genome_t *genome) {
    if (!genome) return EVOCORE_ERR_NULL_PTR;
    if (!genome->data) return EVOCORE_ERR_GENOME_EMPTY;

    unsigned char *data = (unsigned char*)genome->data;
    size_t len = genome->size > 0 ? genome->size : genome->capacity;
    for (size_t i = 0; i < len; i++) {
        data[i] = (unsigned char)rand();
    }

    /* Update size to reflect filled data if it was empty */
    if (genome->size == 0) {
        genome->size = len;
    }

    return EVOCORE_OK;
}

bool evocore_genome_is_valid(const evocore_genome_t *genome) {
    return genome && genome->data && genome->size > 0;
}

size_t evocore_genome_get_size(const evocore_genome_t *genome) {
    return genome ? genome->size : 0;
}

size_t evocore_genome_get_capacity(const evocore_genome_t *genome) {
    return genome ? genome->capacity : 0;
}

void* evocore_genome_get_data(const evocore_genome_t *genome) {
    return genome ? genome->data : NULL;
}
