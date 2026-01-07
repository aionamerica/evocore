# evocore - Meta-Evolutionary Framework Makefile
# Version 0.1.0

# Compiler settings
CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=gnu99 -O2
DEBUG_CFLAGS ?= -Wall -Wextra -std=gnu99 -g -O0 -DDEBUG
LDFLAGS ?=
INCLUDES := -Iinclude

# CUDA support - auto-detect or enable with CUDA=yes
CUDA_ROOT ?= /usr/local/cuda
HAS_CUDA := $(shell test -f $(CUDA_ROOT)/include/cuda_runtime.h && echo yes)

ifeq ($(CUDA),yes)
    HAS_CUDA := yes
endif

ifeq ($(HAS_CUDA),yes)
    NVCC ?= $(CUDA_ROOT)/bin/nvcc
    CUDA_CFLAGS := -DEVOCORE_HAVE_CUDA -I$(CUDA_ROOT)/include
    CUDA_LDFLAGS := -L$(CUDA_ROOT)/lib64 -lcudart
    CUDA_SOURCES := $(wildcard $(SRC_DIR)/cuda/*.cu)
    CUDA_OBJS := $(CUDA_SOURCES:$(SRC_DIR)/cuda/%.cu=$(BUILD_DIR)/cuda_%.o)
    OBJS += $(CUDA_OBJS)
    LDFLAGS += $(CUDA_LDFLAGS)
    CFLAGS += $(CUDA_CFLAGS)
    $(info "CUDA support: enabled")
else
    CUDA_CFLAGS :=
    CUDA_LDFLAGS :=
    CUDA_SOURCES :=
    CUDA_OBJS :=
    $(info "CUDA support: disabled (set CUDA=yes to enable)")
endif

# OpenMP support - enable with OMP=yes
ifeq ($(OMP),yes)
    CFLAGS += -DOMP_SUPPORT -fopenmp
    LDFLAGS += -fopenmp
    $(info "OpenMP support: enabled")
else
    $(info "OpenMP support: disabled (set OMP=yes to enable)")
endif

# Directories
SRC_DIR := src
INCLUDE_DIR := include/evocore
BUILD_DIR := build
EXAMPLE_DIR := examples
TEST_DIR := tests

# Source files
SRCS := $(SRC_DIR)/error.c \
        $(SRC_DIR)/log.c \
        $(SRC_DIR)/config.c \
        $(SRC_DIR)/genome.c \
        $(SRC_DIR)/population.c \
        $(SRC_DIR)/domain.c \
        $(SRC_DIR)/meta.c \
        $(SRC_DIR)/adaptive.c \
        $(SRC_DIR)/gpu.c \
        $(SRC_DIR)/optimize.c \
        $(SRC_DIR)/persist.c \
        $(SRC_DIR)/stats.c \
        $(SRC_DIR)/arena.c \
        $(SRC_DIR)/memory.c \
        $(SRC_DIR)/internal.c

# Object files
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Library
LIB := $(BUILD_DIR)/libevocore.a

# Examples
EXAMPLES := $(BUILD_DIR)/sphere_function \
            $(BUILD_DIR)/trading_domain \
            $(BUILD_DIR)/tsp_domain \
            $(BUILD_DIR)/meta_demo \
            $(BUILD_DIR)/gpu_benchmark \
            $(BUILD_DIR)/checkpoint_demo \
            $(BUILD_DIR)/monitoring_demo

# Tests
TESTS := $(BUILD_DIR)/test_genome \
         $(BUILD_DIR)/test_population \
         $(BUILD_DIR)/test_config \
         $(BUILD_DIR)/test_domain \
         $(BUILD_DIR)/test_meta \
         $(BUILD_DIR)/test_gpu \
         $(BUILD_DIR)/test_persist \
         $(BUILD_DIR)/test_stats \
         $(BUILD_DIR)/test_integration \
         $(BUILD_DIR)/test_performance_bench

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lm -lpthread -lrt
endif
ifeq ($(UNAME_S),Darwin)
    LDFLAGS += -lm -lpthread
endif

# Pthread support for CPU parallel evaluation (always enabled)
CFLAGS += -DEVOCORE_HAVE_PTHREADS

# Default target
.PHONY: all
all: $(LIB) examples

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Library object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# CUDA object files
$(BUILD_DIR)/cuda_%.o: $(SRC_DIR)/cuda/%.cu | $(BUILD_DIR)
	$(NVCC) -O3 -arch=native --compiler-options "$(CFLAGS) $(INCLUDES)" -c $< -o $@

# Build static library
$(LIB): $(OBJS)
	ar rcs $@ $^
	@echo "Built library: $@"

# Build examples
.PHONY: examples
examples: $(EXAMPLES)

$(BUILD_DIR)/sphere_function: $(EXAMPLE_DIR)/sphere_function.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

$(BUILD_DIR)/trading_domain: $(EXAMPLE_DIR)/trading_domain.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

$(BUILD_DIR)/tsp_domain: $(EXAMPLE_DIR)/tsp_domain.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

$(BUILD_DIR)/meta_demo: $(EXAMPLE_DIR)/meta_demo.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

$(BUILD_DIR)/gpu_benchmark: $(EXAMPLE_DIR)/gpu_benchmark.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@ -lrt

$(BUILD_DIR)/checkpoint_demo: $(EXAMPLE_DIR)/checkpoint_demo.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

$(BUILD_DIR)/monitoring_demo: $(EXAMPLE_DIR)/monitoring_demo.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

# Build tests
.PHONY: tests
tests: $(TESTS)

$(BUILD_DIR)/test_%: $(TEST_DIR)/test_%.c $(LIB)
	$(CC) $(CFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -levocore $(LDFLAGS) -o $@

# Debug build
.PHONY: debug
debug: clean
	$(eval CFLAGS=$(DEBUG_CFLAGS))
	$(MAKE) all

# Run tests
.PHONY: test
test: tests
	@echo "Running tests..."
	@for test in $(TESTS); do \
		if [ -f $$test ]; then \
			echo "Running $$test..."; \
			$$test; \
		fi \
	done

# Run example
.PHONY: run
run: examples
	@echo "Running sphere function example..."
	$(BUILD_DIR)/sphere_function $(EXAMPLE_DIR)/sphere_config.ini

# Benchmark target
.PHONY: benchmark
benchmark: tests
	@echo "Running performance benchmarks..."
	@$(BUILD_DIR)/test_performance_bench

# Install library
.PHONY: install
install: $(LIB)
	@echo "Installing evocore to /usr/local..."
	@mkdir -p /usr/local/include/evocore
	@cp $(INCLUDE_DIR)/*.h /usr/local/include/evocore/
	@cp $(LIB) /usr/local/lib/

# Uninstall library
.PHONY: uninstall
	@echo "Uninstalling evocore from /usr/local..."
	@rm -rf /usr/local/include/evocore
	@rm -f /usr/local/lib/libevocore.a

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(EXAMPLE_DIR)/*.log

# Clean everything
.PHONY: distclean
distclean: clean
	@rm -f $(EXAMPLE_DIR)/*.log

# Valgrind check
.PHONY: valgrind
valgrind: examples
	valgrind --leak-check=full --show-leak-kinds=all \
	         --track-origins=yes $(BUILD_DIR)/sphere_function \
	         $(EXAMPLE_DIR)/sphere_config.ini

# Help
.PHONY: help
help:
	@echo "evocore - Meta-Evolutionary Framework"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build library and examples (default)"
	@echo "  examples  - Build example programs"
	@echo "  tests     - Build test programs"
	@echo "  test      - Build and run tests"
	@echo "  run       - Build and run sphere function example"
	@echo "  debug     - Build with debug symbols"
	@echo "  valgrind  - Run example with valgrind"
	@echo "  clean     - Remove build artifacts"
	@echo "  install   - Install library to /usr/local"
	@echo "  uninstall - Remove library from /usr/local"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Build Options:"
	@echo "  CUDA=yes  - Enable CUDA support (requires CUDA toolkit)"
	@echo "  OMP=yes   - Enable OpenMP support"
	@echo ""
	@echo "Examples:"
	@echo "  make                 - Build without CUDA"
	@echo "  make CUDA=yes        - Build with CUDA support"
	@echo "  make OMP=yes         - Build with OpenMP"
	@echo "  make CUDA=yes OMP=yes - Build with both CUDA and OpenMP"

.PHONY: all debug clean distclean install uninstall test run valgrind help
