#ifndef EVOCORE_GENOME_H
#define EVOCORE_GENOME_H

#include <stddef.h>
#include <stdbool.h>
#include "evocore/error.h"

/**
 * Genome structure
 *
 * A genome is an opaque byte array representing a candidate solution.
 * The framework treats genomes as opaque - only the domain knows
 * the internal structure.
 */
typedef struct {
    void *data;              /* Raw genome data */
    size_t size;             /* Current size in bytes */
    size_t capacity;         /* Allocated capacity */
    bool owns_memory;        /* Whether we allocated the memory */
} evocore_genome_t;

/**
 * Result structure for fitness + genome pair
 */
typedef struct {
    evocore_genome_t *genome;
    double fitness;
} evocore_individual_t;

/*========================================================================
 * Genome Lifecycle
 *========================================================================*/

/**
 * Create a new genome with specified capacity
 *
 * @param genome    Pointer to genome structure to initialize
 * @param capacity  Initial capacity in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_init(evocore_genome_t *genome, size_t capacity);

/**
 * Create a genome by copying existing data
 *
 * @param genome    Pointer to genome structure to initialize
 * @param data      Data to copy (must be valid)
 * @param size      Size of data in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_from_data(evocore_genome_t *genome,
                                     const void *data,
                                     size_t size);

/**
 * Create a view (wrapper) over existing data without copying
 *
 * The genome does NOT own the memory and will not free it.
 *
 * @param genome    Pointer to genome structure to initialize
 * @param data      Data to wrap (must remain valid)
 * @param size      Size of data in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_view(evocore_genome_t *genome,
                                const void *data,
                                size_t size);

/**
 * Free genome resources
 *
 * @param genome    Genome to clean up
 */
void evocore_genome_cleanup(evocore_genome_t *genome);

/**
 * Clone a genome
 *
 * @param src       Source genome to copy from
 * @param dst       Destination genome (must be uninitialized)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_clone(const evocore_genome_t *src,
                                 evocore_genome_t *dst);

/*========================================================================
 * Genome Manipulation
 *========================================================================*/

/**
 * Resize genome capacity
 *
 * Preserves existing data if growing. May reallocate.
 *
 * @param genome        Genome to resize
 * @param new_capacity  New capacity in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_resize(evocore_genome_t *genome,
                                  size_t new_capacity);

/**
 * Set genome size
 *
 * Sets the logical size without changing capacity.
 * New size must be <= capacity.
 *
 * @param genome    Genome to modify
 * @param size      New size in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_set_size(evocore_genome_t *genome, size_t size);

/**
 * Copy data into genome at specified offset
 *
 * @param genome    Destination genome
 * @param offset    Byte offset to write to
 * @param data      Source data
 * @param size      Size of data to copy
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_write(evocore_genome_t *genome,
                                  size_t offset,
                                  const void *data,
                                  size_t size);

/**
 * Read data from genome at specified offset
 *
 * @param genome    Source genome
 * @param offset    Byte offset to read from
 * @param data      Destination buffer
 * @param size      Size of data to read
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_read(const evocore_genome_t *genome,
                                size_t offset,
                                void *data,
                                size_t size);

/*========================================================================
 * Genome Utilities
 *========================================================================*/

/**
 * Calculate Hamming distance between two genomes
 *
 * For genomes of equal size, counts differing bytes.
 *
 * @param a         First genome
 * @param b         Second genome
 * @param distance  Output: calculated distance
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_distance(const evocore_genome_t *a,
                                    const evocore_genome_t *b,
                                    size_t *distance);

/**
 * Zero out genome contents
 *
 * @param genome    Genome to clear
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_zero(evocore_genome_t *genome);

/**
 * Fill genome with random bytes
 *
 * @param genome    Genome to fill
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_randomize(evocore_genome_t *genome);

/**
 * Check if genome is valid (non-null data and size > 0)
 *
 * @param genome    Genome to check
 * @return true if valid, false otherwise
 */
bool evocore_genome_is_valid(const evocore_genome_t *genome);

/**
 * Get genome size
 *
 * @param genome    Genome
 * @return Size in bytes
 */
size_t evocore_genome_get_size(const evocore_genome_t *genome);

/**
 * Get genome capacity
 *
 * @param genome    Genome
 * @return Capacity in bytes
 */
size_t evocore_genome_get_capacity(const evocore_genome_t *genome);

/**
 * Get genome data pointer
 *
 * @param genome    Genome
 * @return Pointer to data or NULL
 */
void* evocore_genome_get_data(const evocore_genome_t *genome);

#endif /* EVOCORE_GENOME_H */
