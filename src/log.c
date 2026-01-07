#include "evocore/log.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

/*========================================================================
 * Log State
 *========================================================================*/

static evocore_log_level_t g_log_level = EVOCORE_LOG_INFO;
static FILE *g_log_file = NULL;
static bool g_log_color = true;
static const char *g_log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

/* ANSI color codes */
static const char *g_color_reset = "\033[0m";
static const char *g_color_levels[] = {
    "\033[36m",  /* TRACE - Cyan */
    "\033[34m",  /* DEBUG - Blue */
    "\033[32m",  /* INFO - Green */
    "\033[33m",  /* WARN - Yellow */
    "\033[31m",  /* ERROR - Red */
    "\033[35m",  /* FATAL - Magenta */
};

/*========================================================================
 * Public API
 *========================================================================*/

void evocore_log_set_level(evocore_log_level_t level) {
    if (level >= EVOCORE_LOG_TRACE && level <= EVOCORE_LOG_FATAL) {
        g_log_level = level;
    }
}

evocore_log_level_t evocore_log_get_level(void) {
    return g_log_level;
}

bool evocore_log_set_file(bool enabled, const char *path) {
    /* Close existing file */
    evocore_log_close();

    if (!enabled || path == NULL) {
        return true;
    }

    g_log_file = fopen(path, "a");
    if (g_log_file == NULL) {
        fprintf(stderr, "evocore: Failed to open log file: %s\n", path);
        return false;
    }

    return true;
}

void evocore_log_set_color(bool enabled) {
    g_log_color = enabled;
}

void evocore_log_close(void) {
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

void evocore_log_message(evocore_log_level_t level,
                       const char *file,
                       int line,
                       const char *fmt,
                       ...)
{
    if (level < g_log_level) {
        return;
    }

    /* Get current time */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Extract filename from path */
    const char *filename = file;
    const char *last_slash = strrchr(file, '/');
    if (last_slash) {
        filename = last_slash + 1;
    }

    /* Format the message */
    va_list args;
    va_start(args, fmt);

    /* Console output */
    if (g_log_color) {
        fprintf(stderr, "%s%s %-5s %s:%d%s ",
                g_color_levels[level], time_buf,
                g_log_level_names[level], filename, line,
                g_color_reset);
    } else {
        fprintf(stderr, "%s %-5s %s:%d ",
                time_buf, g_log_level_names[level], filename, line);
    }

    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    /* File output (no colors) */
    if (g_log_file != NULL) {
        va_start(args, fmt);
        fprintf(g_log_file, "%s %-5s %s:%d ",
                time_buf, g_log_level_names[level], filename, line);
        vfprintf(g_log_file, fmt, args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
        va_end(args);
    }

    /* Flush for fatal errors */
    if (level == EVOCORE_LOG_FATAL) {
        fflush(stderr);
    }
}
