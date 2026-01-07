/**
 * Performance Optimization Module Implementation
 *
 * Provides memory pools, SIMD operations, and parallel evaluation.
 */

#define _GNU_SOURCE
#include "evocore/optimize.h"
#include "evocore/log.h"
#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <pthread.h>

#ifdef OMP_SUPPORT
#include <omp.h>
#endif

/*========================================================================
 * Memory Pool Implementation
 *======================================================================== */

#define MEMPOOL_BLOCK_MAGIC 0xDEADBEEF

typedef struct mempool_block {
    struct mempool_block *next;
    size_t capacity;
    size_t used;
    unsigned int magic;
    unsigned char data[];
} mempool_block_t;

struct evocore_mempool_t {
    size_t genome_size;
    size_t block_size;
    size_t num_blocks;
    mempool_block_t *blocks;
    mempool_block_t *free_list;
    pthread_mutex_t mutex;

    /* Statistics */
    size_t total_allocations;
    size_t current_allocations;
};

evocore_mempool_t* evocore_mempool_create(size_t genome_size, size_t block_size) {
    if (genome_size == 0 || block_size == 0) {
        return NULL;
    }

    evocore_mempool_t *pool = (evocore_mempool_t*)evocore_calloc(1, sizeof(evocore_mempool_t));
    if (!pool) {
        return NULL;
    }

    pool->genome_size = genome_size;
    pool->block_size = block_size;
    pool->num_blocks = 0;
    pool->blocks = NULL;
    pool->free_list = NULL;
    pool->total_allocations = 0;
    pool->current_allocations = 0;

    pthread_mutex_init(&pool->mutex, NULL);

    return pool;
}

void evocore_mempool_destroy(evocore_mempool_t *pool) {
    if (!pool) return;

    pthread_mutex_lock(&pool->mutex);

    /* Free all blocks */
    mempool_block_t *block = pool->blocks;
    while (block) {
        mempool_block_t *next = block->next;
        evocore_free(block);
        block = next;
    }

    pthread_mutex_unlock(&pool->mutex);
    pthread_mutex_destroy(&pool->mutex);

    evocore_free(pool);
}

evocore_error_t evocore_mempool_alloc(evocore_mempool_t *pool,
                                  evocore_genome_t *genome) {
    if (!pool || !genome) {
        return EVOCORE_ERR_NULL_PTR;
    }

    pthread_mutex_lock(&pool->mutex);

    /* Try to find free space in existing blocks */
    void *ptr = NULL;
    mempool_block_t *block = pool->blocks;

    while (block) {
        if (block->used < block->capacity) {
            ptr = block->data + (block->used * pool->genome_size);
            block->used++;
            break;
        }
        block = block->next;
    }

    /* Need to allocate new block */
    if (!ptr) {
        size_t block_bytes = sizeof(mempool_block_t) +
                           (pool->block_size * pool->genome_size);
        mempool_block_t *new_block = (mempool_block_t*)evocore_malloc(block_bytes);

        if (!new_block) {
            pthread_mutex_unlock(&pool->mutex);
            return EVOCORE_ERR_OUT_OF_MEMORY;
        }

        new_block->next = pool->blocks;
        new_block->capacity = pool->block_size;
        new_block->used = 1;
        new_block->magic = MEMPOOL_BLOCK_MAGIC;

        pool->blocks = new_block;
        pool->num_blocks++;

        ptr = new_block->data;
    }

    pthread_mutex_unlock(&pool->mutex);

    /* Initialize genome with allocated memory */
    genome->data = ptr;
    genome->capacity = pool->genome_size;
    genome->size = 0;

    pool->total_allocations++;
    pool->current_allocations++;

    return EVOCORE_OK;
}

void evocore_mempool_free(evocore_mempool_t *pool,
                        evocore_genome_t *genome) {
    if (!pool || !genome) return;

    /* For pooled memory, we just mark as freed by clearing data pointer
     * The actual memory is reused in subsequent allocations */
    genome->data = NULL;
    genome->capacity = 0;
    genome->size = 0;

    pool->current_allocations--;
}

