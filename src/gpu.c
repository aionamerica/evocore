#define _GNU_SOURCE
#include "evocore/gpu.h"
#include "internal.h"
#include "evocore/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef EVOCORE_HAVE_PTHREADS
#include <pthread.h>
#endif

/*========================================================================
 * CUDA Detection (Runtime)
 *========================================================================*/

#ifdef EVOCORE_HAVE_CUDA
#include <cuda_runtime.h>

/* External CUDA functions from fitness.cu */
extern int cuda_batch_evaluate_sync(
    const void* d_genomes,
    void* d_fitnesses,
    size_t genome_size,
    int count,
    int fitness_type
);

extern void* cuda_copy_genomes_to_device(
    const void* h_genomes,
    size_t genome_size,
    int count
);

extern int cuda_copy_fitnesses_from_device(
    void* h_fitnesses,
    const void* d_fitnesses,
    int count
);

extern void cuda_free_device_memory(void* d_ptr);

extern const char* cuda_get_error_string(void);

#endif /* EVOCORE_HAVE_CUDA */

/*========================================================================
 * GPU Context State
 *========================================================================*/

struct evocore_gpu_context_s {
    bool initialized;
    bool cuda_available;
    bool gpu_enabled;
    int device_count;
    evocore_gpu_device_t devices[EVOCORE_MAX_GPUS];
    int current_device;
    size_t max_batch_size;

    /* Performance stats */
    evocore_gpu_stats_t stats;

    /* Last error */
    char last_error[256];
};

/*========================================================================
 * GPU Context Management
 *========================================================================*/

evocore_gpu_context_t* evocore_gpu_init(void) {
    evocore_gpu_context_t *ctx = evocore_calloc(1, sizeof(evocore_gpu_context_t));
    if (ctx == NULL) {
        return NULL;
    }

    ctx->initialized = true;
    ctx->gpu_enabled = true;
    ctx->current_device = -1;
    ctx->max_batch_size = 1000;  /* Default for CPU */

#ifdef EVOCORE_HAVE_CUDA
    /* Probe for CUDA devices */
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);

    if (err == cudaSuccess && device_count > 0) {
        ctx->cuda_available = true;
        ctx->device_count = device_count > EVOCORE_MAX_GPUS ? EVOCORE_MAX_GPUS : device_count;

        for (int i = 0; i < ctx->device_count; i++) {
            cudaDeviceProp prop;
            if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
                evocore_gpu_device_t *dev = &ctx->devices[i];
                dev->device_id = i;
                snprintf(dev->name, sizeof(dev->name), "%s", prop.name);
                dev->total_memory = prop.totalGlobalMem;
                dev->compute_capability_major = prop.major;
                dev->compute_capability_minor = prop.minor;
                dev->multiprocessor_count = prop.multiProcessorCount;
                dev->max_threads_per_block = prop.maxThreadsPerBlock;
                dev->max_threads_per_multiprocessor = prop.maxThreadsPerMultiProcessor;
                dev->available = true;

                /* Get free memory */
                size_t free_mem, total_mem;
                if (cudaMemGetInfo(&free_mem, &total_mem) == cudaSuccess) {
                    dev->free_memory = free_mem;
                }
            }
        }

        /* Select first device by default */
        evocore_gpu_select_device(ctx, 0);

        evocore_log_info("CUDA detected: %d device(s)", ctx->device_count);
    } else {
        ctx->cuda_available = false;
        evocore_log_info("CUDA not available - using CPU for evaluations");
    }
#else
    ctx->cuda_available = false;
    evocore_log_info("Built without CUDA support - using CPU for evaluations");
#endif

    return ctx;
}

void evocore_gpu_shutdown(evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        evocore_free(ctx);
        return;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaDeviceReset();
    }
#endif

    evocore_free(ctx);
}

bool evocore_gpu_available(const evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    return ctx->cuda_available && ctx->gpu_enabled;
}

int evocore_gpu_device_count(const evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return 0;
    }
    return ctx->device_count;
}

const evocore_gpu_device_t* evocore_gpu_get_device(const evocore_gpu_context_t *ctx,
                                                int device_id) {
    if (ctx == NULL || !ctx->initialized) {
        return NULL;
    }
    if (device_id < 0 || device_id >= ctx->device_count) {
        return NULL;
    }
    return &ctx->devices[device_id];
}

