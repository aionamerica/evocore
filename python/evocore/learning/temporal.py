"""
Temporal learning system for evocore Python bindings.

Provides time-bucketed parameter learning with trend detection.
"""

from typing import Optional, List
from enum import IntEnum
from datetime import datetime
import time
import numpy as np
from ..utils.error import check_error, check_bool, EvocoreError


class BucketType(IntEnum):
    """Time bucket granularity."""
    MINUTE = 0
    HOUR = 1
    DAY = 2
    WEEK = 3
    MONTH = 4
    YEAR = 5


class TemporalBucket:
    """
    A time bucket containing parameter statistics.

    Wraps evocore_temporal_bucket_t.
    """

    __slots__ = ('_bucket', '_ffi', '_lib', '_system')

    def __init__(self, bucket_ptr, ffi, lib, system: "TemporalSystem"):
        self._bucket = bucket_ptr
        self._ffi = ffi
        self._lib = lib
        self._system = system

    @property
    def start_time(self) -> datetime:
        """Bucket start time."""
        return datetime.fromtimestamp(self._bucket.start_time)

    @property
    def end_time(self) -> datetime:
        """Bucket end time."""
        return datetime.fromtimestamp(self._bucket.end_time)

    @property
    def is_complete(self) -> bool:
        """Whether this bucket's time period has ended."""
        return self._bucket.is_complete

    @property
    def param_count(self) -> int:
        """Number of parameters tracked."""
        return self._bucket.param_count

    @property
    def sample_count(self) -> int:
        """Number of samples in this bucket."""
        return self._bucket.sample_count

    @property
    def avg_fitness(self) -> float:
        """Average fitness in this bucket."""
        return self._bucket.avg_fitness

    @property
    def best_fitness(self) -> float:
        """Best fitness in this bucket."""
        return self._bucket.best_fitness

    @property
    def worst_fitness(self) -> float:
        """Worst fitness in this bucket."""
        return self._bucket.worst_fitness

    def __repr__(self) -> str:
        return (f"TemporalBucket(start={self.start_time}, samples={self.sample_count}, "
                f"avg_fitness={self.avg_fitness:.4f})")


