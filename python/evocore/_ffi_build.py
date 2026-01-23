"""
cffi build configuration for evocore Python bindings.

This module declares all C types and functions from the evocore library.
Run this module directly to compile the cffi extension:
    python -m evocore._ffi_build
"""

import os
from cffi import FFI

ffi = FFI()

# =============================================================================
# C Declarations (cdef)
# =============================================================================

ffi.cdef("""
// ==========================================================================
// Basic Types
// ==========================================================================

typedef long time_t;
typedef unsigned int uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

// ==========================================================================
// Error Handling (error.h)
// ==========================================================================

typedef enum {
    // Success codes
    EVOCORE_OK = 0,
    EVOCORE_SUCCESS_CONVERGED = 1,
    EVOCORE_SUCCESS_MAX_GEN = 2,

    // General errors
    EVOCORE_ERR_UNKNOWN = -1,
    EVOCORE_ERR_NULL_PTR = -2,
    EVOCORE_ERR_OUT_OF_MEMORY = -3,
    EVOCORE_ERR_INVALID_ARG = -4,
    EVOCORE_ERR_NOT_IMPLEMENTED = -5,

    // Genome errors
    EVOCORE_ERR_GENOME_EMPTY = -10,
    EVOCORE_ERR_GENOME_TOO_LARGE = -11,
    EVOCORE_ERR_GENOME_INVALID = -12,

    // Population errors
    EVOCORE_ERR_POP_EMPTY = -20,
    EVOCORE_ERR_POP_FULL = -21,
    EVOCORE_ERR_POP_SIZE = -22,

    // Config errors
    EVOCORE_ERR_CONFIG_NOT_FOUND = -30,
    EVOCORE_ERR_CONFIG_PARSE = -31,
    EVOCORE_ERR_CONFIG_INVALID = -32,

    // File errors
    EVOCORE_ERR_FILE_NOT_FOUND = -40,
    EVOCORE_ERR_FILE_READ = -41,
    EVOCORE_ERR_FILE_WRITE = -42
} evocore_error_t;

const char* evocore_error_string(evocore_error_t err);

// ==========================================================================
// Logging (log.h)
// ==========================================================================

typedef enum {
    EVOCORE_LOG_TRACE = 0,
    EVOCORE_LOG_DEBUG = 1,
    EVOCORE_LOG_INFO = 2,
    EVOCORE_LOG_WARN = 3,
    EVOCORE_LOG_ERROR = 4,
    EVOCORE_LOG_FATAL = 5
} evocore_log_level_t;

void evocore_log_set_level(evocore_log_level_t level);
bool evocore_log_set_file(bool enabled, const char *path);
void evocore_log_set_color(bool enabled);
evocore_log_level_t evocore_log_get_level(void);
void evocore_log_close(void);

// ==========================================================================
// Memory Management (memory.h, arena.h)
// ==========================================================================

typedef struct {
    size_t total_allocated;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
} evocore_memory_stats_t;

void evocore_memory_get_stats(evocore_memory_stats_t *stats);
void evocore_memory_reset_stats(void);
void evocore_memory_set_tracking(bool enable);
size_t evocore_memory_check_leaks(void);
void evocore_memory_dump_stats(void);

// Arena allocator
typedef struct {
    void *buffer;
    size_t capacity;
    size_t offset;
    size_t alignment;
} evocore_arena_t;

evocore_error_t evocore_arena_init(evocore_arena_t *arena, size_t capacity);
evocore_error_t evocore_arena_init_with_buffer(evocore_arena_t *arena, void *buffer, size_t capacity);
void evocore_arena_cleanup(evocore_arena_t *arena);
void* evocore_arena_alloc(evocore_arena_t *arena, size_t size);
void* evocore_arena_calloc(evocore_arena_t *arena, size_t size);
void* evocore_arena_alloc_array(evocore_arena_t *arena, size_t num, size_t size);
void evocore_arena_reset(evocore_arena_t *arena);
size_t evocore_arena_remaining(const evocore_arena_t *arena);
size_t evocore_arena_used(const evocore_arena_t *arena);
bool evocore_arena_can_alloc(const evocore_arena_t *arena, size_t size);
size_t evocore_arena_snapshot(evocore_arena_t *arena);
void evocore_arena_rewind(evocore_arena_t *arena, size_t offset);

// ==========================================================================
// Genome (genome.h)
// ==========================================================================

typedef struct {
    void *data;
    size_t size;
    size_t capacity;
    bool owns_memory;
} evocore_genome_t;

typedef struct {
    evocore_genome_t genome;
    double fitness;
} evocore_individual_t;

// Lifecycle
evocore_error_t evocore_genome_init(evocore_genome_t *genome, size_t capacity);
evocore_error_t evocore_genome_from_data(evocore_genome_t *genome, const void *data, size_t size);
evocore_error_t evocore_genome_view(evocore_genome_t *genome, const void *data, size_t size);
void evocore_genome_cleanup(evocore_genome_t *genome);
evocore_error_t evocore_genome_clone(const evocore_genome_t *src, evocore_genome_t *dst);

// Manipulation
evocore_error_t evocore_genome_resize(evocore_genome_t *genome, size_t new_capacity);
evocore_error_t evocore_genome_set_size(evocore_genome_t *genome, size_t size);
evocore_error_t evocore_genome_write(evocore_genome_t *genome, size_t offset, const void *data, size_t size);
evocore_error_t evocore_genome_read(const evocore_genome_t *genome, size_t offset, void *data, size_t size);

// Utilities
evocore_error_t evocore_genome_distance(const evocore_genome_t *a, const evocore_genome_t *b, size_t *distance);
evocore_error_t evocore_genome_zero(evocore_genome_t *genome);
evocore_error_t evocore_genome_randomize(evocore_genome_t *genome);
bool evocore_genome_is_valid(const evocore_genome_t *genome);
size_t evocore_genome_get_size(const evocore_genome_t *genome);
size_t evocore_genome_get_capacity(const evocore_genome_t *genome);
void* evocore_genome_get_data(const evocore_genome_t *genome);

// Evolution operations
evocore_error_t evocore_genome_crossover(const evocore_genome_t *parent1, const evocore_genome_t *parent2,
                                         evocore_genome_t *child1, evocore_genome_t *child2, unsigned int *seed);
evocore_error_t evocore_genome_mutate(evocore_genome_t *genome, double rate, unsigned int *seed);

// ==========================================================================
// Fitness (fitness.h)
// ==========================================================================

typedef double (*evocore_fitness_func_t)(const evocore_genome_t *genome, void *context);

// ==========================================================================
// Population (population.h)
// ==========================================================================

typedef struct {
    evocore_individual_t *individuals;
    size_t size;
    size_t capacity;
    size_t generation;
    double best_fitness;
    double avg_fitness;
    double worst_fitness;
    size_t best_index;
} evocore_population_t;

// Lifecycle
evocore_error_t evocore_population_init(evocore_population_t *pop, size_t capacity);
void evocore_population_cleanup(evocore_population_t *pop);
void evocore_population_clear(evocore_population_t *pop);

// Manipulation
evocore_error_t evocore_population_add(evocore_population_t *pop, const evocore_genome_t *genome, double fitness);
evocore_error_t evocore_population_remove(evocore_population_t *pop, size_t index);
evocore_error_t evocore_population_resize(evocore_population_t *pop, size_t new_capacity);
evocore_error_t evocore_population_set_size(evocore_population_t *pop, size_t size);

// Queries
evocore_individual_t* evocore_population_get(evocore_population_t *pop, size_t index);
evocore_individual_t* evocore_population_get_best(evocore_population_t *pop);
size_t evocore_population_size(const evocore_population_t *pop);
size_t evocore_population_capacity(const evocore_population_t *pop);
size_t evocore_population_generation(const evocore_population_t *pop);
void evocore_population_increment_generation(evocore_population_t *pop);

// Statistics and evolution
evocore_error_t evocore_population_update_stats(evocore_population_t *pop);
evocore_error_t evocore_population_sort(evocore_population_t *pop);
size_t evocore_population_tournament_select(const evocore_population_t *pop, size_t tournament_size, unsigned int *seed);
evocore_error_t evocore_population_truncate(evocore_population_t *pop, size_t n);
size_t evocore_population_evaluate(evocore_population_t *pop, evocore_fitness_func_t fitness_func, void *context);

// ==========================================================================
// Domain System (domain.h)
// ==========================================================================

typedef struct {
    evocore_error_t (*random_init)(evocore_genome_t *genome, unsigned int *seed);
    evocore_error_t (*mutate)(evocore_genome_t *genome, double rate, unsigned int *seed);
    evocore_error_t (*crossover)(const evocore_genome_t *p1, const evocore_genome_t *p2,
                                  evocore_genome_t *c1, evocore_genome_t *c2, unsigned int *seed);
    double (*diversity)(const evocore_genome_t *a, const evocore_genome_t *b);
} evocore_genome_ops_t;

typedef struct {
    const char *name;
    const char *version;
    size_t genome_size;
    evocore_genome_ops_t genome_ops;
    evocore_fitness_func_t fitness;
    void *user_context;
    // Callbacks
    evocore_error_t (*serialize)(const evocore_genome_t *genome, char **buffer, size_t *size);
    evocore_error_t (*deserialize)(const char *buffer, size_t size, evocore_genome_t *genome);
    void (*print_stats)(const evocore_population_t *pop);
} evocore_domain_t;

evocore_error_t evocore_domain_registry_init(void);
void evocore_domain_registry_shutdown(void);
evocore_error_t evocore_register_domain(const evocore_domain_t *domain);
evocore_error_t evocore_unregister_domain(const char *name);
const evocore_domain_t* evocore_get_domain(const char *name);
bool evocore_has_domain(const char *name);
int evocore_domain_count(void);
const char* evocore_domain_name(int index);
evocore_error_t evocore_domain_create_genome(const char *domain_name, evocore_genome_t *genome);
void evocore_domain_mutate_genome(evocore_genome_t *genome, const evocore_domain_t *domain, double rate);
double evocore_domain_evaluate_fitness(const evocore_genome_t *genome, const evocore_domain_t *domain);
double evocore_domain_diversity(const evocore_genome_t *a, const evocore_genome_t *b, const evocore_domain_t *domain);

// ==========================================================================
// Weighted Statistics (weighted.h)
// ==========================================================================

typedef struct {
    double mean;
    double variance;
    double sum_weights;
    double m2;
    size_t count;
    double min_value;
    double max_value;
    double sum_weighted_x;
} evocore_weighted_stats_t;

typedef struct {
    evocore_weighted_stats_t *stats;
    size_t count;
} evocore_weighted_array_t;

// Single value operations
void evocore_weighted_init(evocore_weighted_stats_t *stats);
bool evocore_weighted_update(evocore_weighted_stats_t *stats, double value, double weight);
double evocore_weighted_mean(const evocore_weighted_stats_t *stats);
double evocore_weighted_std(const evocore_weighted_stats_t *stats);
double evocore_weighted_variance(const evocore_weighted_stats_t *stats);
double evocore_weighted_sample(const evocore_weighted_stats_t *stats, unsigned int *seed);
void evocore_weighted_reset(evocore_weighted_stats_t *stats);
bool evocore_weighted_has_data(const evocore_weighted_stats_t *stats, size_t min_samples);
double evocore_weighted_confidence(const evocore_weighted_stats_t *stats, size_t max_samples);

// Array operations
evocore_weighted_array_t* evocore_weighted_array_create(size_t count);
void evocore_weighted_array_free(evocore_weighted_array_t *array);
bool evocore_weighted_array_update(evocore_weighted_array_t *array, const double *values,
                                    const double *weights, size_t count, double global_weight);
bool evocore_weighted_array_get_means(const evocore_weighted_array_t *array, double *out_means, size_t count);
bool evocore_weighted_array_get_stds(const evocore_weighted_array_t *array, double *out_stds, size_t count);
bool evocore_weighted_array_sample(const evocore_weighted_array_t *array, double *out_values, size_t count,
                                    double exploration_factor, unsigned int *seed);
void evocore_weighted_array_reset(evocore_weighted_array_t *array);

// Utilities
bool evocore_weighted_merge(evocore_weighted_stats_t *stats1, const evocore_weighted_stats_t *stats2);
void evocore_weighted_clone(const evocore_weighted_stats_t *src, evocore_weighted_stats_t *dst);
size_t evocore_weighted_to_json(const evocore_weighted_stats_t *stats, char *buffer, size_t buffer_size);
bool evocore_weighted_from_json(const char *json, evocore_weighted_stats_t *stats);

// ==========================================================================
// Negative Learning (negative.h)
// ==========================================================================

typedef enum {
    EVOCORE_SEVERITY_NONE = 0,
    EVOCORE_SEVERITY_MILD = 1,
    EVOCORE_SEVERITY_MODERATE = 2,
    EVOCORE_SEVERITY_SEVERE = 3,
    EVOCORE_SEVERITY_FATAL = 4
} evocore_failure_severity_t;

typedef struct {
    evocore_genome_t genome;
    double fitness;
    evocore_failure_severity_t severity;
    int generation;
    double penalty_score;
    int repeat_count;
    time_t first_seen;
    time_t last_seen;
    bool is_active;
} evocore_failure_record_t;

typedef struct {
    evocore_failure_record_t *failures;
    size_t capacity;
    size_t count;
    double base_penalty;
    double repeat_multiplier;
    double decay_rate;
    double severity_thresholds[4];
    double similarity_threshold;
    int cleanup_interval;
    int last_cleanup_generation;
    int current_generation;
} evocore_negative_learning_t;

typedef struct {
    size_t total_count;
    size_t active_count;
    size_t mild_count;
    size_t moderate_count;
    size_t severe_count;
    size_t fatal_count;
    double avg_penalty;
    double max_penalty;
    size_t repeat_victims;
} evocore_negative_stats_t;

// Classification
evocore_failure_severity_t evocore_classify_failure(double fitness, const double thresholds[4]);

// Lifecycle
evocore_error_t evocore_negative_learning_init(evocore_negative_learning_t *neg, size_t capacity,
                                                double base_penalty, double decay_rate);
evocore_error_t evocore_negative_learning_init_default(evocore_negative_learning_t *neg, size_t capacity);
evocore_error_t evocore_negative_learning_set_thresholds(evocore_negative_learning_t *neg,
                                                          double mild, double moderate, double severe, double fatal);
void evocore_negative_learning_cleanup(evocore_negative_learning_t *neg);

// Recording
evocore_error_t evocore_negative_learning_record_failure(evocore_negative_learning_t *neg,
                                                          const evocore_genome_t *genome, double fitness, int generation);
evocore_error_t evocore_negative_learning_record_failure_severity(evocore_negative_learning_t *neg,
                                                                   const evocore_genome_t *genome, double fitness,
                                                                   evocore_failure_severity_t severity, int generation);
void evocore_negative_learning_set_generation(evocore_negative_learning_t *neg, int generation);

// Queries
evocore_error_t evocore_negative_learning_check_penalty(const evocore_negative_learning_t *neg,
                                                         const evocore_genome_t *genome, double *penalty_out);
bool evocore_negative_learning_is_forbidden(const evocore_negative_learning_t *neg,
                                             const evocore_genome_t *genome, double threshold);
evocore_error_t evocore_negative_learning_adjust_fitness(const evocore_negative_learning_t *neg,
                                                          const evocore_genome_t *genome, double raw_fitness, double *adjusted_out);
evocore_error_t evocore_negative_learning_find_similar(const evocore_negative_learning_t *neg,
                                                        const evocore_genome_t *genome, evocore_failure_record_t **failure_out,
                                                        double *similarity_out);

// Maintenance
void evocore_negative_learning_decay(evocore_negative_learning_t *neg, int generations_passed);
size_t evocore_negative_learning_prune(evocore_negative_learning_t *neg, double min_penalty, int max_age_generations);
evocore_error_t evocore_negative_learning_stats(const evocore_negative_learning_t *neg, evocore_negative_stats_t *stats_out);
size_t evocore_negative_learning_count(const evocore_negative_learning_t *neg);
size_t evocore_negative_learning_active_count(const evocore_negative_learning_t *neg);
void evocore_negative_learning_clear(evocore_negative_learning_t *neg);

// Configuration
void evocore_negative_learning_set_base_penalty(evocore_negative_learning_t *neg, double base_penalty);
void evocore_negative_learning_set_repeat_multiplier(evocore_negative_learning_t *neg, double multiplier);
void evocore_negative_learning_set_decay_rate(evocore_negative_learning_t *neg, double decay_rate);
void evocore_negative_learning_set_similarity_threshold(evocore_negative_learning_t *neg, double threshold);

// Helpers
const char* evocore_severity_string(evocore_failure_severity_t severity);
evocore_failure_severity_t evocore_severity_from_string(const char *str);

// ==========================================================================
// Context Learning (context.h)
// ==========================================================================

typedef struct {
    const char *name;
    size_t value_count;
    const char **values;
} evocore_context_dimension_t;

typedef struct {
    char *key;
    evocore_weighted_array_t *stats;
    size_t param_count;
    double confidence;
    time_t first_update;
    time_t last_update;
    size_t total_experiences;
    double avg_fitness;
    double best_fitness;
    evocore_negative_learning_t *negative;
    size_t failure_count;
    double avg_failure_fitness;
} evocore_context_stats_t;

typedef struct {
    evocore_context_dimension_t *dimensions;
    size_t dimension_count;
    void *internal;  // Hash table
    size_t param_count;
    size_t total_contexts;
} evocore_context_system_t;

typedef struct {
    evocore_context_stats_t **contexts;
    size_t count;
    size_t capacity;
} evocore_context_query_t;

// System management
evocore_context_system_t* evocore_context_system_create(const evocore_context_dimension_t *dimensions,
                                                         size_t dimension_count, size_t param_count);
void evocore_context_system_free(evocore_context_system_t *system);
bool evocore_context_add_dimension(evocore_context_system_t *system, const char *name,
                                    const char **values, size_t value_count);

// Context keys
bool evocore_context_build_key(const evocore_context_system_t *system, const char **dimension_values,
                                char *out_key, size_t key_size);
bool evocore_context_parse_key(const evocore_context_system_t *system, const char *key, char **out_values);
bool evocore_context_validate_values(const evocore_context_system_t *system, const char **dimension_values);

// Learning
bool evocore_context_learn(evocore_context_system_t *system, const char **dimension_values,
                            const double *parameters, size_t param_count, double fitness);
bool evocore_context_learn_key(evocore_context_system_t *system, const char *context_key,
                                const double *parameters, size_t param_count, double fitness);

// Statistics
bool evocore_context_get_stats(evocore_context_system_t *system, const char **dimension_values,
                                evocore_context_stats_t **out_stats);
bool evocore_context_get_stats_key(const evocore_context_system_t *system, const char *context_key,
                                    evocore_context_stats_t **out_stats);
bool evocore_context_has_data(const evocore_context_stats_t *stats, size_t min_samples);

// Sampling
bool evocore_context_sample(const evocore_context_system_t *system, const char **dimension_values,
                             double *out_parameters, size_t param_count, double exploration_factor, unsigned int *seed);
bool evocore_context_sample_key(const evocore_context_system_t *system, const char *context_key,
                                 double *out_parameters, size_t param_count, double exploration_factor, unsigned int *seed);

// Queries
bool evocore_context_query_best(const evocore_context_system_t *system, const char *partial_match,
                                 size_t min_samples, evocore_context_query_t *out_results, size_t max_results);
void evocore_context_query_free(evocore_context_query_t *results);
size_t evocore_context_count(const evocore_context_system_t *system);
size_t evocore_context_get_keys(const evocore_context_system_t *system, char **out_keys, size_t max_keys);

// Persistence
bool evocore_context_save_json(const evocore_context_system_t *system, const char *filepath);
bool evocore_context_load_json(const char *filepath, evocore_context_system_t **out_system);
bool evocore_context_save_binary(const evocore_context_system_t *system, const char *filepath);
bool evocore_context_load_binary(const char *filepath, evocore_context_system_t **out_system);
bool evocore_context_export_csv(const evocore_context_system_t *system, const char *filepath);

// Negative learning integration
bool evocore_context_record_failure(evocore_context_system_t *system, const char *context_key,
                                     const evocore_genome_t *genome, double fitness,
                                     evocore_failure_severity_t severity, int generation);
bool evocore_context_check_penalty(const evocore_context_system_t *system, const char *context_key,
                                    const evocore_genome_t *genome, double *penalty_out);
bool evocore_context_is_forbidden(const evocore_context_system_t *system, const char *context_key,
                                   const evocore_genome_t *genome, double threshold);
bool evocore_context_get_negative_stats(const evocore_context_system_t *system, const char *context_key,
                                         evocore_negative_stats_t *stats_out);

// Utilities
bool evocore_context_reset(evocore_context_system_t *system, const char **dimension_values);
void evocore_context_reset_all(evocore_context_system_t *system);
double evocore_context_confidence(const evocore_context_stats_t *stats);
bool evocore_context_merge(evocore_context_system_t *system, const char *target_key, const char *source_key);

// ==========================================================================
// Temporal Learning (temporal.h)
// ==========================================================================

typedef enum {
    EVOCORE_BUCKET_MINUTE = 0,
    EVOCORE_BUCKET_HOUR = 1,
    EVOCORE_BUCKET_DAY = 2,
    EVOCORE_BUCKET_WEEK = 3,
    EVOCORE_BUCKET_MONTH = 4,
    EVOCORE_BUCKET_YEAR = 5
} evocore_temporal_bucket_type_t;

typedef struct {
    time_t start_time;
    time_t end_time;
    bool is_complete;
    evocore_weighted_array_t *stats;
    size_t param_count;
    size_t sample_count;
    double avg_fitness;
    double best_fitness;
    double worst_fitness;
} evocore_temporal_bucket_t;

typedef struct {
    evocore_temporal_bucket_t *buckets;
    size_t count;
    size_t capacity;
    evocore_temporal_bucket_type_t bucket_type;
} evocore_temporal_list_t;

typedef struct {
    char *key;
    evocore_temporal_list_t *temporal_list;
} evocore_temporal_key_t;

typedef struct {
    evocore_temporal_bucket_type_t bucket_type;
    size_t param_count;
    size_t retention_count;
    void *internal;  // Hash table
    time_t last_update;
} evocore_temporal_system_t;

// System management
evocore_temporal_system_t* evocore_temporal_create(evocore_temporal_bucket_type_t bucket_type,
                                                    size_t param_count, size_t retention_count);
void evocore_temporal_free(evocore_temporal_system_t *system);
time_t evocore_temporal_bucket_duration(evocore_temporal_bucket_type_t bucket_type);

// Learning
bool evocore_temporal_learn(evocore_temporal_system_t *system, const char *context_key,
                             const double *parameters, size_t param_count, double fitness, time_t timestamp);
bool evocore_temporal_learn_now(evocore_temporal_system_t *system, const char *context_key,
                                 const double *parameters, size_t param_count, double fitness);

// Organic mean (recency-weighted)
bool evocore_temporal_get_organic_mean(const evocore_temporal_system_t *system, const char *context_key,
                                        double *out_parameters, size_t param_count, double *out_confidence);
bool evocore_temporal_get_weighted_mean(const evocore_temporal_system_t *system, const char *context_key,
                                         double *out_parameters, size_t param_count);

// Trend analysis
bool evocore_temporal_get_trend(const evocore_temporal_system_t *system, const char *context_key,
                                 double *out_slopes, size_t param_count);
int evocore_temporal_trend_direction(double slope);

// Regime detection
bool evocore_temporal_compare_recent(const evocore_temporal_system_t *system, const char *context_key,
                                      size_t recent_buckets, double *out_drift, size_t param_count);
bool evocore_temporal_detect_regime_change(const evocore_temporal_system_t *system, const char *context_key,
                                            size_t recent_buckets, double threshold);

// Bucket management
bool evocore_temporal_get_bucket_at(const evocore_temporal_system_t *system, const char *context_key,
                                     time_t timestamp, evocore_temporal_bucket_t **out_bucket);
bool evocore_temporal_get_current_bucket(const evocore_temporal_system_t *system, const char *context_key,
                                          evocore_temporal_bucket_t **out_bucket);
bool evocore_temporal_get_buckets(const evocore_temporal_system_t *system, const char *context_key,
                                   evocore_temporal_list_t **out_list);
void evocore_temporal_free_list(evocore_temporal_list_t *list);

// Sampling
bool evocore_temporal_sample_organic(const evocore_temporal_system_t *system, const char *context_key,
                                      double *out_parameters, size_t param_count, double exploration_factor, unsigned int *seed);
bool evocore_temporal_sample_trend(const evocore_temporal_system_t *system, const char *context_key,
                                    double *out_parameters, size_t param_count, double trend_strength, unsigned int *seed);

// Persistence
bool evocore_temporal_save_json(const evocore_temporal_system_t *system, const char *filepath);
bool evocore_temporal_load_json(const char *filepath, evocore_temporal_system_t **out_system);

// Utilities
size_t evocore_temporal_bucket_count(const evocore_temporal_system_t *system);
size_t evocore_temporal_context_count(const evocore_temporal_system_t *system);
size_t evocore_temporal_prune_old(evocore_temporal_system_t *system);
bool evocore_temporal_reset_context(evocore_temporal_system_t *system, const char *context_key);
void evocore_temporal_reset_all(evocore_temporal_system_t *system);

// ==========================================================================
// Meta-Evolution (meta.h)
// ==========================================================================

typedef struct {
    // Mutation rates
    double optimization_mutation_rate;
    double variance_mutation_rate;
    double experimentation_rate;

    // Selection pressure
    double elite_protection_ratio;
    double culling_ratio;
    double fitness_threshold_for_breeding;

    // Population dynamics
    size_t target_population_size;
    size_t min_population_size;
    size_t max_population_size;

    // Learning
    double learning_rate;
    double exploration_factor;
    double confidence_threshold;

    // Breeding ratios
    double profitable_optimization_ratio;
    double profitable_random_ratio;
    double losing_optimization_ratio;
    double losing_random_ratio;

    // Meta-meta parameters
    double meta_mutation_rate;
    double meta_learning_rate;
    double meta_convergence_threshold;

    // Negative learning
    bool negative_learning_enabled;
    double negative_penalty_weight;
    double negative_decay_rate;
    size_t negative_capacity;
    double negative_similarity_threshold;
    double negative_forbidden_threshold;
} evocore_meta_params_t;

typedef struct {
    evocore_meta_params_t params;
    double meta_fitness;
    size_t generation;
    double *fitness_history;
    size_t history_capacity;
    size_t history_count;
} evocore_meta_individual_t;

typedef struct {
    evocore_meta_individual_t *individuals;
    size_t count;
    size_t capacity;
    size_t current_generation;
    evocore_meta_params_t best_params;
    double best_meta_fitness;
    bool initialized;
} evocore_meta_population_t;

// Parameter management
void evocore_meta_params_init(evocore_meta_params_t *params);
evocore_error_t evocore_meta_params_validate(const evocore_meta_params_t *params);
void evocore_meta_params_mutate(evocore_meta_params_t *params, unsigned int *seed);
void evocore_meta_params_clone(const evocore_meta_params_t *src, evocore_meta_params_t *dst);
double evocore_meta_params_get(const evocore_meta_params_t *params, const char *name);
evocore_error_t evocore_meta_params_set(evocore_meta_params_t *params, const char *name, double value);
void evocore_meta_params_print(const evocore_meta_params_t *params);

// Individual management
evocore_error_t evocore_meta_individual_init(evocore_meta_individual_t *individual,
                                              const evocore_meta_params_t *params, size_t history_capacity);
void evocore_meta_individual_cleanup(evocore_meta_individual_t *individual);
evocore_error_t evocore_meta_individual_record_fitness(evocore_meta_individual_t *individual, double fitness);
double evocore_meta_individual_average_fitness(const evocore_meta_individual_t *individual);
double evocore_meta_individual_improvement_trend(const evocore_meta_individual_t *individual);

// Population management
evocore_error_t evocore_meta_population_init(evocore_meta_population_t *meta_pop, int size, unsigned int *seed);
void evocore_meta_population_cleanup(evocore_meta_population_t *meta_pop);
evocore_meta_individual_t* evocore_meta_population_best(evocore_meta_population_t *meta_pop);
evocore_error_t evocore_meta_population_evolve(evocore_meta_population_t *meta_pop, unsigned int *seed);
void evocore_meta_population_sort(evocore_meta_population_t *meta_pop);
bool evocore_meta_population_converged(const evocore_meta_population_t *meta_pop, double threshold, int generations);

// Adaptation
void evocore_meta_adapt(evocore_meta_params_t *params, const double *recent_fitness, size_t count, bool improvement);
void evocore_meta_suggest_mutation_rate(double diversity, evocore_meta_params_t *params);
void evocore_meta_suggest_selection_pressure(double fitness_stddev, evocore_meta_params_t *params);

// Evaluation and learning
double evocore_meta_evaluate(const evocore_meta_params_t *params, double best_fitness,
                              double avg_fitness, double diversity, int generations);
void evocore_meta_learn_outcome(double mutation_rate, double exploration_factor, double fitness, double learning_rate);
bool evocore_meta_get_learned_params(double *mutation_rate_out, double *exploration_out, int min_samples);
void evocore_meta_reset_learning(void);

// ==========================================================================
// Exploration Control (exploration.h)
// ==========================================================================

typedef enum {
    EVOCORE_EXPLORE_FIXED = 0,
    EVOCORE_EXPLORE_DECAY = 1,
    EVOCORE_EXPLORE_ADAPTIVE = 2,
    EVOCORE_EXPLORE_UCB1 = 3,
    EVOCORE_EXPLORE_BOLTZMANN = 4
} evocore_explore_strategy_t;

typedef struct {
    evocore_explore_strategy_t strategy;
    double base_rate;
    double current_rate;
    double min_rate;
    double max_rate;
    double decay_rate;
    double temperature;
    double cooling_rate;
    double ucb_c;
    // Adaptive tracking
    double last_best_fitness;
    size_t stagnant_generations;
    size_t improvement_count;
    size_t total_generations;
} evocore_exploration_t;

typedef struct {
    size_t count;
    double total_reward;
    double mean_reward;
} evocore_bandit_arm_t;

typedef struct {
    evocore_bandit_arm_t *arms;
    size_t count;
    size_t total_pulls;
    double ucb_c;
} evocore_bandit_t;

// Exploration management
evocore_exploration_t* evocore_exploration_create(evocore_explore_strategy_t strategy, double base_rate);
void evocore_exploration_free(evocore_exploration_t *exp);
void evocore_exploration_reset(evocore_exploration_t *exp);
void evocore_exploration_set_bounds(evocore_exploration_t *exp, double min_rate, double max_rate);
void evocore_exploration_set_decay_rate(evocore_exploration_t *exp, double decay_rate);
void evocore_exploration_set_temperature(evocore_exploration_t *exp, double temperature, double cooling_rate);
void evocore_exploration_set_ucb_c(evocore_exploration_t *exp, double ucb_c);

// Rate updates
double evocore_exploration_update(evocore_exploration_t *exp, size_t generation, double best_fitness);
double evocore_exploration_get_rate(const evocore_exploration_t *exp);
bool evocore_exploration_should_explore(const evocore_exploration_t *exp, unsigned int *seed);

// Bandit (UCB1)
evocore_bandit_t* evocore_bandit_create(size_t arm_count, double ucb_c);
void evocore_bandit_free(evocore_bandit_t *bandit);
size_t evocore_bandit_select_ucb(const evocore_bandit_t *bandit);
void evocore_bandit_update(evocore_bandit_t *bandit, size_t arm, double reward);
size_t evocore_bandit_arm_count(const evocore_bandit_t *bandit);
bool evocore_bandit_get_stats(const evocore_bandit_t *bandit, size_t arm, size_t *out_count, double *out_mean);
void evocore_bandit_reset(evocore_bandit_t *bandit);

// Boltzmann selection
size_t evocore_boltzmann_select(const double *values, size_t count, double temperature, unsigned int *seed);
double evocore_cool_temperature(double temperature, double cooling_rate);

// Adaptive
bool evocore_exploration_is_stagnant(const evocore_exploration_t *exp, size_t threshold);
void evocore_exploration_boost(evocore_exploration_t *exp, double factor);
double evocore_exploration_improvement_rate(const evocore_exploration_t *exp);

// ==========================================================================
// Synthesis (synthesis.h)
// ==========================================================================

typedef enum {
    EVOCORE_SYNTHESIS_AVERAGE = 0,
    EVOCORE_SYNTHESIS_WEIGHTED = 1,
    EVOCORE_SYNTHESIS_TREND = 2,
    EVOCORE_SYNTHESIS_REGIME = 3,
    EVOCORE_SYNTHESIS_ENSEMBLE = 4,
    EVOCORE_SYNTHESIS_NEAREST = 5
} evocore_synthesis_strategy_t;

typedef struct {
    double *parameters;
    size_t param_count;
    double confidence;
    double fitness;
    time_t timestamp;
    char *context_id;
} evocore_param_source_t;

typedef struct {
    evocore_synthesis_strategy_t strategy;
    size_t target_param_count;
    size_t source_count;
    evocore_param_source_t *sources;
    double exploration_factor;
    double trend_strength;
    size_t ensemble_count;
    double *result;
    double synthesis_confidence;
} evocore_synthesis_request_t;

typedef struct {
    size_t context_count;
    char **context_ids;
    double **similarity_matrix;
    time_t last_update;
} evocore_similarity_matrix_t;

// Synthesis operations
evocore_synthesis_request_t* evocore_synthesis_request_create(evocore_synthesis_strategy_t strategy,
                                                               size_t param_count, size_t source_count);
void evocore_synthesis_request_free(evocore_synthesis_request_t *request);
bool evocore_synthesis_add_source(evocore_synthesis_request_t *request, size_t index, const double *parameters,
                                   double confidence, double fitness, const char *context_id);
bool evocore_synthesis_execute(evocore_synthesis_request_t *request, double *out_parameters,
                                double *out_confidence, unsigned int *seed);

// Strategy implementations
bool evocore_synthesis_average(const evocore_synthesis_request_t *request, double *out_parameters);
bool evocore_synthesis_weighted(const evocore_synthesis_request_t *request, double *out_parameters);
bool evocore_synthesis_trend(const evocore_synthesis_request_t *request, double *out_parameters, double trend_strength);
bool evocore_synthesis_regime(const evocore_synthesis_request_t *request, double *out_parameters, const char *current_regime);
bool evocore_synthesis_ensemble(const evocore_synthesis_request_t *request, double *out_parameters, unsigned int *seed);
bool evocore_synthesis_nearest(const evocore_synthesis_request_t *request, double *out_parameters, const char *target_context);

// Similarity
evocore_similarity_matrix_t* evocore_similarity_matrix_create(size_t context_count, char **context_ids);
void evocore_similarity_matrix_free(evocore_similarity_matrix_t *matrix);
bool evocore_similarity_update(evocore_similarity_matrix_t *matrix, const char *context_a,
                                const char *context_b, double similarity);
double evocore_similarity_get(const evocore_similarity_matrix_t *matrix, const char *context_a, const char *context_b);
const char* evocore_similarity_find_nearest(const evocore_similarity_matrix_t *matrix, const char *target_context);
double evocore_param_distance(const double *params1, const double *params2, size_t count);
double evocore_param_similarity(const double *params1, const double *params2, size_t count, double max_distance);

// Transfer learning
bool evocore_transfer_params(const double *source_params, const char *source_context, const char *target_context,
                              const evocore_similarity_matrix_t *similarity_matrix, size_t param_count,
                              double *out_params, double adjustment_factor);
size_t evocore_find_transferable_contexts(const char *target_context, const evocore_similarity_matrix_t *similarity_matrix,
                                           double min_similarity, const char **out_contexts, size_t max_contexts);

// Utilities
bool evocore_synthesis_validate(const evocore_synthesis_request_t *request);
const char* evocore_synthesis_strategy_name(evocore_synthesis_strategy_t strategy);

// ==========================================================================
// Adaptive Scheduler (adaptive_scheduler.h)
// ==========================================================================

typedef enum {
    EVOCORE_PHASE_EARLY = 0,
    EVOCORE_PHASE_MID = 1,
    EVOCORE_PHASE_LATE = 2
} evocore_evolution_phase_t;

typedef struct {
    // Configuration
    size_t max_generations;
    evocore_meta_params_t initial_params;

    // Progress tracking
    size_t current_generation;
    double progress;
    evocore_evolution_phase_t phase;

    // Fitness tracking
    double best_fitness;
    double avg_fitness;
    double fitness_variance;
    double *fitness_history;
    size_t history_capacity;
    size_t history_count;

    // Convergence tracking
    size_t stagnant_generations;
    double improvement_rate;
    double last_improvement_generation;

    // Diversity tracking
    double diversity;
    double diversity_trend;

    // Adaptive parameters
    double current_mutation_rate;
    double current_selection_pressure;
    size_t current_population_size;

    // Recovery state
    bool in_recovery_mode;
    size_t recovery_generations_remaining;
} evocore_adaptive_scheduler_t;

// Lifecycle
evocore_adaptive_scheduler_t* evocore_adaptive_scheduler_create(size_t max_generations,
                                                                  const evocore_meta_params_t *initial_params);
void evocore_adaptive_scheduler_free(evocore_adaptive_scheduler_t *scheduler);

// Phase detection
evocore_evolution_phase_t evocore_adaptive_scheduler_get_phase(const evocore_adaptive_scheduler_t *scheduler);
double evocore_adaptive_scheduler_get_progress(const evocore_adaptive_scheduler_t *scheduler);

// Convergence tracking
evocore_error_t evocore_adaptive_scheduler_update(evocore_adaptive_scheduler_t *scheduler,
                                                   double best_fitness, double avg_fitness, double diversity);
bool evocore_adaptive_scheduler_is_stagnant(const evocore_adaptive_scheduler_t *scheduler);
double evocore_adaptive_scheduler_get_improvement_rate(const evocore_adaptive_scheduler_t *scheduler);
double evocore_adaptive_scheduler_get_fitness_variance(const evocore_adaptive_scheduler_t *scheduler);

// Parameter scheduling
double evocore_adaptive_scheduler_get_mutation_rate(evocore_adaptive_scheduler_t *scheduler);
double evocore_adaptive_scheduler_get_selection_pressure(evocore_adaptive_scheduler_t *scheduler, double fitness_variance);
size_t evocore_adaptive_scheduler_get_population_size(evocore_adaptive_scheduler_t *scheduler);
evocore_error_t evocore_adaptive_scheduler_apply_to_meta(evocore_adaptive_scheduler_t *scheduler,
                                                          evocore_meta_params_t *params);

// Recovery
evocore_error_t evocore_adaptive_scheduler_trigger_recovery(evocore_adaptive_scheduler_t *scheduler);
evocore_error_t evocore_adaptive_scheduler_diversity_intervention(evocore_adaptive_scheduler_t *scheduler,
                                                                    double diversity, char *out_action, size_t action_size);

// Diagnostics
evocore_error_t evocore_adaptive_scheduler_get_state(const evocore_adaptive_scheduler_t *scheduler,
                                                      char *out_state, size_t size);
void evocore_adaptive_scheduler_print_stats(const evocore_adaptive_scheduler_t *scheduler);

// ==========================================================================
// Statistics (stats.h)
// ==========================================================================

typedef struct {
    // Generation info
    size_t generation;
    size_t total_generations;

    // Fitness tracking
    double best_fitness;
    double avg_fitness;
    double worst_fitness;
    double fitness_stddev;
    double fitness_improvement;

    // Convergence
    bool is_converged;
    bool is_stagnant;
    size_t stagnant_generations;

    // Diversity
    double diversity;
    double diversity_trend;

    // Timing
    double generation_time_ms;
    double total_time_ms;
    double avg_eval_time_ms;

    // Operation counts
    int64_t evaluation_count;
    int64_t mutation_count;
    int64_t crossover_count;

    // Memory
    size_t memory_used;
    size_t peak_memory;
} evocore_stats_t;

typedef struct {
    double improvement_threshold;
    size_t stagnation_generations;
    double diversity_threshold;
    bool track_timing;
    bool track_memory;
    bool track_diversity;
} evocore_stats_config_t;

typedef void (*evocore_progress_callback_t)(const evocore_stats_t *stats, void *user_data);

typedef struct {
    evocore_progress_callback_t callback;
    void *user_data;
    size_t report_every_n_generations;
    bool verbose;
} evocore_progress_reporter_t;

// Statistics
evocore_stats_t* evocore_stats_create(const evocore_stats_config_t *config);
void evocore_stats_free(evocore_stats_t *stats);
evocore_error_t evocore_stats_update(evocore_stats_t *stats, const evocore_population_t *pop);
evocore_error_t evocore_stats_record_operations(evocore_stats_t *stats, int64_t eval_count,
                                                 int64_t mutations, int64_t crossovers);
bool evocore_stats_is_converged(const evocore_stats_t *stats);
bool evocore_stats_is_stagnant(const evocore_stats_t *stats);
double evocore_stats_diversity(const evocore_population_t *pop);
evocore_error_t evocore_stats_fitness_distribution(const evocore_population_t *pop, double *out_min,
                                                    double *out_max, double *out_mean, double *out_stddev);

// Progress reporting
evocore_error_t evocore_progress_reporter_init(evocore_progress_reporter_t *reporter,
                                                evocore_progress_callback_t callback, void *user_data);
evocore_error_t evocore_progress_report(const evocore_progress_reporter_t *reporter, const evocore_stats_t *stats);
void evocore_progress_print_console(const evocore_stats_t *stats, void *user_data);

// ==========================================================================
// Configuration (config.h)
// ==========================================================================

typedef enum {
    EVOCORE_CONFIG_STRING = 0,
    EVOCORE_CONFIG_INT = 1,
    EVOCORE_CONFIG_DOUBLE = 2,
    EVOCORE_CONFIG_BOOL = 3
} evocore_config_type_t;

typedef struct {
    const char *key;
    const char *value;
    evocore_config_type_t type;
} evocore_config_entry_t;

typedef struct evocore_config_t evocore_config_t;

evocore_error_t evocore_config_load(const char *path, evocore_config_t **config);
void evocore_config_free(evocore_config_t *config);
const char* evocore_config_get_string(const evocore_config_t *config, const char *section,
                                       const char *key, const char *default_value);
int evocore_config_get_int(const evocore_config_t *config, const char *section, const char *key, int default_value);
double evocore_config_get_double(const evocore_config_t *config, const char *section,
                                  const char *key, double default_value);
bool evocore_config_get_bool(const evocore_config_t *config, const char *section, const char *key, bool default_value);
bool evocore_config_has_key(const evocore_config_t *config, const char *section, const char *key);
size_t evocore_config_section_size(const evocore_config_t *config, const char *section);
const evocore_config_entry_t* evocore_config_get_entry(const evocore_config_t *config,
                                                        const char *section, size_t index);

// ==========================================================================
// Persistence (persist.h)
// ==========================================================================

typedef enum {
    EVOCORE_SERIAL_JSON = 0,
    EVOCORE_SERIAL_BINARY = 1,
    EVOCORE_SERIAL_MSGPACK = 2
} evocore_serial_format_t;

typedef struct {
    evocore_serial_format_t format;
    bool include_metadata;
    bool pretty_print;
    int compression_level;
} evocore_serial_options_t;

typedef struct {
    uint32_t version;
    time_t timestamp;
    size_t population_size;
    size_t generation;
    double best_fitness;
    double avg_fitness;
    evocore_meta_params_t meta_params;
    char *domain_name;
    void *user_data;
    size_t user_data_size;
    char *serialized_population;
    size_t serialized_size;
} evocore_checkpoint_t;

typedef struct {
    bool enabled;
    size_t every_n_generations;
    char *directory;
    size_t max_checkpoints;
    bool compress;
} evocore_auto_checkpoint_config_t;

typedef struct evocore_checkpoint_manager_t evocore_checkpoint_manager_t;

// Genome serialization
evocore_error_t evocore_genome_serialize(const evocore_genome_t *genome, char **buffer, size_t *buffer_size,
                                          const evocore_serial_options_t *options);
evocore_error_t evocore_genome_deserialize(const char *buffer, size_t buffer_size, evocore_genome_t *genome,
                                            evocore_serial_format_t format);
evocore_error_t evocore_genome_save(const evocore_genome_t *genome, const char *filepath,
                                     const evocore_serial_options_t *options);
evocore_error_t evocore_genome_load(const char *filepath, evocore_genome_t *genome);

// Population serialization
evocore_error_t evocore_population_serialize(const evocore_population_t *pop, const evocore_domain_t *domain,
                                              char **buffer, size_t *buffer_size, const evocore_serial_options_t *options);
evocore_error_t evocore_population_deserialize(const char *buffer, size_t buffer_size, evocore_population_t *pop,
                                                const evocore_domain_t *domain);
evocore_error_t evocore_population_save(const evocore_population_t *pop, const evocore_domain_t *domain,
                                         const char *filepath, const evocore_serial_options_t *options);
evocore_error_t evocore_population_load(const char *filepath, evocore_population_t *pop, const evocore_domain_t *domain);

// Meta-evolution serialization
evocore_error_t evocore_meta_serialize(const evocore_meta_population_t *meta_pop, char **buffer, size_t *buffer_size,
                                        const evocore_serial_options_t *options);
evocore_error_t evocore_meta_deserialize(const char *buffer, size_t buffer_size, evocore_meta_population_t *meta_pop);
evocore_error_t evocore_meta_save(const evocore_meta_population_t *meta_pop, const char *filepath,
                                   const evocore_serial_options_t *options);
evocore_error_t evocore_meta_load(const char *filepath, evocore_meta_population_t *meta_pop);

// Checkpointing
evocore_error_t evocore_checkpoint_create(evocore_checkpoint_t *checkpoint, const evocore_population_t *pop,
                                           const evocore_domain_t *domain, const evocore_meta_population_t *meta_pop);
evocore_error_t evocore_checkpoint_save(const evocore_checkpoint_t *checkpoint, const char *filepath,
                                         const evocore_serial_options_t *options);
evocore_error_t evocore_checkpoint_load(const char *filepath, evocore_checkpoint_t *checkpoint);
evocore_error_t evocore_checkpoint_restore(const evocore_checkpoint_t *checkpoint, evocore_population_t *pop,
                                            const evocore_domain_t *domain, evocore_meta_population_t *meta_pop);
void evocore_checkpoint_free(evocore_checkpoint_t *checkpoint);

// Checkpoint manager
evocore_checkpoint_manager_t* evocore_checkpoint_manager_create(const evocore_auto_checkpoint_config_t *config);
void evocore_checkpoint_manager_destroy(evocore_checkpoint_manager_t *manager);
evocore_error_t evocore_checkpoint_manager_update(evocore_checkpoint_manager_t *manager, const evocore_population_t *pop,
                                                   const evocore_domain_t *domain, const evocore_meta_population_t *meta_pop);
char** evocore_checkpoint_list(const char *directory, int *count);
void evocore_checkpoint_list_free(char **list, int count);
evocore_error_t evocore_checkpoint_info(const char *filepath, evocore_checkpoint_t *checkpoint);

// Utility
uint32_t evocore_checksum(const void *data, size_t size);
bool evocore_checksum_validate(const void *data, size_t size, uint32_t expected);

// ==========================================================================
// Performance & Optimization (optimize.h)
// ==========================================================================

typedef struct evocore_mempool_t evocore_mempool_t;
typedef struct evocore_parallel_ctx_t evocore_parallel_ctx_t;

typedef struct {
    const char *name;
    size_t count;
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
} evocore_perf_counter_t;

typedef struct {
    evocore_perf_counter_t *counters;
    size_t count;
    bool enabled;
} evocore_perf_monitor_t;

// Memory pool
evocore_mempool_t* evocore_mempool_create(size_t genome_size, size_t block_size);
void evocore_mempool_destroy(evocore_mempool_t *pool);
evocore_error_t evocore_mempool_alloc(evocore_mempool_t *pool, evocore_genome_t *genome);
void evocore_mempool_free(evocore_mempool_t *pool, evocore_genome_t *genome);
void evocore_mempool_get_stats(const evocore_mempool_t *pool, size_t *total_allocations,
                                size_t *current_allocations, size_t *total_blocks, size_t *free_blocks);

// Parallel evaluation
evocore_parallel_ctx_t* evocore_parallel_create(int num_threads);
void evocore_parallel_destroy(evocore_parallel_ctx_t *ctx);
evocore_error_t evocore_parallel_evaluate_population(evocore_parallel_ctx_t *ctx, evocore_population_t *pop,
                                                      evocore_fitness_func_t fitness_func, void *user_context);
evocore_error_t evocore_parallel_evaluate_batch(evocore_parallel_ctx_t *ctx, const evocore_genome_t **genomes,
                                                 double *fitnesses, size_t count, evocore_fitness_func_t fitness_func,
                                                 void *user_context);
int evocore_parallel_get_num_threads(const evocore_parallel_ctx_t *ctx);

// SIMD operations
void evocore_simd_mutate_genome(evocore_genome_t *genome, double rate, unsigned int *seed);
size_t evocore_simd_genome_hamming_distance(const evocore_genome_t *a, const evocore_genome_t *b);
bool evocore_simd_available(void);

// Population layout
evocore_error_t evocore_population_optimize_layout(evocore_population_t *pop);

// Performance monitoring
evocore_perf_monitor_t* evocore_perf_monitor_get(void);
void evocore_perf_reset(void);
void evocore_perf_set_enabled(bool enabled);
int evocore_perf_start(const char *name);
double evocore_perf_end(int index);
void evocore_perf_print(void);
const evocore_perf_counter_t* evocore_perf_get(const char *name);

// ==========================================================================
// GPU Acceleration (gpu.h)
// ==========================================================================

typedef struct evocore_gpu_context_t evocore_gpu_context_t;

typedef struct {
    int device_id;
    char name[256];
    size_t total_memory;
    size_t free_memory;
    int compute_capability_major;
    int compute_capability_minor;
    int multiprocessor_count;
    int max_threads_per_block;
    bool available;
} evocore_gpu_device_t;

typedef struct {
    evocore_genome_t **genomes;
    double *fitnesses;
    size_t count;
    size_t genome_size;
} evocore_eval_batch_t;

typedef struct {
    size_t evaluated;
    double gpu_time_ms;
    double cpu_time_ms;
    bool used_gpu;
} evocore_eval_result_t;

typedef struct {
    size_t total_evaluations;
    size_t gpu_evaluations;
    size_t cpu_evaluations;
    double total_gpu_time_ms;
    double total_cpu_time_ms;
    double avg_gpu_time_ms;
    double avg_cpu_time_ms;
} evocore_gpu_stats_t;

// Context management
evocore_gpu_context_t* evocore_gpu_init(void);
void evocore_gpu_shutdown(evocore_gpu_context_t *ctx);
bool evocore_gpu_available(const evocore_gpu_context_t *ctx);
int evocore_gpu_device_count(const evocore_gpu_context_t *ctx);
const evocore_gpu_device_t* evocore_gpu_get_device(const evocore_gpu_context_t *ctx, int device_id);
evocore_error_t evocore_gpu_select_device(evocore_gpu_context_t *ctx, int device_id);
int evocore_gpu_get_current_device(const evocore_gpu_context_t *ctx);
void evocore_gpu_print_info(const evocore_gpu_context_t *ctx);

// Batch evaluation
evocore_error_t evocore_gpu_evaluate_batch(evocore_gpu_context_t *ctx, const evocore_eval_batch_t *batch,
                                            evocore_fitness_func_t fitness_func, void *user_context,
                                            evocore_eval_result_t *result);
evocore_error_t evocore_cpu_evaluate_batch(const evocore_eval_batch_t *batch, evocore_fitness_func_t fitness_func,
                                            void *user_context, int num_threads, evocore_eval_result_t *result);

// Memory
evocore_error_t evocore_gpu_malloc(evocore_gpu_context_t *ctx, size_t size, void **d_ptr);
evocore_error_t evocore_gpu_free(evocore_gpu_context_t *ctx, void *d_ptr);
evocore_error_t evocore_gpu_memcpy_h2d(evocore_gpu_context_t *ctx, const void *h_ptr, void *d_ptr, size_t size);
evocore_error_t evocore_gpu_memcpy_d2h(evocore_gpu_context_t *ctx, const void *d_ptr, void *h_ptr, size_t size);

// Utilities
size_t evocore_gpu_recommend_batch_size(const evocore_gpu_context_t *ctx, size_t genome_size);
bool evocore_gpu_batch_fits(const evocore_gpu_context_t *ctx, size_t genome_count, size_t genome_size);
evocore_error_t evocore_gpu_synchronize(evocore_gpu_context_t *ctx);
const char* evocore_gpu_get_error_string(evocore_gpu_context_t *ctx);

// Performance
evocore_error_t evocore_gpu_get_stats(const evocore_gpu_context_t *ctx, evocore_gpu_stats_t *stats);
void evocore_gpu_reset_stats(evocore_gpu_context_t *ctx);
void evocore_gpu_set_enabled(evocore_gpu_context_t *ctx, bool enabled);
bool evocore_gpu_get_enabled(const evocore_gpu_context_t *ctx);
""")

# =============================================================================
# C Source Configuration (set_source)
# =============================================================================

# Find library paths
EVOCORE_ROOT = os.environ.get('EVOCORE_ROOT', '/home/wao/Projects/Aion/evocore')
EVOCORE_BUILD = os.path.join(EVOCORE_ROOT, 'build')
EVOCORE_INCLUDE = os.path.join(EVOCORE_ROOT, 'include')

ffi.set_source(
    "evocore._evocore",
    """
    #include "evocore/evocore.h"
    """,
    libraries=["evocore"],
    library_dirs=[EVOCORE_BUILD, "/usr/local/lib"],
    include_dirs=[EVOCORE_INCLUDE, "/usr/local/include"],
    extra_compile_args=["-O2"],
)

if __name__ == "__main__":
    ffi.compile(verbose=True)