evocore_error_t evocore_gpu_select_device(evocore_gpu_context_t *ctx,
                                      int device_id) {
    if (ctx == NULL || !ctx->initialized) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (device_id < 0 || device_id >= ctx->device_count) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Invalid device ID: %d", device_id);
        return EVOCORE_ERR_INVALID_ARG;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaError_t err = cudaSetDevice(device_id);
        if (err != cudaSuccess) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "cudaSetDevice failed: %s", cudaGetErrorString(err));
            return EVOCORE_ERR_UNKNOWN;
        }
    }
#endif

    ctx->current_device = device_id;
    return EVOCORE_OK;
}

int evocore_gpu_get_current_device(const evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return -1;
    }
    return ctx->current_device;
}

void evocore_gpu_print_info(const evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        printf("GPU context not initialized\n");
        return;
    }

    printf("=== GPU Information ===\n");
    printf("CUDA Available: %s\n", ctx->cuda_available ? "Yes" : "No");
    printf("GPU Enabled: %s\n", ctx->gpu_enabled ? "Yes" : "No");
    printf("Device Count: %d\n", ctx->device_count);
    printf("Current Device: %d\n", ctx->current_device);
    printf("Max Batch Size: %zu\n", ctx->max_batch_size);

    if (ctx->device_count > 0) {
        printf("\nDevices:\n");
        for (int i = 0; i < ctx->device_count; i++) {
            evocore_gpu_device_t *dev = &ctx->devices[i];
            printf("  [%d] %s\n", i, dev->name);
            printf("      Memory: %zu MB total / %zu MB free\n",
                   dev->total_memory / (1024*1024),
                   dev->free_memory / (1024*1024));
            printf("      Compute: %d.%d\n",
                   dev->compute_capability_major,
                   dev->compute_capability_minor);
            printf("      Multiprocessors: %d\n", dev->multiprocessor_count);
        }
    }

    printf("\nPerformance Statistics:\n");
    printf("  Total Evaluations: %zu\n", ctx->stats.total_evaluations);
    printf("  GPU Evaluations: %zu (%.1f%%)\n",
           ctx->stats.gpu_evaluations,
           ctx->stats.total_evaluations > 0 ?
           (100.0 * ctx->stats.gpu_evaluations / ctx->stats.total_evaluations) : 0.0);
    printf("  CPU Evaluations: %zu (%.1f%%)\n",
           ctx->stats.cpu_evaluations,
           ctx->stats.total_evaluations > 0 ?
           (100.0 * ctx->stats.cpu_evaluations / ctx->stats.total_evaluations) : 0.0);
    printf("  Total GPU Time: %.2f ms\n", ctx->stats.total_gpu_time_ms);
    printf("  Total CPU Time: %.2f ms\n", ctx->stats.total_cpu_time_ms);
}

/*========================================================================
 * Batch Fitness Evaluation
 *========================================================================*/

typedef struct {
    const evocore_genome_t **genomes;
    double *fitnesses;
    size_t count;
    evocore_fitness_func_t fitness_func;
    void *user_context;
    size_t start;
    size_t end;
} cpu_eval_task_t;

static void* cpu_eval_worker(void *arg) {
    cpu_eval_task_t *task = (cpu_eval_task_t*)arg;

    for (size_t i = task->start; i < task->end; i++) {
        if (task->genomes[i] != NULL && task->fitness_func != NULL) {
            task->fitnesses[i] = task->fitness_func(task->genomes[i],
                                                    task->user_context);
        }
    }

    return NULL;
}

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

