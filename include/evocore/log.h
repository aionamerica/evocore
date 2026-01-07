#ifndef EVOCORE_LOG_H
#define EVOCORE_LOG_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Log levels
 */
typedef enum {
    EVOCORE_LOG_TRACE = 0,
    EVOCORE_LOG_DEBUG,
    EVOCORE_LOG_INFO,
    EVOCORE_LOG_WARN,
    EVOCORE_LOG_ERROR,
    EVOCORE_LOG_FATAL
} evocore_log_level_t;

/**
 * Set current log level
 *
 * Messages below this level will be ignored.
 *
 * @param level     Log level
 */
void evocore_log_set_level(evocore_log_level_t level);

/**
 * Enable/disable file logging
 *
 * @param enabled   true to enable, false to disable
 * @param path      Path to log file (ignored if disabled)
 * @return true on success, false otherwise
 */
bool evocore_log_set_file(bool enabled, const char *path);

/**
 * Enable/disable colored console output
 *
 * @param enabled   true to enable colors, false to disable
 */
void evocore_log_set_color(bool enabled);

/**
 * Get current log level
 */
evocore_log_level_t evocore_log_get_level(void);

/**
 * Close log file (if open)
 */
void evocore_log_close(void);

/*========================================================================
 * Logging Macros
 *========================================================================*/

/**
 * Log a trace message
 */
#define evocore_log_trace(...) \
    evocore_log_message(EVOCORE_LOG_TRACE, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a debug message
 */
#define evocore_log_debug(...) \
    evocore_log_message(EVOCORE_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log an info message
 */
#define evocore_log_info(...) \
    evocore_log_message(EVOCORE_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a warning message
 */
#define evocore_log_warn(...) \
    evocore_log_message(EVOCORE_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log an error message
 */
#define evocore_log_error(...) \
    evocore_log_message(EVOCORE_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/**
 * Log a fatal message and exit
 */
#define evocore_log_fatal(...) \
    do { \
        evocore_log_message(EVOCORE_LOG_FATAL, __FILE__, __LINE__, __VA_ARGS__); \
        exit(1); \
    } while(0)

/**
 * Core logging function
 *
 * @param level     Log level
 * @param file      Source file name
 * @param line      Line number
 * @param fmt       Format string
 * @param ...       Format arguments
 */
void evocore_log_message(evocore_log_level_t level,
                       const char *file,
                       int line,
                       const char *fmt,
                       ...);

#endif /* EVOCORE_LOG_H */
