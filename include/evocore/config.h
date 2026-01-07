#ifndef EVOCORE_CONFIG_H
#define EVOCORE_CONFIG_H

#include <stdbool.h>
#include "evocore/error.h"

/**
 * Opaque configuration structure
 */
typedef struct evocore_config_s evocore_config_t;

/**
 * Configuration value type
 */
typedef enum {
    EVOCORE_CONFIG_TYPE_STRING,
    EVOCORE_CONFIG_TYPE_INT,
    EVOCORE_CONFIG_TYPE_DOUBLE,
    EVOCORE_CONFIG_TYPE_BOOL
} evocore_config_type_t;

/**
 * Configuration entry
 */
typedef struct {
    const char *key;
    const char *value;
    evocore_config_type_t type;
} evocore_config_entry_t;

/*========================================================================
 * Config Lifecycle
 *========================================================================*/

/**
 * Load configuration from file
 *
 * @param path      Path to INI file
 * @param config    Output: configuration object
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_config_load(const char *path,
                                 evocore_config_t **config);

/**
 * Free configuration
 *
 * @param config    Configuration to free (can be NULL)
 */
void evocore_config_free(evocore_config_t *config);

/*========================================================================
 * Config Accessors
 *========================================================================*/

/**
 * Get string value
 *
 * @param config        Configuration object
 * @param section       Section name (NULL for global)
 * @param key           Key name
 * @param default_value Default if not found
 * @return String value or default
 */
const char* evocore_config_get_string(const evocore_config_t *config,
                                     const char *section,
                                     const char *key,
                                     const char *default_value);

/**
 * Get integer value
 *
 * @param config        Configuration object
 * @param section       Section name (NULL for global)
 * @param key           Key name
 * @param default_value Default if not found
 * @return Integer value or default
 */
int evocore_config_get_int(const evocore_config_t *config,
                          const char *section,
                          const char *key,
                          int default_value);

/**
 * Get double value
 *
 * @param config        Configuration object
 * @param section       Section name (NULL for global)
 * @param key           Key name
 * @param default_value Default if not found
 * @return Double value or default
 */
double evocore_config_get_double(const evocore_config_t *config,
                                const char *section,
                                const char *key,
                                double default_value);

/**
 * Get boolean value
 *
 * @param config        Configuration object
 * @param section       Section name (NULL for global)
 * @param key           Key name
 * @param default_value Default if not found
 * @return Boolean value or default
 */
bool evocore_config_get_bool(const evocore_config_t *config,
                            const char *section,
                            const char *key,
                            bool default_value);

/**
 * Check if key exists
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param key       Key name
 * @return true if key exists, false otherwise
 */
bool evocore_config_has_key(const evocore_config_t *config,
                           const char *section,
                           const char *key);

/**
 * Get number of entries in a section
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @return Number of entries, or 0 if section not found
 */
size_t evocore_config_section_size(const evocore_config_t *config,
                                  const char *section);

/**
 * Iterate through entries in a section
 *
 * Returns entry at index, or NULL if out of bounds.
 *
 * @param config    Configuration object
 * @param section   Section name (NULL for global)
 * @param index     Entry index
 * @return Entry pointer or NULL
 */
const evocore_config_entry_t* evocore_config_get_entry(const evocore_config_t *config,
                                                    const char *section,
                                                    size_t index);

#endif /* EVOCORE_CONFIG_H */
