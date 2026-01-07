#include "evocore/error.h"
#include <string.h>

const char* evocore_error_string(evocore_error_t err) {
    switch (err) {
        /* Success codes */
        case EVOCORE_OK:
            return "Success";
        case EVOCORE_SUCCESS_CONVERGED:
            return "Optimization converged";
        case EVOCORE_SUCCESS_MAX_GEN:
            return "Maximum generations reached";

        /* General errors */
        case EVOCORE_ERR_UNKNOWN:
            return "Unknown error";
        case EVOCORE_ERR_NULL_PTR:
            return "Null pointer argument";
        case EVOCORE_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case EVOCORE_ERR_INVALID_ARG:
            return "Invalid argument";

        /* Genome errors */
        case EVOCORE_ERR_GENOME_EMPTY:
            return "Genome is empty";
        case EVOCORE_ERR_GENOME_TOO_LARGE:
            return "Genome too large";
        case EVOCORE_ERR_GENOME_INVALID:
            return "Invalid genome";

        /* Population errors */
        case EVOCORE_ERR_POP_EMPTY:
            return "Population is empty";
        case EVOCORE_ERR_POP_FULL:
            return "Population is full";
        case EVOCORE_ERR_POP_SIZE:
            return "Invalid population size";

        /* Config errors */
        case EVOCORE_ERR_CONFIG_NOT_FOUND:
            return "Configuration file not found";
        case EVOCORE_ERR_CONFIG_PARSE:
            return "Configuration parse error";
        case EVOCORE_ERR_CONFIG_INVALID:
            return "Invalid configuration";

        /* File I/O errors */
        case EVOCORE_ERR_FILE_NOT_FOUND:
            return "File not found";
        case EVOCORE_ERR_FILE_READ:
            return "File read error";
        case EVOCORE_ERR_FILE_WRITE:
            return "File write error";

        default:
            return "Undefined error code";
    }
}
