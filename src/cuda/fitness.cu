/**
 * CUDA Fitness Evaluation Kernels
 *
 * Provides GPU-accelerated fitness evaluation for batch processing.
 * The fitness function is provided as a device function pointer or
 * through template specialization.
 */

#include "cuda_utils.cuh"
#include <cstdint>

/*========================================================================
 * Forward Declarations
 *======================================================================== */

/* Fitness function type for device */
typedef double (*device_fitness_func_t)(const uint8_t* genome, size_t size);

/*========================================================================
 * Device Functions
 *======================================================================== */

/**
 * Simple genome distance metric (Hamming distance)
 */
__device__ double genome_distance_device(const uint8_t* a, const uint8_t* b, size_t size) {
    int distance = 0;
    for (size_t i = 0; i < size; i++) {
        distance += __popc(a[i] ^ b[i]);
    }
    return (double)distance;
}

/**
 * Byte-wise mutation simulation (for testing)
 */
__device__ void mutate_genome_device(uint8_t* genome, size_t size, float rate, unsigned int seed) {
    curandStatePhilox4_32_10_t state;
    curand_init(seed, 0, 0, &state);

    for (size_t i = 0; i < size; i++) {
        if (curand_uniform(&state) < rate) {
            genome[i] = (uint8_t)curand(&state);
        }
    }
}

/*========================================================================
 * Simple Fitness Function (for testing/demonstration)
 *
 * This is a placeholder - real fitness functions should be provided
 * by the domain through specialization or callback.
 *======================================================================== */

/**
 * Sphere function: f(x) = sum(x_i^2)
 * Genome is interpreted as array of doubles
 */
__device__ double sphere_fitness_device(const uint8_t* genome, size_t genome_size) {
    const double* values = (const double*)genome;
    size_t num_values = genome_size / sizeof(double);

    double sum = 0.0;
    for (size_t i = 0; i < num_values; i++) {
        double v = values[i];
        sum += v * v;
    }
    return -sum;  /* Negative for maximization */
}

/**
 * Rastrigin function: f(x) = 10*n + sum(x_i^2 - 10*cos(2*pi*x_i))
 */
__device__ double rastrigin_fitness_device(const uint8_t* genome, size_t genome_size) {
    const double* values = (const double*)genome;
    size_t num_values = genome_size / sizeof(double);

    const double PI = 3.14159265358979323846;
    double sum = 10.0 * (double)num_values;

    for (size_t i = 0; i < num_values; i++) {
        double v = values[i];
        sum += v * v - 10.0 * cos(2.0 * PI * v);
    }
    return -sum;
}

/**
 * Generic fitness function selector
 */
enum FitnessFunctionType {
    FITNESS_SPHERE = 0,
    FITNESS_RASTRIGIN = 1,
    FITNESS_CUSTOM = 2
};

__device__ double evaluate_fitness_device(
    const uint8_t* genome,
    size_t genome_size,
    FitnessFunctionType func_type)
{
    switch (func_type) {
        case FITNESS_SPHERE:
            return sphere_fitness_device(genome, genome_size);
        case FITNESS_RASTRIGIN:
            return rastrigin_fitness_device(genome, genome_size);
        default:
            return sphere_fitness_device(genome, genome_size);
    }
}

/*========================================================================
 * Batch Evaluation Kernel
 *======================================================================== */

/**
 * Batch fitness evaluation kernel
 *
 * Each thread evaluates one genome
 */
__global__ void batch_evaluate_kernel(
    const uint8_t* genomes,      /* Flattened genome data [count * genome_size] */
    double* fitnesses,           /* Output fitness array [count] */
    size_t genome_size,          /* Size of each genome in bytes */
    int count,                   /* Number of genomes */
    FitnessFunctionType func_type)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < count) {
        const uint8_t* genome = genomes + idx * genome_size;
        fitnesses[idx] = evaluate_fitness_device(genome, genome_size, func_type);
    }
}

/**
 * Batch evaluation kernel with custom function pointer array
 *
 * For domain-specific fitness functions
 */
__global__ void batch_evaluate_custom_kernel(
    const uint8_t* genomes,
    double* fitnesses,
    size_t genome_size,
    int count,
    device_fitness_func_t func)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < count && func != nullptr) {
        const uint8_t* genome = genomes + idx * genome_size;
        fitnesses[idx] = func(genome, genome_size);
    }
}