class TemporalSystem:
    """
    Time-bucketed parameter learning system.

    Tracks parameter statistics over time with configurable bucket
    granularity. Supports trend detection and regime change detection.

    Example:
        >>> temporal = TemporalSystem(BucketType.HOUR, param_count=5, retention=24)
        >>> temporal.learn("BTC:1h", [0.1, 0.2, 0.3, 0.4, 0.5], fitness=0.75)
        >>> params, confidence = temporal.get_organic_mean("BTC:1h")
    """

    __slots__ = ('_system', '_ffi', '_lib', '_bucket_type', '_param_count', '_retention')

    def __init__(self, bucket_type: BucketType, param_count: int,
                 retention_count: int = 100, *, _raw: bool = False):
        """
        Initialize temporal learning system.

        Args:
            bucket_type: Time granularity for buckets
            param_count: Number of parameters to track
            retention_count: Number of buckets to retain
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._system = None
            self._ffi = None
            self._lib = None
            self._bucket_type = BucketType.HOUR
            self._param_count = 0
            self._retention = 0
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib
        self._bucket_type = bucket_type
        self._param_count = param_count
        self._retention = retention_count

        self._system = lib.evocore_temporal_create(int(bucket_type), param_count, retention_count)

        if self._system == ffi.NULL:
            raise EvocoreError("Failed to create temporal system")

    def __del__(self):
        """Clean up temporal system."""
        if hasattr(self, '_system') and self._system is not None:
            self._lib.evocore_temporal_free(self._system)

    @property
    def bucket_type(self) -> BucketType:
        """Time bucket granularity."""
        return self._bucket_type

    @property
    def param_count(self) -> int:
        """Number of parameters tracked."""
        return self._param_count

    @property
    def retention_count(self) -> int:
        """Maximum number of buckets retained."""
        return self._retention

    @property
    def bucket_count(self) -> int:
        """Current number of buckets."""
        return self._lib.evocore_temporal_bucket_count(self._system)

    @property
    def context_count(self) -> int:
        """Number of contexts with data."""
        return self._lib.evocore_temporal_context_count(self._system)

    def bucket_duration(self) -> int:
        """Get duration of a bucket in seconds."""
        return self._lib.evocore_temporal_bucket_duration(int(self._bucket_type))

    def learn(self, context_key: str, parameters: np.ndarray,
              fitness: float, timestamp: Optional[datetime] = None) -> bool:
        """
        Learn from an experience.

        Args:
            context_key: Context identifier
            parameters: Parameter values
            fitness: Fitness value
            timestamp: Optional timestamp (default: now)

        Returns:
            True if successful
        """
        params = np.asarray(parameters, dtype=np.float64)
        if len(params) != self._param_count:
            raise ValueError(f"Expected {self._param_count} parameters")

        params_arr = self._ffi.new("double[]", params.tolist())

        if timestamp is None:
            return self._lib.evocore_temporal_learn_now(
                self._system, context_key.encode(), params_arr, len(params), fitness
            )
        else:
            ts = int(timestamp.timestamp())
            return self._lib.evocore_temporal_learn(
                self._system, context_key.encode(), params_arr, len(params), fitness, ts
            )

    def get_organic_mean(self, context_key: str) -> tuple:
        """
        Get recency-weighted mean parameters.

        More recent buckets have higher weight.

        Args:
            context_key: Context identifier

        Returns:
            Tuple of (parameters, confidence)
        """
        out_params = self._ffi.new("double[]", self._param_count)
        out_confidence = self._ffi.new("double *")

        success = self._lib.evocore_temporal_get_organic_mean(
            self._system, context_key.encode(), out_params, self._param_count, out_confidence
        )

        if not success:
            return np.zeros(self._param_count), 0.0

        params = np.array([out_params[i] for i in range(self._param_count)])
        return params, out_confidence[0]

    def get_weighted_mean(self, context_key: str) -> np.ndarray:
        """
        Get weighted mean parameters across all buckets.

        Args:
            context_key: Context identifier

        Returns:
            Mean parameters
        """
        out_params = self._ffi.new("double[]", self._param_count)

        success = self._lib.evocore_temporal_get_weighted_mean(
            self._system, context_key.encode(), out_params, self._param_count
        )

        if not success:
            return np.zeros(self._param_count)

        return np.array([out_params[i] for i in range(self._param_count)])

    def get_trend(self, context_key: str) -> np.ndarray:
        """
        Get parameter trends (slopes).

        Args:
            context_key: Context identifier

        Returns:
            Array of trend slopes (positive = increasing)
        """
        out_slopes = self._ffi.new("double[]", self._param_count)

        success = self._lib.evocore_temporal_get_trend(
            self._system, context_key.encode(), out_slopes, self._param_count
        )

        if not success:
            return np.zeros(self._param_count)

        return np.array([out_slopes[i] for i in range(self._param_count)])

    def trend_direction(self, slope: float) -> int:
        """
        Interpret trend direction.

        Args:
            slope: Trend slope value

        Returns:
            -1 (decreasing), 0 (stable), or 1 (increasing)
        """
        return self._lib.evocore_temporal_trend_direction(slope)

    def compare_recent(self, context_key: str, recent_buckets: int = 3) -> np.ndarray:
        """
        Compare recent buckets to overall history.

        Args:
            context_key: Context identifier
            recent_buckets: Number of recent buckets to compare

        Returns:
            Array of drift values per parameter
        """
        out_drift = self._ffi.new("double[]", self._param_count)

        success = self._lib.evocore_temporal_compare_recent(
            self._system, context_key.encode(), recent_buckets, out_drift, self._param_count
        )

        if not success:
            return np.zeros(self._param_count)

        return np.array([out_drift[i] for i in range(self._param_count)])

    def detect_regime_change(self, context_key: str, recent_buckets: int = 3,
                             threshold: float = 0.1) -> bool:
        """
        Detect if a regime change has occurred.

        Args:
            context_key: Context identifier
            recent_buckets: Number of recent buckets to compare
            threshold: Drift threshold for regime change

        Returns:
            True if regime change detected
        """
        return self._lib.evocore_temporal_detect_regime_change(
            self._system, context_key.encode(), recent_buckets, threshold
        )

    def sample_organic(self, context_key: str, exploration_factor: float = 0.5,
                       seed: Optional[int] = None) -> np.ndarray:
        """
        Sample parameters with recency weighting.

        Args:
            context_key: Context identifier
            exploration_factor: How much to explore
            seed: Optional random seed

        Returns:
            Sampled parameters
        """
        out_params = self._ffi.new("double[]", self._param_count)
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)

        success = self._lib.evocore_temporal_sample_organic(
            self._system, context_key.encode(), out_params, self._param_count,
            exploration_factor, seed_ptr
        )

        if not success:
            return np.random.uniform(0, 1, self._param_count)

        return np.array([out_params[i] for i in range(self._param_count)])

    def sample_trend(self, context_key: str, trend_strength: float = 0.5,
                     seed: Optional[int] = None) -> np.ndarray:
        """
        Sample parameters following the trend.

        Args:
            context_key: Context identifier
            trend_strength: How much to follow the trend
            seed: Optional random seed

        Returns:
            Sampled parameters
        """
        out_params = self._ffi.new("double[]", self._param_count)
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)

        success = self._lib.evocore_temporal_sample_trend(
            self._system, context_key.encode(), out_params, self._param_count,
            trend_strength, seed_ptr
        )

        if not success:
            return self.sample_organic(context_key, 0.5, seed)

        return np.array([out_params[i] for i in range(self._param_count)])

    def get_current_bucket(self, context_key: str) -> Optional[TemporalBucket]:
        """
        Get the current time bucket.

        Args:
            context_key: Context identifier

        Returns:
            Current bucket or None
        """
        bucket_ptr = self._ffi.new("evocore_temporal_bucket_t **")

        success = self._lib.evocore_temporal_get_current_bucket(
            self._system, context_key.encode(), bucket_ptr
        )

        if not success or bucket_ptr[0] == self._ffi.NULL:
            return None

        return TemporalBucket(bucket_ptr[0], self._ffi, self._lib, self)

    def prune_old(self) -> int:
        """
        Remove old buckets beyond retention limit.

        Returns:
            Number of buckets pruned
        """
        return self._lib.evocore_temporal_prune_old(self._system)

    def reset_context(self, context_key: str) -> bool:
        """
        Reset a specific context.

        Args:
            context_key: Context identifier

        Returns:
            True if successful
        """
        return self._lib.evocore_temporal_reset_context(self._system, context_key.encode())

    def reset_all(self) -> None:
        """Reset all contexts."""
        self._lib.evocore_temporal_reset_all(self._system)

    def save_json(self, filepath: str) -> bool:
        """
        Save to JSON file.

        Args:
            filepath: Path to save to

        Returns:
            True if successful
        """
        return self._lib.evocore_temporal_save_json(self._system, filepath.encode())

    @classmethod
    def load_json(cls, filepath: str) -> "TemporalSystem":
        """
        Load from JSON file.

        Args:
            filepath: Path to load from

        Returns:
            Loaded TemporalSystem
        """
        from .._evocore import ffi, lib

        system_ptr = ffi.new("evocore_temporal_system_t **")
        success = lib.evocore_temporal_load_json(filepath.encode(), system_ptr)

        if not success or system_ptr[0] == ffi.NULL:
            raise EvocoreError(f"Failed to load temporal system from {filepath}")

        obj = cls(_raw=True)
        obj._system = system_ptr[0]
        obj._ffi = ffi
        obj._lib = lib
        obj._bucket_type = BucketType(obj._system.bucket_type)
        obj._param_count = obj._system.param_count
        obj._retention = obj._system.retention_count

        return obj

    def __repr__(self) -> str:
        return (f"TemporalSystem(bucket_type={self._bucket_type.name}, "
                f"param_count={self._param_count}, buckets={self.bucket_count})")


__all__ = ['BucketType', 'TemporalBucket', 'TemporalSystem']
