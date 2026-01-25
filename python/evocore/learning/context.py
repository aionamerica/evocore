"""
Context-aware learning system for evocore Python bindings.

Provides multi-dimensional context-based parameter learning.
"""

from typing import List, Tuple, Optional, Dict, Any
import numpy as np
from ..utils.error import check_error, check_bool, EvocoreError


class ContextStats:
    """
    Statistics for a specific context.

    Wraps evocore_context_stats_t.
    """

    __slots__ = ('_stats', '_ffi', '_lib', '_system')

    def __init__(self, stats_ptr, ffi, lib, system: "ContextSystem"):
        """
        Initialize context stats wrapper.

        Args:
            stats_ptr: Pointer to evocore_context_stats_t
            ffi: cffi FFI instance
            lib: cffi lib instance
            system: Parent context system
        """
        self._stats = stats_ptr
        self._ffi = ffi
        self._lib = lib
        self._system = system

    @property
    def key(self) -> str:
        """Context key string."""
        if self._stats.key == self._ffi.NULL:
            return ""
        return self._ffi.string(self._stats.key).decode()

    @property
    def param_count(self) -> int:
        """Number of parameters tracked."""
        return self._stats.param_count

    @property
    def confidence(self) -> float:
        """Confidence level (0.0 to 1.0)."""
        return self._stats.confidence

    @property
    def total_experiences(self) -> int:
        """Total number of learning experiences."""
        return self._stats.total_experiences

    @property
    def avg_fitness(self) -> float:
        """Average fitness across experiences."""
        return self._stats.avg_fitness

    @property
    def best_fitness(self) -> float:
        """Best fitness observed."""
        return self._stats.best_fitness

    @property
    def failure_count(self) -> int:
        """Number of recorded failures."""
        return self._stats.failure_count

    def has_data(self, min_samples: int = 1) -> bool:
        """
        Check if context has sufficient data.

        Args:
            min_samples: Minimum number of samples required

        Returns:
            True if sufficient data exists
        """
        return self._lib.evocore_context_has_data(self._stats, min_samples)

    def __repr__(self) -> str:
        return (f"ContextStats(key='{self.key}', confidence={self.confidence:.3f}, "
                f"experiences={self.total_experiences})")