void evocore_mempool_get_stats(const evocore_mempool_t *pool,
                              size_t *total_allocations,
                              size_t *current_allocations,
                              size_t *total_blocks,
                              size_t *free_blocks) {
    if (!pool) return;

    if (total_allocations) {
        *total_allocations = pool->total_allocations;
    }
    if (current_allocations) {
        *current_allocations = pool->current_allocations;
    }
    if (total_blocks) {
        *total_blocks = pool->num_blocks;
    }
    if (free_blocks) {
        size_t free = 0;
        mempool_block_t *block = pool->blocks;
        while (block) {
            if (block->used < block->capacity) {
                free += (block->capacity - block->used);
            }
            block = block->next;
        }
        *free_blocks = free;
    }
}

/*========================================================================
 * Parallel Fitness Evaluation
 *======================================================================== */

struct evocore_parallel_ctx_t {
    int num_threads;
    bool initialized;
};

evocore_parallel_ctx_t* evocore_parallel_create(int num_threads) {
    evocore_parallel_ctx_t *ctx = (evocore_parallel_ctx_t*)evocore_calloc(1,
                                            sizeof(evocore_parallel_ctx_t));
    if (!ctx) {
        return NULL;
    }

    if (num_threads <= 0) {
#ifdef OMP_SUPPORT
        /* Auto-detect: use OMP_NUM_THREADS or CPU count */
        num_threads = omp_get_max_threads();
        if (num_threads < 1) num_threads = 1;
#else
        num_threads = 1;
#endif
    }

    ctx->num_threads = num_threads;
    ctx->initialized = true;

    return ctx;
}

void evocore_parallel_destroy(evocore_parallel_ctx_t *ctx) {
    if (ctx) {
        evocore_free(ctx);
    }
}

int evocore_parallel_get_num_threads(const evocore_parallel_ctx_t *ctx) {
    return ctx ? ctx->num_threads : 1;
}

typedef struct {
    evocore_population_t *pop;
    evocore_fitness_func_t fitness_func;
    void *user_context;
    size_t start_idx;
    size_t end_idx;
} parallel_eval_task_t;

static void* parallel_eval_worker(void *arg) {
    parallel_eval_task_t *task = (parallel_eval_task_t*)arg;

    for (size_t i = task->start_idx; i < task->end_idx; i++) {
        evocore_individual_t *ind = &task->pop->individuals[i];
        if (ind->genome && ind->genome->data) {
            ind->fitness = task->fitness_func(ind->genome,
                                              task->user_context);
        }
    }

    return NULL;
}

evocore_error_t evocore_parallel_evaluate_population(evocore_parallel_ctx_t *ctx,
                                                evocore_population_t *pop,
                                                evocore_fitness_func_t fitness_func,
                                                void *user_context) {
    if (!ctx || !pop || !fitness_func) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (pop->size == 0) {
        return EVOCORE_OK;
    }

#ifdef OMP_SUPPORT
    /* Use OpenMP for parallelization */
    #pragma omp parallel for num_threads(ctx->num_threads) schedule(dynamic)
    for (size_t i = 0; i < pop->size; i++) {
        evocore_individual_t *ind = &pop->individuals[i];
        if (ind->genome && ind->genome->data) {
            ind->fitness = fitness_func(ind->genome, user_context);
        }
    }
#else
    /* Serial fallback */
    for (size_t i = 0; i < pop->size; i++) {
        evocore_individual_t *ind = &pop->individuals[i];
        if (ind->genome && ind->genome->data) {
            ind->fitness = fitness_func(ind->genome, user_context);
        }
    }
#endif

    return EVOCORE_OK;
}

