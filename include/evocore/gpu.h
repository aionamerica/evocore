#ifndef EVOCORE_GPU_H
#define EVOCORE_GPU_H

#include "evocore/genome.h"
#include "evocore/fitness.h"
#include "evocore/error.h"
#include <stddef.h>
#include <stdbool.h>

/*========================================================================
 * GPU Acceleration Layer
 * ======================================================================
 *
 * Provides GPU acceleration for fitness evaluation with graceful
 * CPU fallback when CUDA is not available.
 *
 * Features:
 * - Batch evaluation of multiple genomes in parallel
 * - Automatic CPU/GPU selection based on availability
 * - Memory-mapped buffer transfers
 * - Multi-GPU support (future)
 */

/*========================================================================
 * Opaque Context
 *========================================================================*/

typedef struct evocore_gpu_context_s evocore_gpu_context_t;

/**
 * Maximum number of GPUs supported
 */
#define EVOCORE_MAX_GPUS 4

/**
 * GPU device information
 */
typedef struct {
    int device_id;           /* CUDA device ID (0-based) */
    char name[256];          /* Device name */
    size_t total_memory;     /* Total memory in bytes */
    size_t free_memory;      /* Free memory in bytes */
    int compute_capability_major;
    int compute_capability_minor;
    int multiprocessor_count;
    int max_threads_per_block;
    int max_threads_per_multiprocessor;
    bool available;          /* true if device is accessible */
} evocore_gpu_device_t;

/*========================================================================
 * Batch Evaluation
 *========================================================================*/

/**
 * Batch evaluation request
 */
typedef struct {
    const evocore_genome_t **genomes;   /* Array of genome pointers */
    double *fitnesses;                 /* Output fitness array */
    size_t count;                      /* Number of genomes */
    size_t genome_size;                /* Size of each genome in bytes */
} evocore_eval_batch_t;

/**
 * Batch evaluation result
 */
typedef struct {
    size_t evaluated;      /* Number of genomes successfully evaluated */
    double gpu_time_ms;    /* Time spent on GPU (milliseconds) */
    double cpu_time_ms;    /* Time spent on CPU (milliseconds) */
    bool used_gpu;         /* true if GPU was used */
} evocore_eval_result_t;

/*========================================================================
 * GPU Context Management
 *========================================================================*/

/**
 * Initialize GPU subsystem
 *
 * Probes for CUDA availability and sets up the GPU context.
 * Safe to call even without CUDA - will set cuda_available=false.
 * Allocates the context structure which must be freed with evocore_gpu_shutdown.
 *
 * @return New GPU context, or NULL on failure
 */
evocore_gpu_context_t* evocore_gpu_init(void);

/**
 * Shutdown GPU subsystem
 *
 * Releases all GPU resources and frees the context.
 *
 * @param ctx       GPU context to cleanup (can be NULL)
 */
void evocore_gpu_shutdown(evocore_gpu_context_t *ctx);

/**
 * Check if GPU is available
 *
 * @param ctx       GPU context
 * @return true if CUDA is available and device found, false otherwise
 */
bool evocore_gpu_available(const evocore_gpu_context_t *ctx);

/**
 * Get number of available GPU devices
 *
 * @param ctx       GPU context
 * @return Number of devices (0 if none available)
 */
int evocore_gpu_device_count(const evocore_gpu_context_t *ctx);

/**
 * Get device information
 *
 * @param ctx       GPU context
 * @param device_id Device ID (0 to device_count-1)
 * @return Device info pointer, or NULL if invalid
 */
const evocore_gpu_device_t* evocore_gpu_get_device(const evocore_gpu_context_t *ctx,
                                                int device_id);

/**
 * Select GPU device for operations
 *
 * @param ctx       GPU context
 * @param device_id Device ID to select
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_select_device(evocore_gpu_context_t *ctx,
                                      int device_id);

/**
 * Get current GPU device
 *
 * @param ctx       GPU context
 * @return Currently selected device ID, or -1 if none
 */
int evocore_gpu_get_current_device(const evocore_gpu_context_t *ctx);

/**
 * Print GPU device information
 *
 * @param ctx       GPU context
 */
void evocore_gpu_print_info(const evocore_gpu_context_t *ctx);

/*========================================================================
 * Batch Fitness Evaluation
 *========================================================================*/