class ContextSystem:
    """
    Multi-dimensional context-aware learning system.

    Tracks parameter statistics across different contexts defined by
    dimensions (e.g., asset, timeframe, market condition).

    Example:
        >>> dimensions = [
        ...     ("asset", ["BTC", "ETH", "SOL"]),
        ...     ("timeframe", ["1h", "4h", "1d"]),
        ... ]
        >>> ctx = ContextSystem(dimensions, param_count=5)
        >>> ctx.learn(["BTC", "1h"], [0.1, 0.2, 0.3, 0.4, 0.5], fitness=0.75)
        >>> params = ctx.sample(["BTC", "1h"], exploration=0.3)
    """

    __slots__ = ('_system', '_ffi', '_lib', '_dimensions', '_param_count', '_owns_system', '_dim_names', '_dim_values')

    def __init__(self, dimensions: List[Tuple[str, List[str]]], param_count: int,
                 *, _raw: bool = False):
        """
        Initialize a context system.

        Args:
            dimensions: List of (name, values) tuples defining context dimensions
            param_count: Number of parameters to track per context
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._system = None
            self._ffi = None
            self._lib = None
            self._dimensions = []
            self._param_count = 0
            self._owns_system = False
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib
        self._dimensions = dimensions
        self._param_count = param_count
        self._owns_system = True

        # Build C dimension array with exception safety
        dim_array = ffi.new("evocore_context_dimension_t[]", len(dimensions))

        # Keep references to prevent GC
        self._dim_names = []
        self._dim_values = []
        self._system = ffi.NULL  # Initialize to NULL for safe cleanup

        try:
            for i, (name, values) in enumerate(dimensions):
                # Allocate name
                name_buf = ffi.new("char[]", name.encode())
                self._dim_names.append(name_buf)
                dim_array[i].name = name_buf

                # Allocate values array
                values_array = ffi.new("char*[]", len(values))
                value_bufs = []
                for j, v in enumerate(values):
                    v_buf = ffi.new("char[]", v.encode())
                    value_bufs.append(v_buf)
                    values_array[j] = v_buf
                self._dim_values.append((values_array, value_bufs))

                dim_array[i].value_count = len(values)
                dim_array[i].values = values_array

            self._system = lib.evocore_context_system_create(dim_array, len(dimensions), param_count)

            if self._system == ffi.NULL:
                raise EvocoreError("Failed to create context system")
        except Exception:
            # Cleanup on failure - cffi handles memory via GC, but clear refs
            self._dim_names = []
            self._dim_values = []
            self._system = ffi.NULL
            raise

    def __del__(self):
        """Clean up context system."""
        if hasattr(self, '_system') and self._system is not None and self._owns_system:
            self._lib.evocore_context_system_free(self._system)

    @property
    def param_count(self) -> int:
        """Number of parameters tracked."""
        return self._param_count

    @property
    def dimension_count(self) -> int:
        """Number of context dimensions."""
        return len(self._dimensions)

    @property
    def dimensions(self) -> List[Tuple[str, List[str]]]:
        """Get dimension definitions."""
        return self._dimensions.copy()

    @property
    def context_count(self) -> int:
        """Number of contexts with data."""
        return self._lib.evocore_context_count(self._system)

    def learn(self, context: List[str], parameters: np.ndarray, fitness: float) -> bool:
        """
        Learn from an experience in the given context.

        Args:
            context: Values for each dimension (in order)
            parameters: Parameter array
            fitness: Fitness value (weights the update)

        Returns:
            True if learning succeeded
        """
        if len(context) != len(self._dimensions):
            raise ValueError(f"Expected {len(self._dimensions)} context values, got {len(context)}")

        # Ensure numpy array
        params = np.asarray(parameters, dtype=np.float64)
        if len(params) != self._param_count:
            raise ValueError(f"Expected {self._param_count} parameters, got {len(params)}")

        # Build context values array
        context_arr = self._ffi.new("char*[]", len(context))
        context_bufs = []
        for i, c in enumerate(context):
            buf = self._ffi.new("char[]", c.encode())
            context_bufs.append(buf)
            context_arr[i] = buf

        # Build parameters array
        params_arr = self._ffi.new("double[]", params.tolist())

        result = self._lib.evocore_context_learn(
            self._system, context_arr, params_arr, len(params), fitness
        )

        return result

    def sample(self, context: List[str], exploration: float = 0.5,
               seed: Optional[int] = None) -> np.ndarray:
        """
        Sample parameters for the given context.

        Args:
            context: Values for each dimension
            exploration: Exploration factor (0=exploit, 1=explore)
            seed: Optional random seed

        Returns:
            Sampled parameters as numpy array
        """
        if len(context) != len(self._dimensions):
            raise ValueError(f"Expected {len(self._dimensions)} context values")

        # Build context array
        context_arr = self._ffi.new("char*[]", len(context))
        context_bufs = []
        for i, c in enumerate(context):
            buf = self._ffi.new("char[]", c.encode())
            context_bufs.append(buf)
            context_arr[i] = buf

        # Output array
        out_params = self._ffi.new("double[]", self._param_count)
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)

        success = self._lib.evocore_context_sample(
            self._system, context_arr, out_params, self._param_count, exploration, seed_ptr
        )

        if not success:
            # Return random parameters if no data
            return np.random.uniform(0, 1, self._param_count)

        return np.array([out_params[i] for i in range(self._param_count)])

    def get_stats(self, context: List[str]) -> Optional[ContextStats]:
        """
        Get statistics for a specific context.

        Args:
            context: Values for each dimension

        Returns:
            ContextStats or None if no data
        """
        if len(context) != len(self._dimensions):
            raise ValueError(f"Expected {len(self._dimensions)} context values")

        # Build context array
        context_arr = self._ffi.new("char*[]", len(context))
        context_bufs = []
        for i, c in enumerate(context):
            buf = self._ffi.new("char[]", c.encode())
            context_bufs.append(buf)
            context_arr[i] = buf

        stats_ptr = self._ffi.new("evocore_context_stats_t **")
        success = self._lib.evocore_context_get_stats(self._system, context_arr, stats_ptr)

        if not success or stats_ptr[0] == self._ffi.NULL:
            return None

        return ContextStats(stats_ptr[0], self._ffi, self._lib, self)

    def build_key(self, context: List[str]) -> str:
        """
        Build a context key string.

        Args:
            context: Values for each dimension

        Returns:
            Context key string
        """
        if len(context) != len(self._dimensions):
            raise ValueError(f"Expected {len(self._dimensions)} context values")

        # Build context array
        context_arr = self._ffi.new("char*[]", len(context))
        context_bufs = []
        for i, c in enumerate(context):
            buf = self._ffi.new("char[]", c.encode())
            context_bufs.append(buf)
            context_arr[i] = buf

        out_key = self._ffi.new("char[1024]")  # Increased from 256 for longer context keys
        success = self._lib.evocore_context_build_key(self._system, context_arr, out_key, 1024)

        if not success:
            # Fallback to Python implementation
            return ":".join(context)

        return self._ffi.string(out_key).decode()

    def validate_context(self, context: List[str]) -> bool:
        """
        Validate that context values are valid for this system.

        Args:
            context: Values for each dimension

        Returns:
            True if valid
        """
        if len(context) != len(self._dimensions):
            return False

        for i, (name, valid_values) in enumerate(self._dimensions):
            if context[i] not in valid_values:
                return False

        return True

    def reset(self, context: List[str]) -> bool:
        """
        Reset statistics for a specific context.

        Args:
            context: Values for each dimension

        Returns:
            True if successful
        """
        context_arr = self._ffi.new("char*[]", len(context))
        context_bufs = []
        for i, c in enumerate(context):
            buf = self._ffi.new("char[]", c.encode())
            context_bufs.append(buf)
            context_arr[i] = buf

        return self._lib.evocore_context_reset(self._system, context_arr)

    def reset_all(self) -> None:
        """Reset all context statistics."""
        self._lib.evocore_context_reset_all(self._system)

    def save_json(self, filepath: str) -> bool:
        """
        Save context system to JSON file.

        Args:
            filepath: Path to save to

        Returns:
            True if successful
        """
        return self._lib.evocore_context_save_json(self._system, filepath.encode())

    @classmethod
    def load_json(cls, filepath: str) -> "ContextSystem":
        """
        Load context system from JSON file.

        Args:
            filepath: Path to load from

        Returns:
            Loaded ContextSystem
        """
        from .._evocore import ffi, lib

        system_ptr = ffi.new("evocore_context_system_t **")
        success = lib.evocore_context_load_json(filepath.encode(), system_ptr)

        if not success or system_ptr[0] == ffi.NULL:
            raise EvocoreError(f"Failed to load context system from {filepath}")

        obj = cls.__new__(cls)
        obj._system = system_ptr[0]
        obj._ffi = ffi
        obj._lib = lib
        obj._owns_system = True

        # Extract dimensions from loaded system
        obj._dimensions = []
        obj._param_count = obj._system.param_count
        obj._dim_names = []
        obj._dim_values = []

        return obj

    def save_binary(self, filepath: str) -> bool:
        """
        Save context system to binary file (faster than JSON).

        Args:
            filepath: Path to save to

        Returns:
            True if successful
        """
        return self._lib.evocore_context_save_binary(self._system, filepath.encode())

    @classmethod
    def load_binary(cls, filepath: str) -> "ContextSystem":
        """
        Load context system from binary file.

        Args:
            filepath: Path to load from

        Returns:
            Loaded ContextSystem
        """
        from .._evocore import ffi, lib

        system_ptr = ffi.new("evocore_context_system_t **")
        success = lib.evocore_context_load_binary(filepath.encode(), system_ptr)

        if not success or system_ptr[0] == ffi.NULL:
            raise EvocoreError(f"Failed to load context system from {filepath}")

        obj = cls.__new__(cls)
        obj._system = system_ptr[0]
        obj._ffi = ffi
        obj._lib = lib
        obj._owns_system = True
        obj._dimensions = []
        obj._param_count = obj._system.param_count
        obj._dim_names = []
        obj._dim_values = []

        return obj

    def export_csv(self, filepath: str) -> bool:
        """
        Export context system data to CSV file.

        Args:
            filepath: Path to save to

        Returns:
            True if successful
        """
        return self._lib.evocore_context_export_csv(self._system, filepath.encode())

    def __repr__(self) -> str:
        dim_str = ", ".join(f"{d[0]}({len(d[1])})" for d in self._dimensions)
        return (f"ContextSystem(dimensions=[{dim_str}], "
                f"param_count={self._param_count}, contexts={self.context_count})")


__all__ = ['ContextStats', 'ContextSystem']