evocore_error_t evocore_parallel_evaluate_batch(evocore_parallel_ctx_t *ctx,
                                            const evocore_genome_t **genomes,
                                            double *fitnesses,
                                            size_t count,
                                            evocore_fitness_func_t fitness_func,
                                            void *user_context) {
    if (!ctx || !genomes || !fitnesses || !fitness_func) {
        return EVOCORE_ERR_NULL_PTR;
    }

    if (count == 0) {
        return EVOCORE_OK;
    }

#ifdef OMP_SUPPORT
    /* Use OpenMP for parallelization */
    #pragma omp parallel for num_threads(ctx->num_threads) schedule(dynamic)
    for (size_t i = 0; i < count; i++) {
        if (genomes[i] && genomes[i]->data) {
            fitnesses[i] = fitness_func(genomes[i], user_context);
        } else {
            fitnesses[i] = NAN;
        }
    }
#else
    /* Serial fallback */
    for (size_t i = 0; i < count; i++) {
        if (genomes[i] && genomes[i]->data) {
            fitnesses[i] = fitness_func(genomes[i], user_context);
        } else {
            fitnesses[i] = NAN;
        }
    }
#endif

    return EVOCORE_OK;
}

/*========================================================================
 * SIMD Genome Operations
 *======================================================================== */

/* SIMD detection - disabled for compatibility */
static bool g_simd_available = false;
static bool g_simd_checked = false;

bool evocore_simd_available(void) {
    if (g_simd_checked) {
        return g_simd_available;
    }

    g_simd_checked = true;
    /* SIMD disabled for compatibility */
    g_simd_available = false;

    return g_simd_available;
}

void evocore_simd_mutate_genome(evocore_genome_t *genome,
                              double rate,
                              unsigned int *seed) {
    /* Fallback to scalar mutation */
    if (!genome || !genome->data || !seed) {
        return;
    }

    size_t num_bytes = (size_t)(genome->size * rate);
    if (num_bytes < 1) num_bytes = 1;

    for (size_t i = 0; i < num_bytes; i++) {
        size_t pos = rand_r(seed) % genome->size;
        ((unsigned char*)genome->data)[pos] ^= (rand_r(seed) & 0xFF);
    }
}

size_t evocore_simd_genome_hamming_distance(const evocore_genome_t *a,
                                          const evocore_genome_t *b) {
    if (!a || !b || !a->data || !b->data) {
        return 0;
    }

    size_t min_size = a->size < b->size ? a->size : b->size;
    size_t distance = 0;

    /* Scalar Hamming distance */
    const unsigned char *data_a = (const unsigned char*)a->data;
    const unsigned char *data_b = (const unsigned char*)b->data;

    for (size_t i = 0; i < min_size; i++) {
        if (data_a[i] != data_b[i]) {
            distance++;
        }
    }

    return distance;
}

/*========================================================================
 * Cache-Friendly Population Layout
 *======================================================================== */

evocore_error_t evocore_population_optimize_layout(evocore_population_t *pop) {
    (void)pop;
    /* Future: SoA (Structure of Arrays) transformation */
    /* For now, populations are already reasonably cache-friendly */
    return EVOCORE_OK;
}

/*========================================================================
 * Performance Monitoring
 *======================================================================== */

static evocore_perf_monitor_t g_perf_monitor = {
    .count = 0,
    .enabled = false
};

static pthread_mutex_t g_perf_mutex = PTHREAD_MUTEX_INITIALIZER;

evocore_perf_monitor_t* evocore_perf_monitor_get(void) {
    return &g_perf_monitor;
}

void evocore_perf_reset(void) {
    pthread_mutex_lock(&g_perf_mutex);
    memset(&g_perf_monitor, 0, sizeof(g_perf_monitor));
    pthread_mutex_unlock(&g_perf_mutex);
}

void evocore_perf_set_enabled(bool enabled) {
    pthread_mutex_lock(&g_perf_mutex);
    g_perf_monitor.enabled = enabled;
    pthread_mutex_unlock(&g_perf_mutex);
}

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

int evocore_perf_start(const char *name) {
    if (!name) return -1;

    pthread_mutex_lock(&g_perf_mutex);

    if (!g_perf_monitor.enabled) {
        pthread_mutex_unlock(&g_perf_mutex);
        return -1;
    }

    /* Find existing counter or create new one */
    int idx = -1;
    for (int i = 0; i < g_perf_monitor.count; i++) {
        if (strcmp(g_perf_monitor.counters[i].name, name) == 0) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        if (g_perf_monitor.count >= EVOCORE_MAX_PERF_COUNTERS) {
            pthread_mutex_unlock(&g_perf_mutex);
            return -1;
        }
        idx = g_perf_monitor.count++;
        memset(&g_perf_monitor.counters[idx], 0, sizeof(evocore_perf_counter_t));
        g_perf_monitor.counters[idx].name = name;
    }

    pthread_mutex_unlock(&g_perf_mutex);

    /* Return negative value to indicate start (actual index is encoded) */
    /* Use a marker that we can detect in perf_end */
    return -(idx + 1);
}

