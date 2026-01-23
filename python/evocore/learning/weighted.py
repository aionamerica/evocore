"""
Weighted statistics for evocore Python bindings.

Provides weighted mean/variance tracking with sampling capabilities.
"""

from typing import Optional, List
import numpy as np
from ..utils.error import check_error, check_bool, EvocoreError


class WeightedStats:
    """
    Weighted statistics tracker for a single value.

    Uses Welford's algorithm for numerically stable weighted statistics.

    Example:
        >>> stats = WeightedStats()
        >>> stats.update(0.5, weight=1.0)
        >>> stats.update(0.7, weight=2.0)
        >>> print(f"Mean: {stats.mean}, Std: {stats.std}")
    """

    __slots__ = ('_stats', '_ffi', '_lib', '_owns_memory')

    def __init__(self, *, _raw: bool = False):
        """
        Initialize weighted statistics tracker.

        Args:
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._stats = None
            self._ffi = None
            self._lib = None
            self._owns_memory = False
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib
        self._owns_memory = True

        self._stats = ffi.new("evocore_weighted_stats_t *")
        lib.evocore_weighted_init(self._stats)

    def __del__(self):
        """Clean up (no-op since stack allocated)."""
        pass

    def update(self, value: float, weight: float = 1.0) -> bool:
        """
        Update statistics with a new value.

        Args:
            value: Value to add
            weight: Weight of this value (higher = more important)

        Returns:
            True if successful
        """
        return self._lib.evocore_weighted_update(self._stats, value, weight)

    @property
    def mean(self) -> float:
        """Weighted mean."""
        return self._lib.evocore_weighted_mean(self._stats)

    @property
    def std(self) -> float:
        """Weighted standard deviation."""
        return self._lib.evocore_weighted_std(self._stats)

    @property
    def variance(self) -> float:
        """Weighted variance."""
        return self._lib.evocore_weighted_variance(self._stats)

    @property
    def count(self) -> int:
        """Number of samples."""
        return self._stats.count

    @property
    def min_value(self) -> float:
        """Minimum observed value."""
        return self._stats.min_value

    @property
    def max_value(self) -> float:
        """Maximum observed value."""
        return self._stats.max_value

    @property
    def sum_weights(self) -> float:
        """Sum of all weights."""
        return self._stats.sum_weights

    def sample(self, seed: Optional[int] = None) -> float:
        """
        Sample a value from the distribution.

        Returns a value sampled from N(mean, std).

        Args:
            seed: Optional random seed

        Returns:
            Sampled value
        """
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)
        return self._lib.evocore_weighted_sample(self._stats, seed_ptr)

    def has_data(self, min_samples: int = 1) -> bool:
        """
        Check if sufficient data exists.

        Args:
            min_samples: Minimum number of samples required

        Returns:
            True if sufficient data
        """
        return self._lib.evocore_weighted_has_data(self._stats, min_samples)

    def confidence(self, max_samples: int = 100) -> float:
        """
        Get confidence level based on sample count.

        Args:
            max_samples: Number of samples for full confidence

        Returns:
            Confidence level (0.0 to 1.0)
        """
        return self._lib.evocore_weighted_confidence(self._stats, max_samples)

    def reset(self) -> None:
        """Reset all statistics."""
        self._lib.evocore_weighted_reset(self._stats)

    def clone(self) -> "WeightedStats":
        """
        Create a copy of these statistics.

        Returns:
            New WeightedStats with copied data
        """
        new = WeightedStats()
        self._lib.evocore_weighted_clone(self._stats, new._stats)
        return new

    def merge(self, other: "WeightedStats") -> bool:
        """
        Merge another WeightedStats into this one.

        Args:
            other: Statistics to merge

        Returns:
            True if successful
        """
        return self._lib.evocore_weighted_merge(self._stats, other._stats)

    def to_json(self) -> str:
        """
        Serialize to JSON string.

        Returns:
            JSON representation
        """
        buffer = self._ffi.new("char[1024]")
        size = self._lib.evocore_weighted_to_json(self._stats, buffer, 1024)
        if size == 0:
            return "{}"
        return self._ffi.string(buffer).decode()

    @classmethod
    def from_json(cls, json_str: str) -> "WeightedStats":
        """
        Deserialize from JSON string.

        Args:
            json_str: JSON representation

        Returns:
            New WeightedStats
        """
        stats = cls()
        success = stats._lib.evocore_weighted_from_json(
            json_str.encode(), stats._stats
        )
        if not success:
            raise EvocoreError("Failed to parse WeightedStats from JSON")
        return stats

    def __repr__(self) -> str:
        return (f"WeightedStats(mean={self.mean:.6f}, std={self.std:.6f}, "
                f"count={self.count})")


class WeightedArray:
    """
    Array of weighted statistics for multiple parameters.

    Tracks weighted statistics for multiple values simultaneously.

    Example:
        >>> arr = WeightedArray(param_count=3)
        >>> arr.update([0.1, 0.2, 0.3], weight=1.0)
        >>> arr.update([0.2, 0.3, 0.4], weight=2.0)
        >>> means = arr.get_means()
    """

    __slots__ = ('_array', '_ffi', '_lib', '_count')

    def __init__(self, param_count: int, *, _raw: bool = False):
        """
        Initialize weighted array.

        Args:
            param_count: Number of parameters to track
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._array = None
            self._ffi = None
            self._lib = None
            self._count = 0
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib
        self._count = param_count

        self._array = lib.evocore_weighted_array_create(param_count)
        if self._array == ffi.NULL:
            raise EvocoreError("Failed to create weighted array")

    def __del__(self):
        """Clean up weighted array."""
        if hasattr(self, '_array') and self._array is not None:
            self._lib.evocore_weighted_array_free(self._array)

    @property
    def count(self) -> int:
        """Number of parameters tracked."""
        return self._count

    def update(self, values: np.ndarray, weights: Optional[np.ndarray] = None,
               global_weight: float = 1.0) -> bool:
        """
        Update all statistics with new values.

        Args:
            values: Array of values (length must match param_count)
            weights: Optional per-value weights (default: all 1.0)
            global_weight: Global weight multiplier

        Returns:
            True if successful
        """
        values = np.asarray(values, dtype=np.float64)
        if len(values) != self._count:
            raise ValueError(f"Expected {self._count} values, got {len(values)}")

        if weights is None:
            weights = np.ones(self._count, dtype=np.float64)
        else:
            weights = np.asarray(weights, dtype=np.float64)
            if len(weights) != self._count:
                raise ValueError(f"Expected {self._count} weights")

        values_arr = self._ffi.new("double[]", values.tolist())
        weights_arr = self._ffi.new("double[]", weights.tolist())

        return self._lib.evocore_weighted_array_update(
            self._array, values_arr, weights_arr, self._count, global_weight
        )

    def get_means(self) -> np.ndarray:
        """
        Get weighted means for all parameters.

        Returns:
            Array of means
        """
        out = self._ffi.new("double[]", self._count)
        success = self._lib.evocore_weighted_array_get_means(self._array, out, self._count)
        if not success:
            return np.zeros(self._count)
        return np.array([out[i] for i in range(self._count)])

    def get_stds(self) -> np.ndarray:
        """
        Get weighted standard deviations for all parameters.

        Returns:
            Array of standard deviations
        """
        out = self._ffi.new("double[]", self._count)
        success = self._lib.evocore_weighted_array_get_stds(self._array, out, self._count)
        if not success:
            return np.zeros(self._count)
        return np.array([out[i] for i in range(self._count)])

    def sample(self, exploration_factor: float = 0.5,
               seed: Optional[int] = None) -> np.ndarray:
        """
        Sample values from the distributions.

        Args:
            exploration_factor: How much to explore (0=mean, 1=full variance)
            seed: Optional random seed

        Returns:
            Array of sampled values
        """
        out = self._ffi.new("double[]", self._count)
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)

        success = self._lib.evocore_weighted_array_sample(
            self._array, out, self._count, exploration_factor, seed_ptr
        )

        if not success:
            # Return means if sampling fails
            return self.get_means()

        return np.array([out[i] for i in range(self._count)])

    def reset(self) -> None:
        """Reset all statistics."""
        self._lib.evocore_weighted_array_reset(self._array)

    def __repr__(self) -> str:
        means = self.get_means()
        return f"WeightedArray(count={self._count}, means={means})"


__all__ = ['WeightedStats', 'WeightedArray']