/*========================================================================
 * Population Statistics Kernel
 *======================================================================== */

/**
 * Find best fitness in population
 */
__global__ void find_best_fitness_kernel(
    const double* fitnesses,
    int count,
    double* best_fitness,
    int* best_index)
{
    extern __shared__ double s_fitnesses[];
    extern __shared__ int s_indices[];

    int idx = threadIdx.x;
    int gid = blockIdx.x * blockDim.x + idx;

    /* Load data */
    if (gid < count) {
        s_fitnesses[idx] = fitnesses[gid];
        s_indices[idx] = gid;
    } else {
        s_fitnesses[idx] = -1e308;  /* Minimum double */
        s_indices[idx] = -1;
    }
    __syncthreads();

    /* Reduction */
    for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
        if (idx < stride) {
            if (s_fitnesses[idx + stride] > s_fitnesses[idx]) {
                s_fitnesses[idx] = s_fitnesses[idx + stride];
                s_indices[idx] = s_indices[idx + stride];
            }
        }
        __syncthreads();
    }

    /* Write result */
    if (idx == 0) {
        best_fitness[blockIdx.x] = s_fitnesses[0];
        best_index[blockIdx.x] = s_indices[0];
    }
}

/**
 * Compute average fitness
 */
__global__ void average_fitness_kernel(
    const double* fitnesses,
    int count,
    double* result,
    int* counter)
{
    extern __shared__ double s_data[];

    int idx = threadIdx.x;
    int gid = blockIdx.x * blockDim.x + idx;

    /* Load data */
    s_data[idx] = (gid < count) ? fitnesses[gid] : 0.0;
    __syncthreads();

    /* Reduction */
    for (int stride = blockDim.x / 2; stride > 0; stride /= 2) {
        if (idx < stride) {
            s_data[idx] += s_data[idx + stride];
        }
        __syncthreads();
    }

    /* Write block result */
    if (idx == 0) {
        result[blockIdx.x] = s_data[0];
        counter[blockIdx.x] = (blockIdx.x * blockDim.x < count) ?
            min(blockDim.x, count - blockIdx.x * blockDim.x) : blockDim.x;
    }
}

/*========================================================================
 * Host Interface Functions
 *======================================================================== */

extern "C" {

/**
 * Launch batch fitness evaluation
 *
 * Returns the number of genomes evaluated, or 0 on error
 */
int cuda_batch_evaluate(
    const void* d_genomes,       /* Device pointer to genome data */
    void* d_fitnesses,           /* Device pointer to fitness array */
    size_t genome_size,          /* Size of each genome */
    int count,                   /* Number of genomes */
    int fitness_type,            /* FitnessFunctionType value */
    void* stream)                /* CUDA stream (NULL for default) */
{
    if (d_genomes == nullptr || d_fitnesses == nullptr) {
        return 0;
    }
    if (count <= 0 || genome_size == 0) {
        return 0;
    }

    const int block_size = 256;
    const int grid_size = (count + block_size - 1) / block_size;

    cudaStream_t cuda_stream = (cudaStream_t)stream;

    batch_evaluate_kernel<<<grid_size, block_size, 0, cuda_stream>>>(
        (const uint8_t*)d_genomes,
        (double*)d_fitnesses,
        genome_size,
        count,
        (FitnessFunctionType)fitness_type
    );

    /* Check for launch errors */
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        return 0;
    }

    return count;
}

/**
 * Synchronous batch evaluation (waits for completion)
 */
int cuda_batch_evaluate_sync(
    const void* d_genomes,
    void* d_fitnesses,
    size_t genome_size,
    int count,
    int fitness_type)
{
    int result = cuda_batch_evaluate(d_genomes, d_fitnesses,
                                     genome_size, count, fitness_type, nullptr);
    if (result > 0) {
        cudaError_t err = cudaDeviceSynchronize();
        if (err != cudaSuccess) {
            return 0;
        }
    }
    return result;
}

/**
 * Copy genomes to device (flattened)
 */
void* cuda_copy_genomes_to_device(
    const void* h_genomes,
    size_t genome_size,
    int count)
{
    uint8_t* d_genomes = nullptr;
    size_t total_size = genome_size * count;

    cudaError_t err = cudaMalloc(&d_genomes, total_size);
    if (err != cudaSuccess) {
        return nullptr;
    }

    err = cudaMemcpy(d_genomes, h_genomes, total_size, cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(d_genomes);
        return nullptr;
    }

    return d_genomes;
}

