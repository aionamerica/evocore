/**
 * Persistence and State Management Module
 *
 * Provides serialization, deserialization, and checkpointing
 * for evolutionary computation state.
 */

#ifndef EVOCORE_PERSIST_H
#define EVOCORE_PERSIST_H

#include "evocore/genome.h"
#include "evocore/population.h"
#include "evocore/domain.h"
#include "evocore/meta.h"
#include "evocore/error.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/*========================================================================
 * Serialization Format
 *========================================================================*/

/**
 * Serialization format types
 */
typedef enum {
    EVOCORE_SERIAL_FORMAT_JSON,      /* Human-readable JSON */
    EVOCORE_SERIAL_FORMAT_BINARY,    /* Compact binary format */
    EVOCORE_SERIAL_FORMAT_MSGPACK,    /* MessagePack (future) */
} evocore_serial_format_t;

/**
 * Serialization options
 */
typedef struct {
    evocore_serial_format_t format;
    bool include_metadata;     /* Include generation, timestamps, etc. */
    bool pretty_print;          /* For JSON: pretty print output */
    int compression_level;      /* 0-9, 0 = none, 9 = max (future) */
} evocore_serial_options_t;

/**
 * Default serialization options
 */
#define EVOCORE_SERIAL_DEFAULTS { .format = EVOCORE_SERIAL_FORMAT_JSON, \
                                  .include_metadata = true, \
                                  .pretty_print = true, \
                                  .compression_level = 0 }

/*========================================================================
 * Genome Serialization
 *========================================================================*/

/**
 * Serialize genome to string buffer
 *
 * @param genome        Genome to serialize
 * @param buffer        Output buffer (NULL = allocate new)
 * @param buffer_size   Buffer size (input) or bytes written (output)
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_serialize(const evocore_genome_t *genome,
                                     char **buffer,
                                     size_t *buffer_size,
                                     const evocore_serial_options_t *options);

/**
 * Deserialize genome from string buffer
 *
 * @param buffer        Input buffer
 * @param buffer_size   Buffer size
 * @param genome        Genome to deserialize into
 * @param format        Format to expect
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_deserialize(const char *buffer,
                                       size_t buffer_size,
                                       evocore_genome_t *genome,
                                       evocore_serial_format_t format);

/**
 * Serialize genome to file
 *
 * @param genome        Genome to serialize
 * @param filepath      Output file path
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_save(const evocore_genome_t *genome,
                                const char *filepath,
                                const evocore_serial_options_t *options);

/**
 * Deserialize genome from file
 *
 * @param filepath      Input file path
 * @param genome        Genome to deserialize into
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_genome_load(const char *filepath,
                                evocore_genome_t *genome);

/*========================================================================
 * Population Serialization
 *========================================================================*/

/**
 * Serialize population to string buffer
 *
 * @param pop           Population to serialize
 * @param domain        Domain for genome metadata
 * @param buffer        Output buffer (NULL = allocate new)
 * @param buffer_size   Buffer size
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_serialize(const evocore_population_t *pop,
                                         const evocore_domain_t *domain,
                                         char **buffer,
                                         size_t *buffer_size,
                                         const evocore_serial_options_t *options);

/**
 * Deserialize population from string buffer
 *
 * @param buffer        Input buffer
 * @param buffer_size   Buffer size
 * @param pop           Population to deserialize into
 * @param domain        Domain for genome metadata
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_deserialize(const char *buffer,
                                           size_t buffer_size,
                                           evocore_population_t *pop,
                                           const evocore_domain_t *domain);

/**
 * Save population to file
 *
 * @param pop           Population to save
 * @param domain        Domain for genome metadata
 * @param filepath      Output file path
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_save(const evocore_population_t *pop,
                                    const evocore_domain_t *domain,
                                    const char *filepath,
                                    const evocore_serial_options_t *options);

/**
 * Load population from file
 *
 * @param filepath      Input file path
 * @param pop           Population to load into
 * @param domain        Domain for genome metadata
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_population_load(const char *filepath,
                                    evocore_population_t *pop,
                                    const evocore_domain_t *domain);

/*========================================================================
 * Meta-Evolution State Serialization
 *========================================================================*/

/**
 * Serialize meta-population to string
 *
 * @param meta_pop      Meta-population to serialize
 * @param buffer        Output buffer
 * @param buffer_size   Buffer size
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_serialize(const evocore_meta_population_t *meta_pop,
                                   char **buffer,
                                   size_t *buffer_size,
                                   const evocore_serial_options_t *options);

/**
 * Deserialize meta-population from string
 *
 * @param buffer        Input buffer
 * @param buffer_size   Buffer size
 * @param meta_pop      Meta-population to deserialize into
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_deserialize(const char *buffer,
                                      size_t buffer_size,
                                      evocore_meta_population_t *meta_pop);

/**
 * Save meta-population to file
 *
 * @param meta_pop      Meta-population to save
 * @param filepath      Output file path
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_save(const evocore_meta_population_t *meta_pop,
                               const char *filepath,
                               const evocore_serial_options_t *options);

/**
 * Load meta-population from file
 *
 * @param filepath      Input file path
 * @param meta_pop      Meta-population to load into
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_meta_load(const char *filepath,
                               evocore_meta_population_t *meta_pop);

/*========================================================================
 * Checkpoint Management
 *========================================================================*/

/**
 * Checkpoint data structure
 *
 * Contains the complete state of an evolutionary run
 * that can be saved and restored.
 */
