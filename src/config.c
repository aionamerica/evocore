#include "evocore/config.h"
#include "internal.h"
#include "evocore/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*========================================================================
 * Internal Structures
 *========================================================================*/

#define MAX_LINE_LENGTH 4096
#define MAX_SECTIONS 64
#define MAX_KEYS_PER_SECTION 256

typedef struct config_entry_s {
    char *key;
    char *value;
    struct config_entry_s *next;
} config_entry_t;

typedef struct config_section_s {
    char *name;
    config_entry_t *entries;
    size_t entry_count;
    struct config_section_s *next;
} config_section_t;

struct evocore_config_s {
    config_section_t *sections;
    config_section_t *global;  /* NULL-named section for global keys */
};

/*========================================================================
 * Internal Helpers
 *========================================================================*/

static config_section_t* find_section(const evocore_config_t *config,
                                       const char *name) {
    config_section_t *sec = config->sections;
    while (sec) {
        if (evocore_string_equals(sec->name, name)) {
            return sec;
        }
        sec = sec->next;
    }
    return NULL;
}

static config_section_t* create_section(const char *name) {
    config_section_t *sec = evocore_calloc(1, sizeof(config_section_t));
    if (!sec) return NULL;

    sec->name = evocore_strdup(name ? name : "");
    if (!sec->name) {
        evocore_free(sec);
        return NULL;
    }
    return sec;
}

