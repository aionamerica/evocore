# evocore Deployment Guide

This guide covers deploying evocore in production environments.

## Table of Contents

1. [Installation](#installation)
2. [Building from Source](#building-from-source)
3. [Docker Deployment](#docker-deployment)
4. [Configuration](#configuration)
5. [Monitoring](#monitoring)
6. [Performance Tuning](#performance-tuning)

## Installation

### Using Installation Script

```bash
# Clone repository
git clone https://github.com/yourorg/evocore.git
cd evocore

# Run installation script
sudo deployment/scripts/install.sh
```

### Manual Installation

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install build-essential libcuda1 nvidia-cuda-toolkit

# Build
make

# Install
sudo make install
```

### Verification

```bash
# Run health check
deployment/scripts/health_check.sh
```

## Building from Source

### Prerequisites

- **C99 Compiler**: GCC 4.8+ or Clang 3.4+
- **CUDA Toolkit**: 11.0+ (optional, for GPU support)
- **pthread**: Included with glibc
- **make**: GNU Make 3.81+

### Build Options

```bash
# Standard build
make

# With CUDA support
make CUDA=yes

# With OpenMP support
make OMP=yes

# Debug build
make debug

# Release build with optimizations
make CFLAGS="-O3 -march=native"
```

## Docker Deployment

### Building Docker Image

```bash
docker build -f deployment/docker/Dockerfile -t evocore:0.4.0 .
```

### Running with Docker Compose

```bash
cd deployment/docker

# With GPU support
docker-compose up evocore

# CPU only
docker-compose up evocore-cpu
```

### Docker Volume Mounts

- `./checkpoints` → `/var/lib/evocore/checkpoints`
- `./logs` → `/var/log/evocore`

## Configuration

### Configuration Files

Place configuration files in `/etc/evocore/` or specify via command line:

```bash
evocore_app /etc/evocore/production.conf
```

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `EVOCORE_LOG_LEVEL` | Log level (ERROR/WARN/INFO/DEBUG) | INFO |
| `EVOCORE_CUDA_DEVICE` | GPU device ID | 0 |
| `EVOCORE_CHECKPOINT_DIR` | Checkpoint directory | ./checkpoints |
| `EVOCORE_NUM_THREADS` | Thread count (0 = auto) | 0 |

### Resource Limits

Recommended for production:

```ini
[performance]
arena_capacity = 10485760      # 10 MB arena
max_population = 10000
batch_size = 1000
```

## Monitoring

### Health Checks

```bash
# Basic health check
deployment/scripts/health_check.sh

# Run with monitoring
./build/monitoring_demo
```

### Metrics Export

```c
#include "evocore/memory.h"

// Dump memory statistics
evocore_memory_dump_stats();

// Check for leaks
size_t leaks = evocore_memory_check_leaks();
```

### Log Monitoring

Logs are written to `/var/log/evocore/evocore.log` by default.

Key log patterns:
- `WARN.*convergence` - Population converged early
- `WARN.*stagnation` - Fitness not improving
- `ERROR.*cuda` - GPU errors

## Performance Tuning

### Memory Optimization

1. **Use Arena Allocators**
   ```c
   evocore_arena_t arena;
   evocore_arena_init(&arena, 1024 * 1024);
   // ... allocate genomes for generation ...
   evocore_arena_reset(&arena);  // Reset instead of individual frees
   ```

2. **Batch Evaluation**
   ```c
   // Evaluate 1000 genomes at once
   evocore_gpu_evaluate_batch(ctx, &batch, fitness, NULL, &result);
   ```

3. **Thread Pool**
   ```c
   // Auto-detects CPU cores
   evocore_cpu_evaluate_batch(&batch, fitness, NULL, 0, &result);
   ```

### GPU Optimization

```ini
[gpu]
enabled = true
batch_size = 1000           # Tune based on GPU memory
device = 0                  # GPU device ID
pinned_memory = true        # Faster transfers
```

### Benchmarking

```bash
# Run performance benchmarks
make benchmark

# GPU benchmark specifically
./build/gpu_benchmark
```

Expected results on modern hardware:
- **Serial CPU**: ~2M evals/sec
- **Parallel CPU**: ~12M evals/sec
- **GPU**: ~80M evals/sec

## Troubleshooting

### CUDA Not Found

```bash
# Check CUDA installation
nvcc --version

# Set CUDA_ROOT manually
export CUDA_ROOT=/usr/local/cuda
make CUDA=yes
```

### Memory Leaks

```bash
# Run with valgrind
make valgrind

# Check leaks programmatically
evocore_memory_set_tracking(true);
// ... run code ...
evocore_memory_check_leaks();
```

### Slow Performance

1. Check GPU is being used: `EVOCORE_LOG_LEVEL=DEBUG ./app`
2. Increase batch size in config
3. Enable arena allocation
4. Reduce logging overhead: `EVOCORE_LOG_LEVEL=WARN`

## Production Checklist

- [ ] Built with optimizations (`-O3 -march=native`)
- [ ] CUDA enabled and tested
- [ ] Checkpointing configured
- [ ] Log rotation configured
- [ ] Monitoring enabled
- [ ] Health checks passing
- [ ] Resource limits set
- [ ] Backup strategy in place