typedef struct {
    char version[16];              /* Format version */
    double timestamp;              /* Unix timestamp */

    /* Population state */
    size_t population_size;
    size_t population_capacity;
    size_t generation;
    double best_fitness;
    double avg_fitness;

    /* Meta-evolution state */
    bool has_meta_state;
    evocore_meta_params_t meta_params;

    /* Domain info */
    char domain_name[64];

    /* User data */
    char *user_data;               /* Arbitrary user state */
    size_t user_data_size;

    /* Serialized populations */
    char *population_data;         /* JSON or binary population */
    size_t population_data_size;

    char *meta_data;               /* JSON or binary meta-population */
    size_t meta_data_size;
} evocore_checkpoint_t;

/**
 * Create a checkpoint
 *
 * Captures the complete state for saving.
 *
 * @param checkpoint    Checkpoint to fill
 * @param pop           Population to checkpoint
 * @param domain        Domain for context
 * @param meta_pop      Optional meta-population (NULL to skip)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_checkpoint_create(evocore_checkpoint_t *checkpoint,
                                      const evocore_population_t *pop,
                                      const evocore_domain_t *domain,
                                      const evocore_meta_population_t *meta_pop);

/**
 * Save checkpoint to file
 *
 * @param checkpoint    Checkpoint to save
 * @param filepath      Output file path
 * @param options       Serialization options
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_checkpoint_save(const evocore_checkpoint_t *checkpoint,
                                    const char *filepath,
                                    const evocore_serial_options_t *options);

/**
 * Load checkpoint from file
 *
 * @param filepath      Input file path
 * @param checkpoint    Checkpoint to load into
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_checkpoint_load(const char *filepath,
                                    evocore_checkpoint_t *checkpoint);

/**
 * Restore state from checkpoint
 *
 * @param checkpoint    Checkpoint to restore from
 * @param pop           Population to restore into
 * @param domain        Domain for context
 * @param meta_pop      Optional meta-population to restore (NULL to skip)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_checkpoint_restore(const evocore_checkpoint_t *checkpoint,
                                       evocore_population_t *pop,
                                       const evocore_domain_t *domain,
                                       evocore_meta_population_t *meta_pop);

/**
 * Free checkpoint resources
 *
 * @param checkpoint    Checkpoint to free
 */
void evocore_checkpoint_free(evocore_checkpoint_t *checkpoint);

/*========================================================================
 * Checkpoint Management (Advanced)
 *========================================================================*/

/**
 * Auto-checkpoint configuration
 */
typedef struct {
    bool enabled;
    int every_n_generations;     /* 0 = only on explicit request */
    char directory[256];         /* Directory for checkpoints */
    int max_checkpoints;          /* 0 = unlimited, >0 = keep N most recent */
    bool compress;                /* Use compression (future) */
} evocore_auto_checkpoint_config_t;

/**
 * Default auto-checkpoint configuration
 */
#define EVOCORE_AUTOCHECK_DEFAULTS { .enabled = false, \
                                    .every_n_generations = 10, \
                                    .directory = "./checkpoints", \
                                    .max_checkpoints = 5, \
                                    .compress = false }

/**
 * Checkpoint manager
 *
 * Manages automatic checkpointing and cleanup.
 */
typedef struct evocore_checkpoint_manager_t evocore_checkpoint_manager_t;

/**
 * Create checkpoint manager
 *
 * @param config        Configuration
 * @return Manager instance, or NULL on failure
 */
evocore_checkpoint_manager_t* evocore_checkpoint_manager_create(
    const evocore_auto_checkpoint_config_t *config);

/**
 * Destroy checkpoint manager
 *
 * @param manager       Manager to destroy
 */
void evocore_checkpoint_manager_destroy(evocore_checkpoint_manager_t *manager);

/**
 * Update checkpoint manager (call after each generation)
 *
 * Creates automatic checkpoints if configured.
 *
 * @param manager       Checkpoint manager
 * @param pop           Current population
 * @param domain        Domain for context
 * @param meta_pop      Optional meta-population
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_checkpoint_manager_update(evocore_checkpoint_manager_t *manager,
                                             const evocore_population_t *pop,
                                             const evocore_domain_t *domain,
                                             const evocore_meta_population_t *meta_pop);

/**
 * List available checkpoints
 *
 * @param directory     Checkpoint directory
 * @param count         Output: number of checkpoints found
 * @return Array of checkpoint filepaths (caller frees), or NULL
 */
char** evocore_checkpoint_list(const char *directory, int *count);

/**
 * Free checkpoint list
 *
 * @param list          List from checkpoint_list
 * @param count         Number of entries
 */
void evocore_checkpoint_list_free(char **list, int count);

/**
 * Get checkpoint info from file
 *
 * @param filepath      Checkpoint file
 * @param checkpoint    Output: checkpoint info (user frees)
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_checkpoint_info(const char *filepath,
                                     evocore_checkpoint_t *checkpoint);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Calculate checksum of data
 *
 * @param data          Data buffer
 * @param size          Data size
 * @return 32-bit checksum
 */
uint32_t evocore_checksum(const void *data, size_t size);

/**
 * Validate checksum during deserialization
 *
 * @param data          Data buffer
 * @param size          Data size
 * @param expected      Expected checksum
 * @return true if checksum matches, false otherwise
 */
bool evocore_checksum_validate(const void *data, size_t size, uint32_t expected);

#endif /* EVOCORE_PERSIST_H */
