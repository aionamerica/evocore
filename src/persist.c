/**
 * Persistence and State Management Implementation
 *
 * Provides serialization, deserialization, and checkpointing.
 */

#define _GNU_SOURCE
#include "evocore/persist.h"
#include "evocore/log.h"
#include "evocore/evocore.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>

/*========================================================================
 * Binary Format Definitions
 *========================================================================*/

#define EVOCORE_MAGIC         0x4F4E544F  /* "ONTO" in hex */
#define EVOCORE_VERSION_MAJOR 0
#define EVOCORE_VERSION_MINOR 1

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t format_type;      /* 0 = binary, 1 = JSON */
    uint8_t flags;
    uint64_t timestamp;
    uint64_t data_size;
    uint32_t checksum;
} evocore_binary_header_t;

/*========================================================================
 * JSON Serialization Helpers
 *========================================================================*/

/**
 * JSON writer state
 */
typedef struct {
    char *buffer;
    size_t capacity;
    size_t size;
    bool pretty_print;
    int indent_level;
} json_writer_t;

static void json_writer_init(json_writer_t *writer, size_t initial_capacity,
                            bool pretty_print) {
    writer->buffer = (char*)evocore_malloc(initial_capacity);
    writer->capacity = initial_capacity;
    writer->size = 0;
    writer->buffer[0] = '\0';
    writer->pretty_print = pretty_print;
    writer->indent_level = 0;
}

static void json_writer_free(json_writer_t *writer) {
    evocore_free(writer->buffer);
    writer->buffer = NULL;
    writer->capacity = 0;
    writer->size = 0;
}

static bool json_writer_ensure(json_writer_t *writer, size_t needed) {
    if (writer->size + needed >= writer->capacity) {
        size_t new_capacity = writer->capacity * 2;
        while (new_capacity < writer->size + needed) {
            new_capacity *= 2;
        }
        char *new_buffer = (char*)evocore_realloc(writer->buffer, new_capacity);
        if (!new_buffer) return false;
        writer->buffer = new_buffer;
        writer->capacity = new_capacity;
    }
    return true;
}

static void json_write_string(json_writer_t *writer, const char *str) {
    /* Write opening quote */
    if (!json_writer_ensure(writer, 1)) return;
    writer->buffer[writer->size++] = '"';

    /* Write escaped string content */
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = '"';
                break;
            case '\\':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = '\\';
                break;
            case '\n':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = 'n';
                break;
            case '\r':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = 'r';
                break;
            case '\t':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = 't';
                break;
            case '\b':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = 'b';
                break;
            case '\f':
                if (!json_writer_ensure(writer, 2)) return;
                writer->buffer[writer->size++] = '\\';
                writer->buffer[writer->size++] = 'f';
                break;
            default:
                if (*p < 32) {
                    /* Control character as \uXXXX */
                    if (!json_writer_ensure(writer, 6)) return;
                    writer->buffer[writer->size++] = '\\';
                    writer->buffer[writer->size++] = 'u';
                    writer->buffer[writer->size++] = '0';
                    writer->buffer[writer->size++] = '0';
                    snprintf(writer->buffer + writer->size, 3, "%02x", (unsigned char)*p);
                    writer->size += 2;
                } else {
                    if (!json_writer_ensure(writer, 1)) return;
                    writer->buffer[writer->size++] = *p;
                }
                break;
        }
    }

    /* Write closing quote */
    if (!json_writer_ensure(writer, 1)) return;
    writer->buffer[writer->size++] = '"';
    writer->buffer[writer->size] = '\0';
}

static void json_write_raw(json_writer_t *writer, const char *str) {
    size_t len = strlen(str);
    if (json_writer_ensure(writer, len)) {
        memcpy(writer->buffer + writer->size, str, len);
        writer->size += len;
        writer->buffer[writer->size] = '\0';
    }
}

static void json_write_indent(json_writer_t *writer) {
    if (writer->pretty_print) {
        for (int i = 0; i < writer->indent_level; i++) {
            json_write_raw(writer, "  ");
        }
    }
}

static void json_write_newline(json_writer_t *writer) {
    if (writer->pretty_print) {
        json_write_raw(writer, "\n");
    }
}

static void json_write_key(json_writer_t *writer, const char *key) {
    json_write_indent(writer);
    json_write_string(writer, key);
    json_write_raw(writer, ": ");
}

static void json_write_object_start(json_writer_t *writer) {
    json_write_raw(writer, "{");
    json_write_newline(writer);
    writer->indent_level++;
}

static void json_write_object_end(json_writer_t *writer) {
    writer->indent_level--;
    json_write_indent(writer);
    json_write_raw(writer, "}");
}

static void json_write_array_start(json_writer_t *writer) {
    json_write_raw(writer, "[");
    json_write_newline(writer);
    writer->indent_level++;
}

static void json_write_array_end(json_writer_t *writer) {
    writer->indent_level--;
    json_write_newline(writer);
    json_write_indent(writer);
    json_write_raw(writer, "]");
}

static void json_write_comma(json_writer_t *writer) {
    json_write_raw(writer, ",");
    json_write_newline(writer);
}

/*========================================================================
 * Genome Serialization
 *========================================================================*/