evocore_error_t evocore_gpu_evaluate_batch(evocore_gpu_context_t *ctx,
                                       const evocore_eval_batch_t *batch,
                                       evocore_fitness_func_t fitness_func,
                                       void *user_context,
                                       evocore_eval_result_t *result) {
    if (ctx == NULL || !ctx->initialized) {
        return EVOCORE_ERR_NULL_PTR;
    }
    if (batch == NULL || batch->genomes == NULL || batch->fitnesses == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }
    if (result == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    memset(result, 0, sizeof(evocore_eval_result_t));
    result->evaluated = 0;

    /* Try GPU first if available */
    if (ctx->cuda_available && ctx->gpu_enabled) {
#ifdef EVOCORE_HAVE_CUDA
        double gpu_start = get_time_ms();

        /* Flatten genome data for GPU transfer */
        size_t total_size = batch->genome_size * batch->count;
        uint8_t *flat_genomes = (uint8_t*)evocore_malloc(total_size);
        if (flat_genomes) {
            /* Flatten genomes */
            for (size_t i = 0; i < batch->count; i++) {
                if (batch->genomes[i] != NULL) {
                    memcpy(flat_genomes + i * batch->genome_size,
                           batch->genomes[i]->data,
                           batch->genomes[i]->size < batch->genome_size ?
                               batch->genomes[i]->size : batch->genome_size);
                    /* Zero pad if needed */
                    if (batch->genomes[i]->size < batch->genome_size) {
                        memset(flat_genomes + i * batch->genome_size + batch->genomes[i]->size,
                               0, batch->genome_size - batch->genomes[i]->size);
                    }
                }
            }

            /* Copy to device */
            void *d_genomes = cuda_copy_genomes_to_device(flat_genomes,
                                                          batch->genome_size,
                                                          batch->count);
            void *d_fitnesses = NULL;
            cudaMalloc(&d_fitnesses, batch->count * sizeof(double));

            if (d_genomes && d_fitnesses) {
                /* Evaluate on GPU */
                int cuda_result = cuda_batch_evaluate_sync(d_genomes, d_fitnesses,
                                                           batch->genome_size,
                                                           batch->count,
                                                           0);  /* FITNESS_SPHERE */

                if (cuda_result > 0) {
                    /* Copy results back */
                    if (cuda_copy_fitnesses_from_device(batch->fitnesses,
                                                        d_fitnesses,
                                                        batch->count)) {
                        result->evaluated = batch->count;
                        result->used_gpu = true;
                    }
                }
            }

            /* Cleanup */
            if (d_genomes) cuda_free_device_memory(d_genomes);
            if (d_fitnesses) cudaFree(d_fitnesses);
            evocore_free(flat_genomes);

            double gpu_end = get_time_ms();
            result->gpu_time_ms = gpu_end - gpu_start;
        }

        /* If GPU evaluation failed, fall through to CPU */
        if (result->evaluated > 0) {
            ctx->stats.total_evaluations += result->evaluated;
            ctx->stats.gpu_evaluations += result->evaluated;
            ctx->stats.total_gpu_time_ms += result->gpu_time_ms;
            ctx->stats.avg_gpu_time_ms = ctx->stats.total_gpu_time_ms /
                                         (ctx->stats.gpu_evaluations > 0 ?
                                          ctx->stats.gpu_evaluations : 1);
            return EVOCORE_OK;
        }
#endif
    }

    /* CPU evaluation with pthread support */
    double start_time = get_time_ms();

#ifdef EVOCORE_HAVE_PTHREADS
    int num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_threads < 1) num_threads = 1;
    if (num_threads > 16) num_threads = 16;  /* Cap at 16 threads */

    if (batch->count > 10 && num_threads > 1) {
        /* Parallel evaluation */
        pthread_t *threads = evocore_calloc(num_threads, sizeof(pthread_t));
        cpu_eval_task_t *tasks = evocore_calloc(num_threads, sizeof(cpu_eval_task_t));
        size_t chunk_size = (batch->count + num_threads - 1) / num_threads;

        if (threads && tasks) {
            for (int t = 0; t < num_threads; t++) {
                tasks[t].genomes = batch->genomes;
                tasks[t].fitnesses = batch->fitnesses;
                tasks[t].count = batch->count;
                tasks[t].fitness_func = fitness_func;
                tasks[t].user_context = user_context;
                tasks[t].start = t * chunk_size;
                tasks[t].end = (t + 1) * chunk_size;
                if (tasks[t].end > batch->count) tasks[t].end = batch->count;

                if (tasks[t].start < tasks[t].end) {
                    pthread_create(&threads[t], NULL, cpu_eval_worker, &tasks[t]);
                }
            }

            /* Wait for all threads */
            for (int t = 0; t < num_threads; t++) {
                if (tasks[t].start < tasks[t].end) {
                    pthread_join(threads[t], NULL);
                }
            }

            result->evaluated = batch->count;
        }

        evocore_free(threads);
        evocore_free(tasks);
    } else
#endif
    {
        /* Serial evaluation */
        for (size_t i = 0; i < batch->count; i++) {
            if (batch->genomes[i] != NULL && fitness_func != NULL) {
                batch->fitnesses[i] = fitness_func(batch->genomes[i], user_context);
                result->evaluated++;
            }
        }
    }

    double end_time = get_time_ms();
    result->cpu_time_ms = end_time - start_time;
    result->used_gpu = false;

    /* Update stats */
    ctx->stats.total_evaluations += result->evaluated;
    ctx->stats.cpu_evaluations += result->evaluated;
    ctx->stats.total_cpu_time_ms += result->cpu_time_ms;
    ctx->stats.avg_cpu_time_ms = ctx->stats.total_cpu_time_ms /
                                  (ctx->stats.cpu_evaluations > 0 ?
                                   ctx->stats.cpu_evaluations : 1);

    return EVOCORE_OK;
}

