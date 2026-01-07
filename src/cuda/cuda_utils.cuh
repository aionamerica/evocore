#ifndef EVOCORE_CUDA_UTILS_CUH
#define EVOCORE_CUDA_UTILS_CUH

#include <cuda_runtime.h>
#include <stddef.h>

/*========================================================================
 * CUDA Error Handling
 *========================================================================*/

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = (call); \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA Error at %s:%d: %s\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err)); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

#define CUDA_CHECK_LAST() \
    do { \
        cudaError_t err = cudaGetLastError(); \
        if (err != cudaSuccess) { \
            fprintf(stderr, "CUDA Error at %s:%d: %s\n", \
                    __FILE__, __LINE__, cudaGetErrorString(err)); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

/*========================================================================
 * Device Information
 *======================================================================== */

/**
 * Get optimal block size for kernel launch
 */
inline int get_optimal_block_size(int max_threads_per_block) {
    /* Typically 256 is a good balance */
    return (max_threads_per_block >= 256) ? 256 : max_threads_per_block;
}

/**
 * Calculate grid size based on data count and block size
 */
inline int calculate_grid_size(int count, int block_size) {
    return (count + block_size - 1) / block_size;
}

/**
 * Get number of SMs for device
 */
inline int get_sm_count() {
    int device;
    CUDA_CHECK(cudaGetDevice(&device));

    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, device));

    return prop.multiProcessorCount;
}

/**
 * Get max concurrent kernels for device
 */
inline int get_max_concurrent_kernels() {
    int device;
    CUDA_CHECK(cudaGetDevice(&device));

    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, device));

    return prop.concurrentKernels ? 1 : 0;
}

/*========================================================================
 * Memory Utilities
 *======================================================================== */

/**
 * Allocate pinned memory for faster host-device transfers
 */
inline void* allocate_pinned_memory(size_t size) {
    void* ptr = nullptr;
    CUDA_CHECK(cudaMallocHost(&ptr, size));
    return ptr;
}

/**
 * Free pinned memory
 */
inline void free_pinned_memory(void* ptr) {
    CUDA_CHECK(cudaFreeHost(ptr));
}

/*========================================================================
 * Kernel Launch Wrappers
 *======================================================================== */

/**
 * Stream handle for async operations
 */
class CudaStream {
public:
    cudaStream_t stream;

    CudaStream() {
        CUDA_CHECK(cudaStreamCreate(&stream));
    }

    ~CudaStream() {
        CUDA_CHECK(cudaStreamDestroy(stream));
    }

    void sync() {
        CUDA_CHECK(cudaStreamSynchronize(stream));
    }

    operator cudaStream_t() { return stream; }
};

/**
 * Event for timing
 */
class CudaEvent {
public:
    cudaEvent_t event;

    CudaEvent() {
        CUDA_CHECK(cudaEventCreate(&event));
    }

    ~CudaEvent() {
        CUDA_CHECK(cudaEventDestroy(event));
    }

    void record(cudaStream_t stream = 0) {
        CUDA_CHECK(cudaEventRecord(event, stream));
    }

    float elapsed_since(const CudaEvent& start) {
        float ms;
        CUDA_CHECK(cudaEventElapsedTime(&ms, start.event, event));
        return ms;
    }
};

#endif /* EVOCORE_CUDA_UTILS_CUH */