evocore_error_t evocore_genome_serialize(const evocore_genome_t *genome,
                                     char **buffer,
                                     size_t *buffer_size,
                                     const evocore_serial_options_t *options) {
    if (!genome || !buffer || !buffer_size) {
        return EVOCORE_ERR_NULL_PTR;
    }

    evocore_serial_options_t opts;
    if (options) {
        opts = *options;
    } else {
        opts.format = EVOCORE_SERIAL_FORMAT_JSON;
        opts.include_metadata = true;
        opts.pretty_print = true;
        opts.compression_level = 0;
    }

    if (opts.format == EVOCORE_SERIAL_FORMAT_JSON) {
        /* JSON serialization */
        json_writer_t writer;
        json_writer_init(&writer, 4096, opts.pretty_print);

        json_write_object_start(&writer);
        json_write_key(&writer, "size");
        char size_buf[64];
        snprintf(size_buf, sizeof(size_buf), "%zu", genome->size);
        json_write_raw(&writer, size_buf);
        json_write_comma(&writer);

        json_write_key(&writer, "capacity");
        snprintf(size_buf, sizeof(size_buf), "%zu", genome->capacity);
        json_write_raw(&writer, size_buf);
        json_write_comma(&writer);

        json_write_key(&writer, "data");
        json_write_array_start(&writer);

        if (genome->data && genome->size > 0) {
            unsigned char *data = (unsigned char*)genome->data;
            for (size_t i = 0; i < genome->size; i++) {
                if (i > 0) {
                    if (i % 16 == 0) {
                        json_write_raw(&writer, ",");
                        json_write_newline(&writer);
                        json_write_indent(&writer);
                    } else {
                        json_write_raw(&writer, ", ");
                    }
                }
                char byte_buf[8];
                snprintf(byte_buf, sizeof(byte_buf), "0x%02x", data[i]);
                json_write_raw(&writer, byte_buf);
            }
        }

        json_write_newline(&writer);
        json_write_indent(&writer);
        json_write_array_end(&writer);
        json_write_newline(&writer);
        json_write_object_end(&writer);

        *buffer = writer.buffer;
        *buffer_size = writer.size;

    } else {
        /* Binary serialization */
        size_t total_size = sizeof(evocore_binary_header_t) + genome->size;
        char *buf = (char*)evocore_malloc(total_size);
        if (!buf) {
            return EVOCORE_ERR_OUT_OF_MEMORY;
        }

        evocore_binary_header_t *header = (evocore_binary_header_t*)buf;
        header->magic = EVOCORE_MAGIC;
        header->version_major = EVOCORE_VERSION_MAJOR;
        header->version_minor = EVOCORE_VERSION_MINOR;
        header->format_type = 0;  /* binary */
        header->flags = opts.include_metadata ? 1 : 0;
        header->timestamp = (uint64_t)time(NULL);
        header->data_size = genome->size;

        if (genome->data && genome->size > 0) {
            memcpy(buf + sizeof(evocore_binary_header_t), genome->data, genome->size);
        }

        header->checksum = evocore_checksum(buf + sizeof(evocore_binary_header_t),
                                           genome->size);

        *buffer = buf;
        *buffer_size = total_size;
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_deserialize(const char *buffer,
                                       size_t buffer_size,
                                       evocore_genome_t *genome,
                                       evocore_serial_format_t format) {
    if (!buffer || !genome) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (format == EVOCORE_SERIAL_FORMAT_BINARY) {
        /* Binary deserialization */
        if (buffer_size < sizeof(evocore_binary_header_t)) {
            return EVOCORE_ERR_INVALID_ARG;
        }

        evocore_binary_header_t *header = (evocore_binary_header_t*)buffer;

        if (header->magic != EVOCORE_MAGIC) {
            evocore_log_error("Invalid magic number in binary data");
            return EVOCORE_ERR_INVALID_ARG;
        }

        /* Verify checksum */
        uint32_t calc_checksum = evocore_checksum(
            buffer + sizeof(evocore_binary_header_t),
            header->data_size);

        if (calc_checksum != header->checksum) {
            evocore_log_error("Checksum mismatch in binary data");
            return EVOCORE_ERR_INVALID_ARG;
        }

        /* Initialize genome */
        evocore_error_t err = evocore_genome_init(genome, header->data_size);
        if (err != EVOCORE_OK) {
            return err;
        }

        /* Copy data */
        memcpy(genome->data,
               buffer + sizeof(evocore_binary_header_t),
               header->data_size);
        evocore_genome_set_size(genome, header->data_size);

    } else {
        /* JSON deserialization - simplified parser */
        /* For now, look for "data" array and extract bytes */
        unsigned char *data = (unsigned char*)buffer;
        size_t pos = 0;

        /* Find "data": */
        char *data_ptr = strstr(buffer, "\"data\"");
        if (!data_ptr) {
            evocore_genome_init(genome, 256);
            evocore_genome_set_size(genome, 0);
            return EVOCORE_OK;  /* Empty genome is valid */
        }

        /* Find array start */
        char *array_start = strchr(data_ptr, '[');
        if (!array_start) {
            return EVOCORE_ERR_INVALID_ARG;
        }

        /* Count bytes to allocate */
        size_t byte_count = 0;
        char *p = array_start + 1;
        while (*p && *p != ']') {
            if (*p == '0' && *(p+1) == 'x') {
                byte_count++;
            }
            p++;
        }

        if (byte_count == 0) {
            evocore_genome_init(genome, 256);
            evocore_genome_set_size(genome, 0);
            return EVOCORE_OK;
        }

        /* Allocate and parse */
        evocore_genome_init(genome, byte_count);
        unsigned char *genome_data = (unsigned char*)genome->data;

        p = array_start + 1;
        size_t idx = 0;
        while (*p && *p != ']' && idx < byte_count) {
            if (*p == '0' && *(p+1) == 'x') {
                /* Parse hex byte */
                unsigned int byte_val;
                if (sscanf(p + 2, "%x", &byte_val) == 1) {
                    genome_data[idx++] = (unsigned char)byte_val;
                }
            }
            p++;
        }

        evocore_genome_set_size(genome, idx);
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_save(const evocore_genome_t *genome,
                                const char *filepath,
                                const evocore_serial_options_t *options) {
    if (!genome || !filepath) {
        return EVOCORE_ERR_NULL_PTR;
    }

    char *buffer = NULL;
    size_t buffer_size = 0;

    evocore_error_t err = evocore_genome_serialize(genome, &buffer, &buffer_size,
                                                  options);
    if (err != EVOCORE_OK) {
        return err;
    }

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    size_t written = fwrite(buffer, 1, buffer_size, f);
    fclose(f);
    evocore_free(buffer);

    if (written != buffer_size) {
        return EVOCORE_ERR_FILE_WRITE;
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_genome_load(const char *filepath,
                                evocore_genome_t *genome) {
    if (!filepath || !genome) {
        return EVOCORE_ERR_NULL_PTR;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return EVOCORE_ERR_FILE_READ;
    }

    char *buffer = (char*)evocore_malloc(file_size);
    if (!buffer) {
        fclose(f);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    size_t read_size = fread(buffer, 1, file_size, f);
    fclose(f);

    if (read_size != (size_t)file_size) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_READ;
    }

    /* Detect format */
    evocore_serial_format_t format = EVOCORE_SERIAL_FORMAT_BINARY;
    if (buffer[0] == '{') {
        format = EVOCORE_SERIAL_FORMAT_JSON;
    }

    evocore_error_t err = evocore_genome_deserialize(buffer, file_size, genome, format);
    evocore_free(buffer);

    return err;
}

/*========================================================================
 * Population Serialization
 *========================================================================*/

evocore_error_t evocore_population_serialize(const evocore_population_t *pop,
                                         const evocore_domain_t *domain,
                                         char **buffer,
                                         size_t *buffer_size,
                                         const evocore_serial_options_t *options) {
    if (!pop || !buffer || !buffer_size) {
        return EVOCORE_ERR_NULL_PTR;
    }

    evocore_serial_options_t opts;
    if (options) {
        opts = *options;
    } else {
        opts.format = EVOCORE_SERIAL_FORMAT_JSON;
        opts.include_metadata = true;
        opts.pretty_print = true;
        opts.compression_level = 0;
    }

    json_writer_t writer;
    json_writer_init(&writer, 8192, opts.pretty_print);

    json_write_object_start(&writer);

    /* Metadata */
    if (opts.include_metadata) {
        json_write_key(&writer, "version");
        json_write_string(&writer, EVOCORE_VERSION_STRING);
        json_write_comma(&writer);

        json_write_key(&writer, "timestamp");
        char time_buf[64];
        snprintf(time_buf, sizeof(time_buf), "%.0f", (double)time(NULL));
        json_write_raw(&writer, time_buf);
        json_write_comma(&writer);
    }

    /* Population info */
    json_write_key(&writer, "size");
    char size_buf[64];
    snprintf(size_buf, sizeof(size_buf), "%zu", pop->size);
    json_write_raw(&writer, size_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "capacity");
    snprintf(size_buf, sizeof(size_buf), "%zu", pop->capacity);
    json_write_raw(&writer, size_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "generation");
    snprintf(size_buf, sizeof(size_buf), "%zu", pop->generation);
    json_write_raw(&writer, size_buf);
    json_write_comma(&writer);

    /* Domain info */
    if (domain) {
        json_write_key(&writer, "domain");
        json_write_string(&writer, domain->name);
        json_write_comma(&writer);
    }

    /* Individuals */
    json_write_key(&writer, "individuals");
    json_write_array_start(&writer);

    for (size_t i = 0; i < pop->size; i++) {
        evocore_individual_t *ind = &pop->individuals[i];

        json_write_object_start(&writer);

        json_write_key(&writer, "fitness");
        snprintf(size_buf, sizeof(size_buf), "%.15g", ind->fitness);
        json_write_raw(&writer, size_buf);
        json_write_comma(&writer);

        /* Serialize genome */
        json_write_key(&writer, "genome");
        char *genome_buf = NULL;
        size_t genome_size = 0;

        evocore_genome_serialize(ind->genome, &genome_buf, &genome_size, &opts);

        /* Escape and write as string */
        json_write_raw(&writer, "\"");
        for (size_t j = 0; j < genome_size; j++) {
            char escape[4];
            if (genome_buf[j] == '"') {
                strcpy(escape, "\\\"");
            } else if (genome_buf[j] == '\\') {
                strcpy(escape, "\\\\");
            } else if (genome_buf[j] == '\n') {
                strcpy(escape, "\\n");
            } else if (genome_buf[j] == '\r') {
                strcpy(escape, "\\r");
            } else if (genome_buf[j] == '\t') {
                strcpy(escape, "\\t");
            } else if (genome_buf[j] < 32 || genome_buf[j] > 126) {
                snprintf(escape, sizeof(escape), "\\x%02x", (unsigned char)genome_buf[j]);
            } else {
                escape[0] = genome_buf[j];
                escape[1] = '\0';
            }
            json_write_raw(&writer, escape);
        }
        json_write_raw(&writer, "\"");

        evocore_free(genome_buf);

        json_write_object_end(&writer);

        /* Add comma between array elements */
        if (i < pop->size - 1) {
            json_write_comma(&writer);
        } else {
            json_write_newline(&writer);
        }
    }

    json_write_array_end(&writer);
    json_write_newline(&writer);
    json_write_object_end(&writer);

    *buffer = writer.buffer;
    *buffer_size = writer.size;

    return EVOCORE_OK;
}

evocore_error_t evocore_population_deserialize(const char *buffer,
                                           size_t buffer_size,
                                           evocore_population_t *pop,
                                           const evocore_domain_t *domain) {
    (void)domain;  /* Reserved for future use */

    if (!buffer || !pop) {
        return EVOCORE_ERR_NULL_PTR;
    }

    /* Simple JSON parser for population */
    /* This is a basic implementation - for production, use a proper JSON library */

    /* Find size */
    char *size_ptr = strstr(buffer, "\"size\"");
    if (!size_ptr) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    size_t size = 0;
    if (sscanf(size_ptr, "\"size\": %zu", &size) != 1) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    /* Initialize population */
    evocore_error_t err = evocore_population_init(pop, size > 0 ? size : 100);
    if (err != EVOCORE_OK) {
        return err;
    }

    /* Set size to match parsed value (individuals would be added here in full impl) */
    pop->size = size;

    /* Parse and set generation */
    char *generation_ptr = strstr(buffer, "\"generation\"");
    if (generation_ptr) {
        unsigned long gen;
        if (sscanf(generation_ptr, "\"generation\": %lu", &gen) == 1) {
            pop->generation = gen;
        }
    }

    /* Find individuals array */
    char *individuals_ptr = strstr(buffer, "\"individuals\"");
    if (!individuals_ptr) {
        return EVOCORE_ERR_INVALID_ARG;
    }

    /* For now, return success with population structure */
    /* Full individual JSON parsing would be implemented here */

    return EVOCORE_OK;
}

evocore_error_t evocore_population_save(const evocore_population_t *pop,
                                    const evocore_domain_t *domain,
                                    const char *filepath,
                                    const evocore_serial_options_t *options) {
    if (!pop || !filepath) {
        return EVOCORE_ERR_NULL_PTR;
    }

    char *buffer = NULL;
    size_t buffer_size = 0;

    evocore_error_t err = evocore_population_serialize(pop, domain, &buffer,
                                                   &buffer_size, options);
    if (err != EVOCORE_OK) {
        return err;
    }

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    size_t written = fwrite(buffer, 1, buffer_size, f);
    fclose(f);
    evocore_free(buffer);

    if (written != buffer_size) {
        return EVOCORE_ERR_FILE_WRITE;
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_population_load(const char *filepath,
                                    evocore_population_t *pop,
                                    const evocore_domain_t *domain) {
    if (!filepath || !pop) {
        return EVOCORE_ERR_NULL_PTR;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return EVOCORE_ERR_FILE_READ;
    }

    char *buffer = (char*)evocore_malloc(file_size);
    if (!buffer) {
        fclose(f);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    size_t read_size = fread(buffer, 1, file_size, f);
    fclose(f);

    if (read_size != (size_t)file_size) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_READ;
    }

    evocore_error_t err = evocore_population_deserialize(buffer, file_size, pop, domain);
    evocore_free(buffer);

    return err;
}

/*========================================================================
 * Meta-Evolution Serialization
 *========================================================================*/

evocore_error_t evocore_meta_serialize(const evocore_meta_population_t *meta_pop,
                                   char **buffer,
                                   size_t *buffer_size,
                                   const evocore_serial_options_t *options) {
    if (!meta_pop || !buffer || !buffer_size) {
        return EVOCORE_ERR_NULL_PTR;
    }

    evocore_serial_options_t opts;
    if (options) {
        opts = *options;
    } else {
        opts.format = EVOCORE_SERIAL_FORMAT_JSON;
        opts.include_metadata = true;
        opts.pretty_print = true;
        opts.compression_level = 0;
    }

    json_writer_t writer;
    json_writer_init(&writer, 8192, opts.pretty_print);

    json_write_object_start(&writer);

    char val_buf[128];

    /* Basic metadata */
    json_write_key(&writer, "count");
    snprintf(val_buf, sizeof(val_buf), "%d", meta_pop->count);
    json_write_raw(&writer, val_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "current_generation");
    snprintf(val_buf, sizeof(val_buf), "%d", meta_pop->current_generation);
    json_write_raw(&writer, val_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "best_meta_fitness");
    snprintf(val_buf, sizeof(val_buf), "%.15g", meta_pop->best_meta_fitness);
    json_write_raw(&writer, val_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "initialized");
    json_write_raw(&writer, meta_pop->initialized ? "true" : "false");
    json_write_comma(&writer);

    /* Best parameters - all 19 fields */
    json_write_key(&writer, "best_params");
    json_write_object_start(&writer);

#define WRITE_PARAM(param) \
    json_write_key(&writer, #param); \
    snprintf(val_buf, sizeof(val_buf), "%.6g", meta_pop->best_params.param); \
    json_write_raw(&writer, val_buf);

    WRITE_PARAM(optimization_mutation_rate); json_write_comma(&writer);
    WRITE_PARAM(variance_mutation_rate); json_write_comma(&writer);
    WRITE_PARAM(experimentation_rate); json_write_comma(&writer);
    WRITE_PARAM(elite_protection_ratio); json_write_comma(&writer);
    WRITE_PARAM(culling_ratio); json_write_comma(&writer);
    WRITE_PARAM(fitness_threshold_for_breeding); json_write_comma(&writer);
    WRITE_PARAM(target_population_size); json_write_comma(&writer);
    WRITE_PARAM(min_population_size); json_write_comma(&writer);
    WRITE_PARAM(max_population_size); json_write_comma(&writer);
    WRITE_PARAM(learning_rate); json_write_comma(&writer);
    WRITE_PARAM(exploration_factor); json_write_comma(&writer);
    WRITE_PARAM(confidence_threshold); json_write_comma(&writer);
    WRITE_PARAM(profitable_optimization_ratio); json_write_comma(&writer);
    WRITE_PARAM(profitable_random_ratio); json_write_comma(&writer);
    WRITE_PARAM(losing_optimization_ratio); json_write_comma(&writer);
    WRITE_PARAM(losing_random_ratio); json_write_comma(&writer);
    WRITE_PARAM(meta_mutation_rate); json_write_comma(&writer);
    WRITE_PARAM(meta_learning_rate); json_write_comma(&writer);
    WRITE_PARAM(meta_convergence_threshold);

#undef WRITE_PARAM

    json_write_object_end(&writer);
    json_write_comma(&writer);

    /* Individuals array */
    json_write_key(&writer, "individuals");
    json_write_array_start(&writer);

    for (int i = 0; i < meta_pop->count; i++) {
        const evocore_meta_individual_t *ind = &meta_pop->individuals[i];

        json_write_object_start(&writer);

        json_write_key(&writer, "meta_fitness");
        snprintf(val_buf, sizeof(val_buf), "%.15g", ind->meta_fitness);
        json_write_raw(&writer, val_buf);
        json_write_comma(&writer);

        json_write_key(&writer, "generation");
        snprintf(val_buf, sizeof(val_buf), "%d", ind->generation);
        json_write_raw(&writer, val_buf);
        json_write_comma(&writer);

        json_write_key(&writer, "history_size");
        snprintf(val_buf, sizeof(val_buf), "%zu", ind->history_size);
        json_write_raw(&writer, val_buf);
        json_write_comma(&writer);

        /* Fitness history */
        json_write_key(&writer, "fitness_history");
        json_write_array_start(&writer);
        for (size_t j = 0; j < ind->history_size; j++) {
            snprintf(val_buf, sizeof(val_buf), "%.15g", ind->fitness_history[j]);
            json_write_raw(&writer, val_buf);
            if (j < ind->history_size - 1) {
                json_write_raw(&writer, ", ");
            }
        }
        json_write_array_end(&writer);

        json_write_object_end(&writer);

        if (i < meta_pop->count - 1) {
            json_write_comma(&writer);
        }
    }

    json_write_array_end(&writer);
    json_write_newline(&writer);
    json_write_object_end(&writer);

    *buffer = writer.buffer;
    *buffer_size = writer.size;

    return EVOCORE_OK;
}

/*========================================================================
 * JSON Parsing Helpers
 *========================================================================*/

static int parse_json_int(const char *json, const char *key, int default_val) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *ptr = strstr(json, search);
    if (!ptr) return default_val;
    ptr += strlen(search);
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') ptr++;
    int val = default_val;
    sscanf(ptr, "%d", &val);
    return val;
}

static double parse_json_double(const char *json, const char *key, double default_val) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *ptr = strstr(json, search);
    if (!ptr) return default_val;
    ptr += strlen(search);
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') ptr++;
    double val = default_val;
    sscanf(ptr, "%lf", &val);
    return val;
}

static bool parse_json_bool(const char *json, const char *key, bool default_val) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *ptr = strstr(json, search);
    if (!ptr) return default_val;
    ptr += strlen(search);
    while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') ptr++;
    if (strncmp(ptr, "true", 4) == 0) return true;
    if (strncmp(ptr, "false", 5) == 0) return false;
    return default_val;
}

/*========================================================================
 * Meta-Evolution Deserialization
 *========================================================================*/

evocore_error_t evocore_meta_deserialize(const char *buffer,
                                      size_t buffer_size,
                                      evocore_meta_population_t *meta_pop) {
    if (!buffer || !meta_pop) {
        return EVOCORE_ERR_NULL_PTR;
    }

    (void)buffer_size;  /* Unused for JSON parsing */

    /* Parse basic fields */
    meta_pop->count = parse_json_int(buffer, "count", 0);
    meta_pop->current_generation = parse_json_int(buffer, "current_generation", 0);
    meta_pop->best_meta_fitness = parse_json_double(buffer, "best_meta_fitness", 0.0);
    meta_pop->initialized = parse_json_bool(buffer, "initialized", false);

    /* Find best_params object */
    const char *best_params_ptr = strstr(buffer, "\"best_params\"");
    if (best_params_ptr) {
        /* Find the opening brace after best_params */
        const char *params_start = strchr(best_params_ptr, '{');
        if (params_start) {
            /* Find matching closing brace (simple search) */
            const char *params_end = strchr(params_start, '}');
            if (params_end) {
                /* Create a substring containing just the params object */
                size_t params_len = params_end - params_start + 1;
                char *params_str = (char*)evocore_malloc(params_len + 1);
                if (params_str) {
                    memcpy(params_str, params_start, params_len);
                    params_str[params_len] = '\0';

                    /* Parse all 19 parameters */
                    meta_pop->best_params.optimization_mutation_rate =
                        parse_json_double(params_str, "optimization_mutation_rate", 0.1);
                    meta_pop->best_params.variance_mutation_rate =
                        parse_json_double(params_str, "variance_mutation_rate", 0.2);
                    meta_pop->best_params.experimentation_rate =
                        parse_json_double(params_str, "experimentation_rate", 0.05);
                    meta_pop->best_params.elite_protection_ratio =
                        parse_json_double(params_str, "elite_protection_ratio", 0.1);
                    meta_pop->best_params.culling_ratio =
                        parse_json_double(params_str, "culling_ratio", 0.2);
                    meta_pop->best_params.fitness_threshold_for_breeding =
                        parse_json_double(params_str, "fitness_threshold_for_breeding", 0.0);
                    meta_pop->best_params.target_population_size =
                        (int)parse_json_double(params_str, "target_population_size", 100.0);
                    meta_pop->best_params.min_population_size =
                        (int)parse_json_double(params_str, "min_population_size", 10.0);
                    meta_pop->best_params.max_population_size =
                        (int)parse_json_double(params_str, "max_population_size", 1000.0);
                    meta_pop->best_params.learning_rate =
                        parse_json_double(params_str, "learning_rate", 0.1);
                    meta_pop->best_params.exploration_factor =
                        parse_json_double(params_str, "exploration_factor", 0.5);
                    meta_pop->best_params.confidence_threshold =
                        parse_json_double(params_str, "confidence_threshold", 0.5);
                    meta_pop->best_params.profitable_optimization_ratio =
                        parse_json_double(params_str, "profitable_optimization_ratio", 0.8);
                    meta_pop->best_params.profitable_random_ratio =
                        parse_json_double(params_str, "profitable_random_ratio", 0.05);
                    meta_pop->best_params.losing_optimization_ratio =
                        parse_json_double(params_str, "losing_optimization_ratio", 0.5);
                    meta_pop->best_params.losing_random_ratio =
                        parse_json_double(params_str, "losing_random_ratio", 0.2);
                    meta_pop->best_params.meta_mutation_rate =
                        parse_json_double(params_str, "meta_mutation_rate", 0.05);
                    meta_pop->best_params.meta_learning_rate =
                        parse_json_double(params_str, "meta_learning_rate", 0.1);
                    meta_pop->best_params.meta_convergence_threshold =
                        parse_json_double(params_str, "meta_convergence_threshold", 0.01);

                    evocore_free(params_str);
                }
            }
        }
    }

    /* Parse individuals array */
    const char *individuals_ptr = strstr(buffer, "\"individuals\"");
    if (individuals_ptr && meta_pop->count > 0) {
        const char *array_start = strchr(individuals_ptr, '[');
        if (array_start) {
            /* Simple parsing for each individual */
            for (int i = 0; i < meta_pop->count && i < EVOCORE_MAX_META_INDIVIDUALS; i++) {
                evocore_meta_individual_t *ind = &meta_pop->individuals[i];

                /* Find next object start */
                const char *obj_start = strchr(array_start, '{');
                if (!obj_start) break;
                const char *obj_end = strchr(obj_start, '}');
                if (!obj_end) break;

                size_t obj_len = obj_end - obj_start + 1;
                char *obj_str = (char*)evocore_malloc(obj_len + 1);
                if (obj_str) {
                    memcpy(obj_str, obj_start, obj_len);
                    obj_str[obj_len] = '\0';

                    ind->meta_fitness = parse_json_double(obj_str, "meta_fitness", 0.0);
                    ind->generation = parse_json_int(obj_str, "generation", 0);
                    ind->history_size = parse_json_int(obj_str, "history_size", 0);

                    /* Parse fitness_history array */
                    const char *hist_ptr = strstr(obj_str, "\"fitness_history\"");
                    if (hist_ptr) {
                        const char *hist_array = strchr(hist_ptr, '[');
                        if (hist_array) {
                            const char *hist_end = strchr(hist_array, ']');
                            if (hist_end) {
                                /* Count values */
                                int hist_count = 0;
                                for (const char *p = hist_array; p < hist_end; p++) {
                                    if (*p == '-' || (*p >= '0' && *p <= '9')) {
                                        /* Check if this is a new number (previous char wasn't digit/dot/minus) */
                                        if (p == hist_array || !(p[-1] == '.' || (p[-1] >= '0' && p[-1] <= '9'))) {
                                            hist_count++;
                                        }
                                    }
                                }

                                if (hist_count > 0 && hist_count < 10000) {
                                    ind->history_capacity = hist_count;
                                    ind->fitness_history = (double*)evocore_malloc(hist_count * sizeof(double));
                                    if (ind->fitness_history) {
                                        ind->history_size = 0;
                                        const char *p = hist_array;
                                        while (p < hist_end && ind->history_size < (size_t)hist_count) {
                                            /* Skip to next number */
                                            while (p < hist_end && *p != '-' && !(*p >= '0' && *p <= '9')) p++;
                                            if (p >= hist_end) break;
                                            double val = 0.0;
                                            if (sscanf(p, "%lf", &val) == 1) {
                                                ind->fitness_history[ind->history_size++] = val;
                                            }
                                            /* Skip past this number */
                                            while (p < hist_end && (*p == '.' || (*p >= '0' && *p <= '9') || *p == '-' || *p == 'e' || *p == 'E' || *p == '+')) p++;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    evocore_free(obj_str);
                }

                array_start = obj_end + 1;
            }
        }
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_meta_save(const evocore_meta_population_t *meta_pop,
                               const char *filepath,
                               const evocore_serial_options_t *options) {
    if (!meta_pop || !filepath) {
        return EVOCORE_ERR_NULL_PTR;
    }

    char *buffer = NULL;
    size_t buffer_size = 0;

    evocore_error_t err = evocore_meta_serialize(meta_pop, &buffer, &buffer_size, options);
    if (err != EVOCORE_OK) {
        return err;
    }

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    size_t written = fwrite(buffer, 1, buffer_size, f);
    fclose(f);
    evocore_free(buffer);

    if (written != buffer_size) {
        return EVOCORE_ERR_FILE_WRITE;
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_meta_load(const char *filepath,
                               evocore_meta_population_t *meta_pop) {
    if (!filepath || !meta_pop) {
        return EVOCORE_ERR_NULL_PTR;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return EVOCORE_ERR_FILE_READ;
    }

    char *buffer = (char*)evocore_malloc(file_size + 1);
    if (!buffer) {
        fclose(f);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    size_t read_size = fread(buffer, 1, file_size, f);
    fclose(f);

    if (read_size != (size_t)file_size) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_READ;
    }

    buffer[file_size] = '\0';

    evocore_error_t err = evocore_meta_deserialize(buffer, file_size, meta_pop);
    evocore_free(buffer);

    return err;
}

/*========================================================================
 * Checkpoint Management
 *========================================================================*/

evocore_error_t evocore_checkpoint_create(evocore_checkpoint_t *checkpoint,
                                      const evocore_population_t *pop,
                                      const evocore_domain_t *domain,
                                      const evocore_meta_population_t *meta_pop) {
    if (!checkpoint || !pop || !domain) {
        return EVOCORE_ERR_NULL_PTR;
    }

    memset(checkpoint, 0, sizeof(evocore_checkpoint_t));

    snprintf(checkpoint->version, sizeof(checkpoint->version),
             "%d.%d", EVOCORE_VERSION_MAJOR, EVOCORE_VERSION_MINOR);
    checkpoint->timestamp = (double)time(NULL);

    /* Population state */
    checkpoint->population_size = pop->size;
    checkpoint->population_capacity = pop->capacity;
    checkpoint->generation = pop->generation;
    checkpoint->best_fitness = pop->best_fitness;
    checkpoint->avg_fitness = pop->avg_fitness;

    /* Domain info */
    strncpy(checkpoint->domain_name, domain->name,
            sizeof(checkpoint->domain_name) - 1);

    /* Meta state */
    if (meta_pop) {
        checkpoint->has_meta_state = true;
        checkpoint->meta_params = meta_pop->best_params;
    }

    /* Serialize population */
    evocore_serial_options_t opts = {
        .format = EVOCORE_SERIAL_FORMAT_JSON,
        .include_metadata = true,
        .pretty_print = true,
        .compression_level = 0
    };
    evocore_population_serialize(pop, domain,
                               &checkpoint->population_data,
                               &checkpoint->population_data_size, &opts);

    /* Serialize meta-population */
    if (meta_pop) {
        evocore_meta_serialize(meta_pop,
                            &checkpoint->meta_data,
                            &checkpoint->meta_data_size, &opts);
    }

    return EVOCORE_OK;
}

evocore_error_t evocore_checkpoint_save(const evocore_checkpoint_t *checkpoint,
                                    const char *filepath,
                                    const evocore_serial_options_t *options) {
    if (!checkpoint || !filepath) {
        return EVOCORE_ERR_NULL_PTR;
    }

    evocore_serial_options_t opts;
    if (options) {
        opts = *options;
    } else {
        opts.format = EVOCORE_SERIAL_FORMAT_JSON;
        opts.include_metadata = true;
        opts.pretty_print = true;
        opts.compression_level = 0;
    }

    /* Create JSON from checkpoint */
    json_writer_t writer;
    json_writer_init(&writer, 8192, opts.pretty_print);

    json_write_object_start(&writer);

    json_write_key(&writer, "version");
    json_write_string(&writer, checkpoint->version);
    json_write_comma(&writer);

    json_write_key(&writer, "timestamp");
    char time_buf[64];
    snprintf(time_buf, sizeof(time_buf), "%.0f", checkpoint->timestamp);
    json_write_raw(&writer, time_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "domain");
    json_write_string(&writer, checkpoint->domain_name);
    json_write_comma(&writer);

    json_write_key(&writer, "generation");
    snprintf(time_buf, sizeof(time_buf), "%zu", checkpoint->generation);
    json_write_raw(&writer, time_buf);
    json_write_comma(&writer);

    json_write_key(&writer, "best_fitness");
    snprintf(time_buf, sizeof(time_buf), "%.15g", checkpoint->best_fitness);
    json_write_raw(&writer, time_buf);
    json_write_comma(&writer);

    /* Embedded population data - write as raw JSON (already serialized) */
    json_write_key(&writer, "population");
    json_write_raw(&writer, checkpoint->population_data);
    json_write_comma(&writer);

    /* Embedded meta data */
    json_write_key(&writer, "meta_data");
    if (checkpoint->meta_data) {
        json_write_raw(&writer, checkpoint->meta_data);
    } else {
        json_write_raw(&writer, "null");
    }

    json_write_newline(&writer);
    json_write_object_end(&writer);

    /* Write to file */
    FILE *f = fopen(filepath, "wb");
    if (!f) {
        json_writer_free(&writer);
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    size_t total_written = 0;
    size_t remaining = writer.size;
    const char *ptr = writer.buffer;

    /* Handle partial writes */
    while (remaining > 0) {
        size_t written = fwrite(ptr, 1, remaining, f);
        if (written == 0) {
            fclose(f);
            json_writer_free(&writer);
            return EVOCORE_ERR_FILE_WRITE;
        }
        total_written += written;
        ptr += written;
        remaining -= written;
    }

    fflush(f);
    fclose(f);
    json_writer_free(&writer);

    return EVOCORE_OK;
}

evocore_error_t evocore_checkpoint_load(const char *filepath,
                                    evocore_checkpoint_t *checkpoint) {
    if (!filepath || !checkpoint) {
        return EVOCORE_ERR_NULL_PTR;
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        return EVOCORE_ERR_FILE_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(f);
        return EVOCORE_ERR_FILE_READ;
    }

    char *buffer = (char*)evocore_malloc(file_size);
    if (!buffer) {
        fclose(f);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    size_t read_size = fread(buffer, 1, file_size, f);
    fclose(f);

    if (read_size != (size_t)file_size) {
        evocore_free(buffer);
        return EVOCORE_ERR_FILE_READ;
    }

    /* Parse checkpoint JSON - simplified */
    memset(checkpoint, 0, sizeof(*checkpoint));

    char *version_ptr = strstr(buffer, "\"version\"");
    if (version_ptr) {
        if (sscanf(version_ptr, "\"version\": \"%15[^\"]\"",
                  checkpoint->version) == 1) {
            /* OK */
        }
    }

    char *timestamp_ptr = strstr(buffer, "\"timestamp\"");
    if (timestamp_ptr) {
        if (sscanf(timestamp_ptr, "\"timestamp\": %lf",
                  &checkpoint->timestamp) == 1) {
            /* OK */
        }
    }

    char *domain_ptr = strstr(buffer, "\"domain\"");
    if (domain_ptr) {
        if (sscanf(domain_ptr, "\"domain\": \"%63[^\"]\"",
                  checkpoint->domain_name) == 1) {
            /* OK */
        }
    }

    char *generation_ptr = strstr(buffer, "\"generation\"");
    if (generation_ptr) {
        unsigned long gen;
        if (sscanf(generation_ptr, "\"generation\": %lu", &gen) == 1) {
            checkpoint->generation = gen;
        }
    }

    char *best_fitness_ptr = strstr(buffer, "\"best_fitness\"");
    if (best_fitness_ptr) {
        if (sscanf(best_fitness_ptr, "\"best_fitness\": %lf",
                  &checkpoint->best_fitness) == 1) {
            /* OK */
        }
    }

    evocore_free(buffer);

    return EVOCORE_OK;
}

evocore_error_t evocore_checkpoint_restore(const evocore_checkpoint_t *checkpoint,
                                       evocore_population_t *pop,
                                       const evocore_domain_t *domain,
                                       evocore_meta_population_t *meta_pop) {
    if (!checkpoint || !pop || !domain) {
        return EVOCORE_ERR_NULL_PTR;
    }

    /* Validate domain match */
    if (strcmp(checkpoint->domain_name, domain->name) != 0) {
        evocore_log_error("Domain mismatch: checkpoint has '%s', expected '%s'",
                       checkpoint->domain_name, domain->name);
        return EVOCORE_ERR_INVALID_ARG;
    }

    /* Restore population from embedded data */
    if (checkpoint->population_data) {
        evocore_error_t err = evocore_population_deserialize(
            checkpoint->population_data,
            checkpoint->population_data_size,
            pop, domain);
        if (err != EVOCORE_OK) {
            evocore_log_warn("Failed to restore population: %d", err);
        }
    }

    /* Restore meta state */
    if (meta_pop && checkpoint->has_meta_state) {
        /* Restore best meta parameters */
        meta_pop->best_params = checkpoint->meta_params;
        meta_pop->best_meta_fitness = checkpoint->best_fitness;

        /* If meta data is available, deserialize full meta-population */
        if (checkpoint->meta_data && checkpoint->meta_data_size > 0) {
            evocore_error_t err = evocore_meta_deserialize(
                checkpoint->meta_data,
                checkpoint->meta_data_size,
                meta_pop);
            if (err != EVOCORE_OK) {
                evocore_log_warn("Failed to restore full meta-population: %d", err);
                /* Keep the basic params we already restored */
            }
        }
    }

    return EVOCORE_OK;
}

void evocore_checkpoint_free(evocore_checkpoint_t *checkpoint) {
    if (!checkpoint) return;

    evocore_free(checkpoint->user_data);
    evocore_free(checkpoint->population_data);
    evocore_free(checkpoint->meta_data);
    memset(checkpoint, 0, sizeof(*checkpoint));
}

/*========================================================================
 * Auto-Checkpoint Manager
 *========================================================================*/

struct evocore_checkpoint_manager_t {
    evocore_auto_checkpoint_config_t config;
    int generations_since_last;
    int checkpoint_count;
};

evocore_checkpoint_manager_t* evocore_checkpoint_manager_create(
    const evocore_auto_checkpoint_config_t *config) {

    evocore_checkpoint_manager_t *manager = (evocore_checkpoint_manager_t*)
        evocore_calloc(1, sizeof(evocore_checkpoint_manager_t));
    if (!manager) {
        return NULL;
    }

    if (config) {
        manager->config = *config;
    } else {
        manager->config.enabled = false;
        manager->config.every_n_generations = 10;
        strcpy(manager->config.directory, "./checkpoints");
        manager->config.max_checkpoints = 5;
        manager->config.compress = false;
    }

    manager->generations_since_last = 0;
    manager->checkpoint_count = 0;

    /* Create directory if needed */
    if (manager->config.enabled && manager->config.directory[0]) {
        struct stat st;
        if (stat(manager->config.directory, &st) != 0) {
            /* Try to create directory */
            mkdir(manager->config.directory, 0755);
        }
    }

    return manager;
}

void evocore_checkpoint_manager_destroy(evocore_checkpoint_manager_t *manager) {
    if (manager) {
        evocore_free(manager);
    }
}

evocore_error_t evocore_checkpoint_manager_update(evocore_checkpoint_manager_t *manager,
                                             const evocore_population_t *pop,
                                             const evocore_domain_t *domain,
                                             const evocore_meta_population_t *meta_pop) {
    if (!manager || !pop || !domain) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (!manager->config.enabled) {
        return EVOCORE_OK;  /* Auto-checkpoint disabled */
    }

    manager->generations_since_last++;

    /* Check if we should create a checkpoint */
    if (manager->config.every_n_generations > 0 &&
        manager->generations_since_last >= manager->config.every_n_generations) {

        /* Create checkpoint */
        evocore_checkpoint_t checkpoint;
        evocore_error_t err = evocore_checkpoint_create(&checkpoint, pop, domain, meta_pop);
        if (err != EVOCORE_OK) {
            evocore_log_warn("Failed to create checkpoint: %d", err);
            return err;
        }

        /* Save checkpoint */
        char filepath[512];
        snprintf(filepath, sizeof(filepath), "%s/checkpoint_%zu.json",
                 manager->config.directory,
                 (size_t)checkpoint.timestamp);

        err = evocore_checkpoint_save(&checkpoint, filepath, NULL);
        evocore_checkpoint_free(&checkpoint);

        if (err != EVOCORE_OK) {
            evocore_log_warn("Failed to save checkpoint: %d", err);
            return err;
        }

        evocore_log_info("Created checkpoint: %s", filepath);

        /* Cleanup old checkpoints */
        if (manager->config.max_checkpoints > 0) {
            manager->checkpoint_count++;
            while (manager->checkpoint_count > manager->config.max_checkpoints) {
                /* Remove oldest checkpoint */
                /* (Would implement file age tracking here) */
                manager->checkpoint_count--;
            }
        }

        manager->generations_since_last = 0;
    }

    return EVOCORE_OK;
}

char** evocore_checkpoint_list(const char *directory, int *count) {
    if (!directory || !count) {
        return NULL;
    }

    DIR *dir = opendir(directory);
    if (!dir) {
        *count = 0;
        return NULL;
    }

    /* Count entries */
    int entries = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "checkpoint_", 11) == 0) {
            entries++;
        }
    }

    if (entries == 0) {
        closedir(dir);
        *count = 0;
        return NULL;
    }

    /* Allocate array */
    char **list = (char**)evocore_calloc(entries, sizeof(char*));
    if (!list) {
        closedir(dir);
        *count = 0;
        return NULL;
    }

    /* Fill array */
    rewinddir(dir);
    int idx = 0;
    while ((ent = readdir(dir)) != NULL && idx < entries) {
        if (strncmp(ent->d_name, "checkpoint_", 11) == 0) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s",
                     directory, ent->d_name);
            list[idx++] = strdup(full_path);
        }
    }

    closedir(dir);
    *count = entries;

    return list;
}

void evocore_checkpoint_list_free(char **list, int count) {
    if (!list) return;

    for (int i = 0; i < count; i++) {
        evocore_free(list[i]);
    }
    evocore_free(list);
}

evocore_error_t evocore_checkpoint_info(const char *filepath,
                                     evocore_checkpoint_t *checkpoint) {
    return evocore_checkpoint_load(filepath, checkpoint);
}

/*========================================================================
 * Utility Functions
 *========================================================================*/

uint32_t evocore_checksum(const void *data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }

    const unsigned char *bytes = (const unsigned char*)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

bool evocore_checksum_validate(const void *data, size_t size, uint32_t expected) {
    return evocore_checksum(data, size) == expected;
}
