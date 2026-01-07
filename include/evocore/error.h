#ifndef EVOCORE_ERROR_H
#define EVOCORE_ERROR_H

#include <stddef.h>

/**
 * Error codes returned by evocore functions
 *
 * Positive values indicate success (with info).
 * Zero indicates success with no special info.
 * Negative values indicate errors.
 */
typedef enum {
    /* Success codes */
    EVOCORE_OK = 0,
    EVOCORE_SUCCESS_CONVERGED = 1,    /* Optimization converged */
    EVOCORE_SUCCESS_MAX_GEN = 2,      /* Reached max generations */

    /* General errors (negative) */
    EVOCORE_ERR_UNKNOWN = -1,
    EVOCORE_ERR_NULL_PTR = -2,        /* Null pointer argument */
    EVOCORE_ERR_OUT_OF_MEMORY = -3,   /* Memory allocation failed */
    EVOCORE_ERR_INVALID_ARG = -4,     /* Invalid argument value */
    EVOCORE_ERR_NOT_IMPLEMENTED = -5, /* Feature not implemented */

    /* Genome errors */
    EVOCORE_ERR_GENOME_EMPTY = -10,
    EVOCORE_ERR_GENOME_TOO_LARGE = -11,
    EVOCORE_ERR_GENOME_INVALID = -12,

    /* Population errors */
    EVOCORE_ERR_POP_EMPTY = -20,
    EVOCORE_ERR_POP_FULL = -21,
    EVOCORE_ERR_POP_SIZE = -22,

    /* Config errors */
    EVOCORE_ERR_CONFIG_NOT_FOUND = -30,
    EVOCORE_ERR_CONFIG_PARSE = -31,
    EVOCORE_ERR_CONFIG_INVALID = -32,

    /* File I/O errors */
    EVOCORE_ERR_FILE_NOT_FOUND = -40,
    EVOCORE_ERR_FILE_READ = -41,
    EVOCORE_ERR_FILE_WRITE = -42
} evocore_error_t;

/**
 * Get human-readable error message
 */
const char* evocore_error_string(evocore_error_t err);

/**
 * Macro for checking and returning errors
 */
#define EVOCORE_CHECK(expr) \
    do { \
        evocore_error_t _err = (expr); \
        if (_err != EVOCORE_OK) \
            return _err; \
    } while(0)

/**
 * Macro for logging and returning errors
 */
#define EVOCORE_CHECK_LOG(expr, msg) \
    do { \
        evocore_error_t _err = (expr); \
        if (_err != EVOCORE_OK) { \
            evocore_log_error("%s: %s", msg, evocore_error_string(_err)); \
            return _err; \
        } \
    } while(0)

#endif /* EVOCORE_ERROR_H */