static config_entry_t* find_entry(const config_section_t *sec,
                                   const char *key) {
    config_entry_t *entry = sec->entries;
    while (entry) {
        if (evocore_string_equals(entry->key, key)) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

static bool add_entry(config_section_t *sec, const char *key,
                      const char *value) {
    /* Check for existing key - update it */
    config_entry_t *existing = find_entry(sec, key);
    if (existing) {
        evocore_free(existing->value);
        existing->value = evocore_strdup(value);
        return existing->value != NULL;
    }

    /* Create new entry */
    config_entry_t *entry = evocore_calloc(1, sizeof(config_entry_t));
    if (!entry) return false;

    entry->key = evocore_strdup(key);
    entry->value = evocore_strdup(value);
    if (!entry->key || !entry->value) {
        evocore_free(entry->key);
        evocore_free(entry->value);
        evocore_free(entry);
        return false;
    }

    /* Add to front of list */
    entry->next = sec->entries;
    sec->entries = entry;
    sec->entry_count++;

    return true;
}

static void trim_comment(char *line) {
    char *hash = strchr(line, '#');
    if (hash && (hash == line || *(hash - 1) != '\\')) {
        *hash = '\0';
    }
    char *semi = strchr(line, ';');
    if (semi && (semi == line || *(semi - 1) != '\\')) {
        *semi = '\0';
    }
}

static bool parse_bool_string(const char *value) {
    if (!value) return false;
    char lower_val[32];
    size_t i;
    for (i = 0; i < sizeof(lower_val) - 1 && value[i]; i++) {
        lower_val[i] = tolower((unsigned char)value[i]);
    }
    lower_val[i] = '\0';

    return (strcmp(lower_val, "true") == 0 ||
            strcmp(lower_val, "yes") == 0 ||
            strcmp(lower_val, "1") == 0 ||
            strcmp(lower_val, "on") == 0);
}

/*========================================================================
 * Public API
 *========================================================================*/

evocore_error_t evocore_config_load(const char *path,
                                 evocore_config_t **config_out) {
    if (!path || !config_out) {
        return EVOCORE_ERR_NULL_PTR;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    evocore_config_t *config = evocore_calloc(1, sizeof(evocore_config_t));
    if (!config) {
        fclose(fp);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    /* Create global section */
    config->global = create_section(NULL);
    if (!config->global) {
        evocore_free(config);
        fclose(fp);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }
    config->sections = config->global;

    config_section_t *current_section = config->global;
    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        char *trimmed = evocore_string_trim(line);
        evocore_string_trim_newline(trimmed);

        /* Skip empty lines */
        if (*trimmed == '\0') continue;

        /* Skip comments */
        trim_comment(trimmed);
        trimmed = evocore_string_trim(trimmed);
        if (*trimmed == '\0') continue;

        /* Section header */
        if (*trimmed == '[') {
            char *end = strchr(trimmed, ']');
            if (!end) {
                evocore_log_warn("Line %d: Invalid section header", line_num);
                continue;
            }
            *end = '\0';
            char *sec_name = trimmed + 1;
            sec_name = evocore_string_trim(sec_name);

            /* Check if section exists */
            config_section_t *sec = find_section(config, sec_name);
            if (!sec) {
                sec = create_section(sec_name);
                if (!sec) {
                    evocore_log_error("Failed to create section: %s", sec_name);
                    break;
                }
                /* Add to sections list */
                sec->next = config->sections;
                config->sections = sec;
            }
            current_section = sec;
            continue;
        }

        /* Key-value pair */
        char *eq = strchr(trimmed, '=');
        if (!eq) {
            evocore_log_warn("Line %d: Invalid key-value pair", line_num);
            continue;
        }

        *eq = '\0';
        char *key = evocore_string_trim(trimmed);
        char *value = evocore_string_trim(eq + 1);

        if (*key == '\0') {
            evocore_log_warn("Line %d: Empty key", line_num);
            continue;
        }

        if (!add_entry(current_section, key, value)) {
            evocore_log_error("Failed to add entry: %s", key);
            break;
        }
    }

    fclose(fp);
    *config_out = config;
    return EVOCORE_OK;
}

void evocore_config_free(evocore_config_t *config) {
    if (!config) return;

    config_section_t *sec = config->sections;
    while (sec) {
        config_section_t *next_sec = sec->next;

        config_entry_t *entry = sec->entries;
        while (entry) {
            config_entry_t *next_entry = entry->next;
            evocore_free(entry->key);
            evocore_free(entry->value);
            evocore_free(entry);
            entry = next_entry;
        }

        evocore_free(sec->name);
        evocore_free(sec);
        sec = next_sec;
    }

    evocore_free(config);
}

const char* evocore_config_get_string(const evocore_config_t *config,
                                     const char *section,
                                     const char *key,
                                     const char *default_value) {
    if (!config) return default_value;

    /* NULL section means global section (empty string name) */
    const char *sec_name = section ? section : "";
    config_section_t *sec = find_section(config, sec_name);
    if (!sec) return default_value;

    config_entry_t *entry = find_entry(sec, key);
    if (!entry) return default_value;

    return entry->value;
}

int evocore_config_get_int(const evocore_config_t *config,
                          const char *section,
                          const char *key,
                          int default_value) {
    const char *str = evocore_config_get_string(config, section, key, NULL);
    if (!str) return default_value;
    return atoi(str);
}

double evocore_config_get_double(const evocore_config_t *config,
                                const char *section,
                                const char *key,
                                double default_value) {
    const char *str = evocore_config_get_string(config, section, key, NULL);
    if (!str) return default_value;
    return atof(str);
}

bool evocore_config_get_bool(const evocore_config_t *config,
                            const char *section,
                            const char *key,
                            bool default_value) {
    const char *str = evocore_config_get_string(config, section, key, NULL);
    if (!str) return default_value;
    return parse_bool_string(str);
}

bool evocore_config_has_key(const evocore_config_t *config,
                           const char *section,
                           const char *key) {
    if (!config) return false;

    /* NULL section means global section (empty string name) */
    const char *sec_name = section ? section : "";
    config_section_t *sec = find_section(config, sec_name);
    if (!sec) return false;

    return find_entry(sec, key) != NULL;
}

size_t evocore_config_section_size(const evocore_config_t *config,
                                  const char *section) {
    if (!config) return 0;

    /* NULL section means global section (empty string name) */
    const char *sec_name = section ? section : "";
    config_section_t *sec = find_section(config, sec_name);
    if (!sec) return 0;

    return sec->entry_count;
}

const evocore_config_entry_t* evocore_config_get_entry(const evocore_config_t *config,
                                                    const char *section,
                                                    size_t index) {
    if (!config) return NULL;

    /* NULL section means global section (empty string name) */
    const char *sec_name = section ? section : "";
    config_section_t *sec = find_section(config, sec_name);
    if (!sec) return NULL;

    if (index >= sec->entry_count) return NULL;

    config_entry_t *entry = sec->entries;
    for (size_t i = 0; i < index && entry; i++) {
        entry = entry->next;
    }

    if (!entry) return NULL;

    static evocore_config_entry_t static_entry;
    static_entry.key = entry->key;
    static_entry.value = entry->value;
    static_entry.type = EVOCORE_CONFIG_TYPE_STRING;

    return &static_entry;
}
