#define _GNU_SOURCE
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*========================================================================
 * Memory Allocation Wrappers
 *========================================================================*/

void* evocore_malloc(size_t size) {
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (ptr == NULL) {
        fprintf(stderr, "evocore: Failed to allocate %zu bytes\n", size);
    }
    return ptr;
}

void* evocore_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    void *ptr = calloc(count, size);
    if (ptr == NULL) {
        fprintf(stderr, "evocore: Failed to allocate %zu elements of %zu bytes\n",
                count, size);
    }
    return ptr;
}

void* evocore_realloc(void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    if (new_ptr == NULL && size > 0) {
        fprintf(stderr, "evocore: Failed to reallocate to %zu bytes\n", size);
    }
    return new_ptr;
}

void evocore_free(void *ptr) {
    free(ptr);
}

/*========================================================================
 * String Utilities
 *========================================================================*/

char* evocore_strdup(const char *str) {
    if (str == NULL) return NULL;
    size_t len = strlen(str);
    char *dup = evocore_malloc(len + 1);
    if (dup) {
        memcpy(dup, str, len + 1);
    }
    return dup;
}

bool evocore_string_equals(const char *a, const char *b) {
    if (a == NULL || b == NULL) return a == b;
    return strcmp(a, b) == 0;
}

char* evocore_string_trim(char *str) {
    if (str == NULL) return NULL;

    /* Trim leading whitespace */
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    /* Trim trailing whitespace */
    char *end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    return start;
}

char* evocore_string_trim_newline(char *str) {
    if (str == NULL) return NULL;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[--len] = '\0';
    }
    return str;
}