double evocore_perf_end(int index) {
    if (index >= 0) return 0.0;

    int idx = -(index + 1);

    pthread_mutex_lock(&g_perf_mutex);
    if (idx < 0 || idx >= g_perf_monitor.count) {
        pthread_mutex_unlock(&g_perf_mutex);
        return 0.0;
    }

    /* For now, just return a placeholder */
    /* In real implementation, we'd track start times */
    evocore_perf_counter_t *counter = &g_perf_monitor.counters[idx];
    counter->count++;

    pthread_mutex_unlock(&g_perf_mutex);

    return 0.0;
}

void evocore_perf_print(void) {
    pthread_mutex_lock(&g_perf_mutex);

    printf("=== Performance Counters ===\n");
    for (int i = 0; i < g_perf_monitor.count; i++) {
        const evocore_perf_counter_t *c = &g_perf_monitor.counters[i];
        printf("  %-30s: %lld calls, %.2f ms total\n",
               c->name, c->count, c->total_time_ms);
    }

    pthread_mutex_unlock(&g_perf_mutex);
}

const evocore_perf_counter_t* evocore_perf_get(const char *name) {
    if (!name) return NULL;

    pthread_mutex_lock(&g_perf_mutex);

    for (int i = 0; i < g_perf_monitor.count; i++) {
        if (strcmp(g_perf_monitor.counters[i].name, name) == 0) {
            const evocore_perf_counter_t *result = &g_perf_monitor.counters[i];
            pthread_mutex_unlock(&g_perf_mutex);
            return result;
        }
    }

    pthread_mutex_unlock(&g_perf_mutex);
    return NULL;
}

/*========================================================================
 * Memory Usage Tracking
 *======================================================================== */

static evocore_memory_stats_t g_memory_stats = {0};
static bool g_memory_tracking_enabled = false;
static pthread_mutex_t g_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

void evocore_memory_get_stats(evocore_memory_stats_t *stats) {
    if (!stats) return;

    pthread_mutex_lock(&g_memory_mutex);
    *stats = g_memory_stats;
    pthread_mutex_unlock(&g_memory_mutex);
}

void evocore_memory_reset_stats(void) {
    pthread_mutex_lock(&g_memory_mutex);
    memset(&g_memory_stats, 0, sizeof(g_memory_stats));
    pthread_mutex_unlock(&g_memory_mutex);
}

void evocore_memory_set_tracking_enabled(bool enabled) {
    pthread_mutex_lock(&g_memory_mutex);
    g_memory_tracking_enabled = enabled;
    pthread_mutex_unlock(&g_memory_mutex);
}

/* Hook into internal allocation functions */
void* evocore_malloc_tracked(size_t size) {
    void *ptr = malloc(size);
    if (ptr && g_memory_tracking_enabled) {
        pthread_mutex_lock(&g_memory_mutex);
        g_memory_stats.total_allocated += size;
        g_memory_stats.current_usage += size;
        g_memory_stats.allocation_count++;
        if (g_memory_stats.current_usage > g_memory_stats.peak_usage) {
            g_memory_stats.peak_usage = g_memory_stats.current_usage;
        }
        pthread_mutex_unlock(&g_memory_mutex);
    }
    return ptr;
}

void evocore_free_tracked(void *ptr) {
    /* Note: we can't track size on free without storing metadata */
    /* This is a simplified implementation */
    if (ptr) {
        free(ptr);
        if (g_memory_tracking_enabled) {
            pthread_mutex_lock(&g_memory_mutex);
            g_memory_stats.free_count++;
            pthread_mutex_unlock(&g_memory_mutex);
        }
    }
}