/**
 * Copy fitnesses from device
 */
int cuda_copy_fitnesses_from_device(
    void* h_fitnesses,
    const void* d_fitnesses,
    int count)
{
    size_t size = sizeof(double) * count;
    cudaError_t err = cudaMemcpy(h_fitnesses, d_fitnesses, size,
                                 cudaMemcpyDeviceToHost);
    return (err == cudaSuccess) ? 1 : 0;
}

/**
 * Free device memory
 */
void cuda_free_device_memory(void* d_ptr) {
    if (d_ptr != nullptr) {
        cudaFree(d_ptr);
    }
}

/**
 * Get last CUDA error string
 */
const char* cuda_get_error_string() {
    cudaError_t err = cudaGetLastError();
    static char error_buffer[256];
    snprintf(error_buffer, sizeof(error_buffer),
             "CUDA Error: %s", cudaGetErrorString(err));
    return error_buffer;
}

/**
 * Compute population statistics on GPU
 */
int cuda_population_stats(
    const void* d_fitnesses,
    int count,
    double* h_best_fitness,
    int* h_best_index,
    double* h_avg_fitness)
{
    if (d_fitnesses == nullptr || count <= 0) {
        return 0;
    }

    const int block_size = 256;
    const int grid_size = min((count + block_size - 1) / block_size, 1024);

    /* Temporary arrays for reduction */
    double* d_best_fitness = nullptr;
    int* d_best_index = nullptr;
    double* d_avg_sums = nullptr;
    int* d_avg_counts = nullptr;

    cudaMalloc(&d_best_fitness, grid_size * sizeof(double));
    cudaMalloc(&d_best_index, grid_size * sizeof(int));
    cudaMalloc(&d_avg_sums, grid_size * sizeof(double));
    cudaMalloc(&d_avg_counts, grid_size * sizeof(int));

    /* Find best */
    find_best_fitness_kernel<<<grid_size, block_size,
        block_size * (sizeof(double) + sizeof(int))>>>(
        (const double*)d_fitnesses, count, d_best_fitness, d_best_index);

    /* Compute average */
    average_fitness_kernel<<<grid_size, block_size,
        block_size * sizeof(double)>>>(
        (const double*)d_fitnesses, count, d_avg_sums, d_avg_counts);

    /* Copy results */
    double* h_best_fitness_buf = new double[grid_size];
    int* h_best_index_buf = new int[grid_size];
    double* h_avg_sums = new double[grid_size];
    int* h_avg_counts = new int[grid_size];

    cudaMemcpy(h_best_fitness_buf, d_best_fitness, grid_size * sizeof(double),
               cudaMemcpyDeviceToHost);
    cudaMemcpy(h_best_index_buf, d_best_index, grid_size * sizeof(int),
               cudaMemcpyDeviceToHost);
    cudaMemcpy(h_avg_sums, d_avg_sums, grid_size * sizeof(double),
               cudaMemcpyDeviceToHost);
    cudaMemcpy(h_avg_counts, d_avg_counts, grid_size * sizeof(int),
               cudaMemcpyDeviceToHost);

    /* Final reduction on host */
    double best = h_best_fitness_buf[0];
    int best_idx = h_best_index_buf[0];
    double sum = 0.0;
    int total = 0;

    for (int i = 0; i < grid_size; i++) {
        if (h_best_fitness_buf[i] > best) {
            best = h_best_fitness_buf[i];
            best_idx = h_best_index_buf[i];
        }
        sum += h_avg_sums[i];
        total += h_avg_counts[i];
    }

    if (h_best_fitness) *h_best_fitness = best;
    if (h_best_index) *h_best_index = best_idx;
    if (h_avg_fitness) *h_avg_fitness = (total > 0) ? (sum / total) : 0.0;

    /* Cleanup */
    delete[] h_best_fitness_buf;
    delete[] h_best_index_buf;
    delete[] h_avg_sums;
    delete[] h_avg_counts;
    cudaFree(d_best_fitness);
    cudaFree(d_best_index);
    cudaFree(d_avg_sums);
    cudaFree(d_avg_counts);

    return 1;
}

} /* extern "C" */
