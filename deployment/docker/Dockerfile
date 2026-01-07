# Multi-stage Dockerfile for evocore
# Stage 1: Build stage
FROM ubuntu:22.04 AS builder

# Prevent interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    cuda-toolkit-12-x \
    libcuda1-1 \
    nvidia-cuda-toolkit \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set CUDA environment
ENV CUDA_HOME=/usr/local/cuda
ENV PATH=${CUDA_HOME}/bin:${PATH}
ENV LD_LIBRARY_PATH=${CUDA_HOME}/lib64:${LD_LIBRARY_PATH}

WORKDIR /usr/src/evocore

# Copy source code
COPY . .

# Build library
RUN make clean && make

# Stage 2: Runtime stage
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libcuda1-1 \
    nvidia-cuda-toolkit \
    && rm -rf /var/lib/apt/lists/*

ENV CUDA_HOME=/usr/local/cuda
ENV PATH=${CUDA_HOME}/bin:${PATH}
ENV LD_LIBRARY_PATH=${CUDA_HOME}/lib64:${LD_LIBRARY_PATH}

# Create directories
RUN mkdir -p /usr/local/lib \
            /usr/local/include/evocore \
            /var/log/evocore \
            /var/lib/evocore/checkpoints \
            /app

WORKDIR /app

# Copy built library from builder stage
COPY --from=builder /usr/src/evocore/build/libevocore.a /usr/local/lib/
COPY --from=builder /usr/src/evocore/include/evocore/ /usr/local/include/evocore/

# Copy examples
COPY --from=builder /usr/src/evocore/build/sphere_function /app/
COPY --from=builder /usr/src/evocore/build/meta_demo /app/
COPY --from=builder /usr/src/evocore/build/gpu_benchmark /app/
COPY --from=builder /usr/src/evocore/build/checkpoint_demo /app/
COPY --from=builder /usr/src/evocore/build/monitoring_demo /app/

# Copy example configs
COPY --from=builder /usr/src/evocore/examples/*.ini /app/

# Default command
CMD ["./sphere_function"]
