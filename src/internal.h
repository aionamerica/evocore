#ifndef EVOCORE_INTERNAL_H
#define EVOCORE_INTERNAL_H

#include <stddef.h>
#include <stdbool.h>

/**
 * Internal shared definitions for evocore
 */

#define EVOCORE_MIN_CAPACITY 16
#define EVOCORE_GROWTH_FACTOR 2

/**
 * Safe allocation wrappers
 */
void* evocore_malloc(size_t size);
void* evocore_calloc(size_t count, size_t size);
void* evocore_realloc(void *ptr, size_t size);
void evocore_free(void *ptr);

/**
 * String utilities
 */
char* evocore_strdup(const char *str);
bool evocore_string_equals(const char *a, const char *b);
char* evocore_string_trim(char *str);
char* evocore_string_trim_newline(char *str);

#endif /* EVOCORE_INTERNAL_H */