evocore_error_t evocore_cpu_evaluate_batch(const evocore_eval_batch_t *batch,
                                       evocore_fitness_func_t fitness_func,
                                       void *user_context,
                                       int num_threads,
                                       evocore_eval_result_t *result) {
    if (batch == NULL || batch->genomes == NULL || batch->fitnesses == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }
    if (result == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    memset(result, 0, sizeof(evocore_eval_result_t));
    result->evaluated = 0;
    result->used_gpu = false;

    double start_time = get_time_ms();

#ifdef EVOCORE_HAVE_PTHREADS
    /* Determine thread count */
    if (num_threads <= 0) {
        num_threads = sysconf(_SC_NPROCESSORS_ONLN);
        if (num_threads < 1) num_threads = 1;
        if (num_threads > 16) num_threads = 16;
    }

    /* Use pthread if batch is large enough */
    if (batch->count > 10 && num_threads > 1) {
        pthread_t *threads = evocore_calloc(num_threads, sizeof(pthread_t));
        cpu_eval_task_t *tasks = evocore_calloc(num_threads, sizeof(cpu_eval_task_t));
        size_t chunk_size = (batch->count + num_threads - 1) / num_threads;

        if (threads && tasks) {
            for (int t = 0; t < num_threads; t++) {
                tasks[t].genomes = batch->genomes;
                tasks[t].fitnesses = batch->fitnesses;
                tasks[t].count = batch->count;
                tasks[t].fitness_func = fitness_func;
                tasks[t].user_context = user_context;
                tasks[t].start = t * chunk_size;
                tasks[t].end = (t + 1) * chunk_size;
                if (tasks[t].end > batch->count) tasks[t].end = batch->count;

                if (tasks[t].start < tasks[t].end) {
                    pthread_create(&threads[t], NULL, cpu_eval_worker, &tasks[t]);
                }
            }

            /* Wait for all threads */
            for (int t = 0; t < num_threads; t++) {
                if (tasks[t].start < tasks[t].end) {
                    pthread_join(threads[t], NULL);
                }
            }

            result->evaluated = batch->count;
        }

        evocore_free(threads);
        evocore_free(tasks);
    } else
#endif
    {
        /* Serial evaluation */
        for (size_t i = 0; i < batch->count; i++) {
            if (batch->genomes[i] != NULL && fitness_func != NULL) {
                batch->fitnesses[i] = fitness_func(batch->genomes[i], user_context);
                result->evaluated++;
            }
        }
    }

    double end_time = get_time_ms();
    result->cpu_time_ms = end_time - start_time;

    return EVOCORE_OK;
}

/*========================================================================
 * Memory Management (Stubs for CPU fallback)
 *========================================================================*/

evocore_error_t evocore_gpu_malloc(evocore_gpu_context_t *ctx,
                               size_t size,
                               void **d_ptr) {
    if (ctx == NULL || d_ptr == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaError_t err = cudaMalloc(d_ptr, size);
        if (err != cudaSuccess) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "cudaMalloc failed: %s", cudaGetErrorString(err));
            return EVOCORE_ERR_OUT_OF_MEMORY;
        }
        return EVOCORE_OK;
    }
#endif

    /* CPU fallback - allocate regular memory */
    *d_ptr = evocore_malloc(size);
    if (*d_ptr == NULL) {
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }
    return EVOCORE_OK;
}

evocore_error_t evocore_gpu_free(evocore_gpu_context_t *ctx,
                             void *d_ptr) {
    if (ctx == NULL || d_ptr == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaError_t err = cudaFree(d_ptr);
        if (err != cudaSuccess) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "cudaFree failed: %s", cudaGetErrorString(err));
            return EVOCORE_ERR_UNKNOWN;
        }
        return EVOCORE_OK;
    }
#endif

    /* CPU fallback */
    evocore_free(d_ptr);
    return EVOCORE_OK;
}