/**
 * Evaluate batch of genomes using GPU (with CPU fallback)
 *
 * Evaluates multiple genomes in parallel. If GPU is available,
 * uses CUDA acceleration. Falls back to CPU parallel evaluation.
 *
 * @param ctx           GPU context
 * @param batch         Batch evaluation request
 * @param fitness_func  Fitness function (for CPU fallback)
 * @param user_context  User context for fitness function
 * @param result        Output: evaluation results
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_evaluate_batch(evocore_gpu_context_t *ctx,
                                       const evocore_eval_batch_t *batch,
                                       evocore_fitness_func_t fitness_func,
                                       void *user_context,
                                       evocore_eval_result_t *result);

/**
 * Evaluate batch of genomes using CPU (threaded)
 *
 * Always uses CPU, potentially with multiple threads.
 *
 * @param batch         Batch evaluation request
 * @param fitness_func  Fitness function
 * @param user_context  User context for fitness function
 * @param num_threads   Number of threads to use (0 = auto)
 * @param result        Output: evaluation results
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_cpu_evaluate_batch(const evocore_eval_batch_t *batch,
                                       evocore_fitness_func_t fitness_func,
                                       void *user_context,
                                       int num_threads,
                                       evocore_eval_result_t *result);

/*========================================================================
 * Memory Management
 *========================================================================*/

/**
 * Allocate GPU memory for genome data
 *
 * @param ctx       GPU context
 * @param size      Size in bytes
 * @param d_ptr     Output: device pointer
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_malloc(evocore_gpu_context_t *ctx,
                               size_t size,
                               void **d_ptr);

/**
 * Free GPU memory
 *
 * @param ctx       GPU context
 * @param d_ptr     Device pointer to free
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_free(evocore_gpu_context_t *ctx,
                             void *d_ptr);

/**
 * Copy data from host to device
 *
 * @param ctx       GPU context
 * @param h_ptr     Host pointer
 * @param d_ptr     Device pointer
 * @param size      Size in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_memcpy_h2d(evocore_gpu_context_t *ctx,
                                   const void *h_ptr,
                                   void *d_ptr,
                                   size_t size);

/**
 * Copy data from device to host
 *
 * @param ctx       GPU context
 * @param d_ptr     Device pointer
 * @param h_ptr     Host pointer
 * @param size      Size in bytes
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_memcpy_d2h(evocore_gpu_context_t *ctx,
                                   const void *d_ptr,
                                   void *h_ptr,
                                   size_t size);

/*========================================================================
 * Utility Functions
 *========================================================================*/

/**
 * Get recommended batch size for device
 *
 * Returns optimal batch size based on GPU memory and capabilities.
 *
 * @param ctx       GPU context
 * @param genome_size Size of each genome in bytes
 * @return Recommended batch size (>= 1)
 */
size_t evocore_gpu_recommend_batch_size(const evocore_gpu_context_t *ctx,
                                      size_t genome_size);

/**
 * Check if batch would fit in GPU memory
 *
 * @param ctx           GPU context
 * @param genome_count  Number of genomes
 * @param genome_size   Size of each genome
 * @return true if it fits, false otherwise
 */
bool evocore_gpu_batch_fits(const evocore_gpu_context_t *ctx,
                          size_t genome_count,
                          size_t genome_size);

/**
 * Synchronize GPU operations
 *
 * Blocks until all GPU operations complete.
 *
 * @param ctx       GPU context
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_synchronize(evocore_gpu_context_t *ctx);

/**
 * Get last GPU error string
 *
 * @param ctx       GPU context
 * @return Error string, or "No error"
 */
const char* evocore_gpu_get_error_string(evocore_gpu_context_t *ctx);

/*========================================================================
 * Performance Monitoring
 *========================================================================*/

/**
 * GPU performance statistics
 */
typedef struct {
    size_t total_evaluations;    /* Total genomes evaluated */
    size_t gpu_evaluations;      /* Evaluations done on GPU */
    size_t cpu_evaluations;      /* Evaluations done on CPU */
    double total_gpu_time_ms;    /* Total GPU time */
    double total_cpu_time_ms;    /* Total CPU time */
    double avg_gpu_time_ms;      /* Average time per GPU batch */
    double avg_cpu_time_ms;      /* Average time per CPU batch */
} evocore_gpu_stats_t;

/**
 * Get GPU performance statistics
 *
 * @param ctx       GPU context
 * @param stats     Output: statistics
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_gpu_get_stats(const evocore_gpu_context_t *ctx,
                                   evocore_gpu_stats_t *stats);

/**
 * Reset GPU performance statistics
 *
 * @param ctx       GPU context
 */
void evocore_gpu_reset_stats(evocore_gpu_context_t *ctx);

/**
 * Enable/disable GPU acceleration
 *
 * When disabled, all evaluations use CPU even if GPU is available.
 *
 * @param ctx       GPU context
 * @param enabled   true to enable, false to disable
 */
void evocore_gpu_set_enabled(evocore_gpu_context_t *ctx, bool enabled);

/**
 * Check if GPU acceleration is enabled
 *
 * @param ctx       GPU context
 * @return true if enabled, false otherwise
 */
bool evocore_gpu_get_enabled(const evocore_gpu_context_t *ctx);

#endif /* EVOCORE_GPU_H */