evocore_error_t evocore_gpu_memcpy_h2d(evocore_gpu_context_t *ctx,
                                   const void *h_ptr,
                                   void *d_ptr,
                                   size_t size) {
    if (ctx == NULL || h_ptr == NULL || d_ptr == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaError_t err = cudaMemcpy(d_ptr, h_ptr, size, cudaMemcpyHostToDevice);
        if (err != cudaSuccess) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "cudaMemcpy H2D failed: %s", cudaGetErrorString(err));
            return EVOCORE_ERR_UNKNOWN;
        }
        return EVOCORE_OK;
    }
#endif

    /* CPU fallback - just memcpy */
    memcpy(d_ptr, h_ptr, size);
    return EVOCORE_OK;
}

evocore_error_t evocore_gpu_memcpy_d2h(evocore_gpu_context_t *ctx,
                                   const void *d_ptr,
                                   void *h_ptr,
                                   size_t size) {
    if (ctx == NULL || d_ptr == NULL || h_ptr == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaError_t err = cudaMemcpy(h_ptr, d_ptr, size, cudaMemcpyDeviceToHost);
        if (err != cudaSuccess) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "cudaMemcpy D2H failed: %s", cudaGetErrorString(err));
            return EVOCORE_ERR_UNKNOWN;
        }
        return EVOCORE_OK;
    }
#endif

    /* CPU fallback - just memcpy */
    memcpy(h_ptr, d_ptr, size);
    return EVOCORE_OK;
}

/*========================================================================
 * Utility Functions
 *========================================================================*/

size_t evocore_gpu_recommend_batch_size(const evocore_gpu_context_t *ctx,
                                      size_t genome_size) {
    if (ctx == NULL || !ctx->initialized) {
        return 100;  /* Default */
    }

    if (ctx->cuda_available && ctx->current_device >= 0) {
        evocore_gpu_device_t *dev = &ctx->devices[ctx->current_device];
        /* Use 10% of available memory, divided by genome size */
        size_t usable_memory = dev->free_memory / 10;
        size_t batch = usable_memory / (genome_size * 2);  /* *2 for output buffer */
        if (batch < 1) batch = 1;
        if (batch > 10000) batch = 10000;
        return batch;
    }

    return 100;  /* CPU default */
}

bool evocore_gpu_batch_fits(const evocore_gpu_context_t *ctx,
                          size_t genome_count,
                          size_t genome_size) {
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }

    if (!ctx->cuda_available) {
        return true;  /* CPU always "fits" */
    }

    if (ctx->current_device < 0) {
        return false;
    }

    evocore_gpu_device_t *dev = &ctx->devices[ctx->current_device];
    size_t required = genome_count * genome_size * 2;  /* Input + output */

    return required <= dev->free_memory;
}

evocore_error_t evocore_gpu_synchronize(evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return EVOCORE_ERR_NULL_PTR;
    }

#ifdef EVOCORE_HAVE_CUDA
    if (ctx->cuda_available) {
        cudaError_t err = cudaDeviceSynchronize();
        if (err != cudaSuccess) {
            snprintf(ctx->last_error, sizeof(ctx->last_error),
                     "cudaDeviceSynchronize failed: %s", cudaGetErrorString(err));
            return EVOCORE_ERR_UNKNOWN;
        }
    }
#endif

    return EVOCORE_OK;
}

const char* evocore_gpu_get_error_string(evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return "Context not initialized";
    }
    if (ctx->last_error[0] == '\0') {
        return "No error";
    }
    return ctx->last_error;
}

/*========================================================================
 * Performance Monitoring
 *========================================================================*/

evocore_error_t evocore_gpu_get_stats(const evocore_gpu_context_t *ctx,
                                   evocore_gpu_stats_t *stats) {
    if (ctx == NULL || stats == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    memcpy(stats, &ctx->stats, sizeof(evocore_gpu_stats_t));
    return EVOCORE_OK;
}

void evocore_gpu_reset_stats(evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return;
    }

    memset(&ctx->stats, 0, sizeof(evocore_gpu_stats_t));
}

void evocore_gpu_set_enabled(evocore_gpu_context_t *ctx, bool enabled) {
    if (ctx == NULL || !ctx->initialized) {
        return;
    }
    ctx->gpu_enabled = enabled;
}

bool evocore_gpu_get_enabled(const evocore_gpu_context_t *ctx) {
    if (ctx == NULL || !ctx->initialized) {
        return false;
    }
    return ctx->gpu_enabled;
}
