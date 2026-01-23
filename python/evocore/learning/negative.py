"""
Negative learning system for evocore Python bindings.

Tracks failures and penalizes similar solutions.
"""

from typing import Optional, Tuple
from enum import IntEnum
from ..core.genome import Genome
from ..utils.error import check_error, EvocoreError


class FailureSeverity(IntEnum):
    """Failure severity levels."""
    NONE = 0
    MILD = 1
    MODERATE = 2
    SEVERE = 3
    FATAL = 4


class NegativeStats:
    """
    Statistics about negative learning.

    Wraps evocore_negative_stats_t.
    """

    __slots__ = ('_stats', '_ffi', '_lib')

    def __init__(self, stats_ptr, ffi, lib):
        self._stats = stats_ptr
        self._ffi = ffi
        self._lib = lib

    @property
    def total_count(self) -> int:
        """Total number of recorded failures."""
        return self._stats.total_count

    @property
    def active_count(self) -> int:
        """Number of active (non-decayed) failures."""
        return self._stats.active_count

    @property
    def mild_count(self) -> int:
        """Number of mild failures."""
        return self._stats.mild_count

    @property
    def moderate_count(self) -> int:
        """Number of moderate failures."""
        return self._stats.moderate_count

    @property
    def severe_count(self) -> int:
        """Number of severe failures."""
        return self._stats.severe_count

    @property
    def fatal_count(self) -> int:
        """Number of fatal failures."""
        return self._stats.fatal_count

    @property
    def avg_penalty(self) -> float:
        """Average penalty score."""
        return self._stats.avg_penalty

    @property
    def max_penalty(self) -> float:
        """Maximum penalty score."""
        return self._stats.max_penalty

    @property
    def repeat_victims(self) -> int:
        """Number of repeated failures."""
        return self._stats.repeat_victims

    def __repr__(self) -> str:
        return (f"NegativeStats(total={self.total_count}, active={self.active_count}, "
                f"avg_penalty={self.avg_penalty:.4f})")


class NegativeLearning:
    """
    Negative learning system for avoiding past failures.

    Records failed solutions and penalizes new solutions that are
    similar to known failures.

    Example:
        >>> neg = NegativeLearning(capacity=100)
        >>> neg.record_failure(genome, fitness=-1.0, generation=5)
        >>> penalty = neg.check_penalty(new_genome)
        >>> adjusted_fitness = neg.adjust_fitness(new_genome, raw_fitness)
    """

    __slots__ = ('_neg', '_ffi', '_lib')

    def __init__(self, capacity: int = 100, base_penalty: float = 0.1,
                 decay_rate: float = 0.95, *, _raw: bool = False):
        """
        Initialize negative learning system.

        Args:
            capacity: Maximum number of failures to track
            base_penalty: Base penalty for similar failures
            decay_rate: How fast penalties decay each generation
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._neg = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        self._neg = ffi.new("evocore_negative_learning_t *")
        err = lib.evocore_negative_learning_init(self._neg, capacity, base_penalty, decay_rate)
        check_error(err, lib)

    @classmethod
    def with_defaults(cls, capacity: int = 100) -> "NegativeLearning":
        """
        Create negative learning with default parameters.

        Args:
            capacity: Maximum number of failures to track

        Returns:
            New NegativeLearning instance
        """
        from .._evocore import ffi, lib

        neg = cls(_raw=True)
        neg._ffi = ffi
        neg._lib = lib
        neg._neg = ffi.new("evocore_negative_learning_t *")

        err = lib.evocore_negative_learning_init_default(neg._neg, capacity)
        check_error(err, lib)

        return neg

    def __del__(self):
        """Clean up negative learning."""
        if hasattr(self, '_neg') and self._neg is not None:
            self._lib.evocore_negative_learning_cleanup(self._neg)

    def set_thresholds(self, mild: float, moderate: float,
                       severe: float, fatal: float) -> None:
        """
        Set severity classification thresholds.

        Args:
            mild: Threshold for mild failures
            moderate: Threshold for moderate failures
            severe: Threshold for severe failures
            fatal: Threshold for fatal failures
        """
        err = self._lib.evocore_negative_learning_set_thresholds(
            self._neg, mild, moderate, severe, fatal
        )
        check_error(err, self._lib)

    def record_failure(self, genome: Genome, fitness: float, generation: int) -> None:
        """
        Record a failure.

        Args:
            genome: Failed genome
            fitness: Fitness value
            generation: Generation when failure occurred
        """
        err = self._lib.evocore_negative_learning_record_failure(
            self._neg, genome._genome, fitness, generation
        )
        check_error(err, self._lib)

    def record_failure_severity(self, genome: Genome, fitness: float,
                                severity: FailureSeverity, generation: int) -> None:
        """
        Record a failure with explicit severity.

        Args:
            genome: Failed genome
            fitness: Fitness value
            severity: Failure severity level
            generation: Generation when failure occurred
        """
        err = self._lib.evocore_negative_learning_record_failure_severity(
            self._neg, genome._genome, fitness, int(severity), generation
        )
        check_error(err, self._lib)

    def set_generation(self, generation: int) -> None:
        """
        Set current generation for decay calculations.

        Args:
            generation: Current generation number
        """
        self._lib.evocore_negative_learning_set_generation(self._neg, generation)

    def check_penalty(self, genome: Genome) -> float:
        """
        Check penalty for a genome based on similarity to failures.

        Args:
            genome: Genome to check

        Returns:
            Penalty value (0 if no similar failures)
        """
        penalty = self._ffi.new("double *")
        err = self._lib.evocore_negative_learning_check_penalty(
            self._neg, genome._genome, penalty
        )
        check_error(err, self._lib)
        return penalty[0]

    def is_forbidden(self, genome: Genome, threshold: float = 0.5) -> bool:
        """
        Check if a genome is forbidden (too similar to fatal failures).

        Args:
            genome: Genome to check
            threshold: Similarity threshold

        Returns:
            True if genome should be forbidden
        """
        return self._lib.evocore_negative_learning_is_forbidden(
            self._neg, genome._genome, threshold
        )

    def adjust_fitness(self, genome: Genome, raw_fitness: float) -> float:
        """
        Adjust fitness based on similarity to failures.

        Args:
            genome: Genome being evaluated
            raw_fitness: Original fitness value

        Returns:
            Adjusted fitness with penalties applied
        """
        adjusted = self._ffi.new("double *")
        err = self._lib.evocore_negative_learning_adjust_fitness(
            self._neg, genome._genome, raw_fitness, adjusted
        )
        check_error(err, self._lib)
        return adjusted[0]

    def decay(self, generations_passed: int = 1) -> None:
        """
        Apply decay to penalties.

        Args:
            generations_passed: Number of generations to decay
        """
        self._lib.evocore_negative_learning_decay(self._neg, generations_passed)

    def prune(self, min_penalty: float = 0.01, max_age_generations: int = 100) -> int:
        """
        Remove old or low-penalty failures.

        Args:
            min_penalty: Minimum penalty to keep
            max_age_generations: Maximum age in generations

        Returns:
            Number of failures pruned
        """
        return self._lib.evocore_negative_learning_prune(
            self._neg, min_penalty, max_age_generations
        )

    def get_stats(self) -> NegativeStats:
        """
        Get statistics about recorded failures.

        Returns:
            NegativeStats object
        """
        stats = self._ffi.new("evocore_negative_stats_t *")
        err = self._lib.evocore_negative_learning_stats(self._neg, stats)
        check_error(err, self._lib)
        return NegativeStats(stats, self._ffi, self._lib)

    @property
    def count(self) -> int:
        """Total number of recorded failures."""
        return self._lib.evocore_negative_learning_count(self._neg)

    @property
    def active_count(self) -> int:
        """Number of active failures."""
        return self._lib.evocore_negative_learning_active_count(self._neg)

    def clear(self) -> None:
        """Clear all recorded failures."""
        self._lib.evocore_negative_learning_clear(self._neg)

    def set_base_penalty(self, base_penalty: float) -> None:
        """Set base penalty for failures."""
        self._lib.evocore_negative_learning_set_base_penalty(self._neg, base_penalty)

    def set_repeat_multiplier(self, multiplier: float) -> None:
        """Set multiplier for repeated failures."""
        self._lib.evocore_negative_learning_set_repeat_multiplier(self._neg, multiplier)

    def set_decay_rate(self, decay_rate: float) -> None:
        """Set penalty decay rate."""
        self._lib.evocore_negative_learning_set_decay_rate(self._neg, decay_rate)

    def set_similarity_threshold(self, threshold: float) -> None:
        """Set similarity threshold for penalty application."""
        self._lib.evocore_negative_learning_set_similarity_threshold(self._neg, threshold)

    def __repr__(self) -> str:
        return f"NegativeLearning(count={self.count}, active={self.active_count})"


def severity_string(severity: FailureSeverity) -> str:
    """
    Get string representation of severity.

    Args:
        severity: Severity level

    Returns:
        String name
    """
    from .._evocore import ffi, lib
    ptr = lib.evocore_severity_string(int(severity))
    if ptr == ffi.NULL:
        return "unknown"
    return ffi.string(ptr).decode()


def severity_from_string(s: str) -> FailureSeverity:
    """
    Parse severity from string.

    Args:
        s: String representation

    Returns:
        Severity level
    """
    from .._evocore import lib
    return FailureSeverity(lib.evocore_severity_from_string(s.encode()))


def classify_failure(fitness: float, thresholds: Tuple[float, float, float, float]) -> FailureSeverity:
    """
    Classify a failure by fitness value.

    Args:
        fitness: Fitness value
        thresholds: (mild, moderate, severe, fatal) thresholds

    Returns:
        Severity level
    """
    from .._evocore import ffi, lib
    thresh_arr = ffi.new("double[4]", list(thresholds))
    return FailureSeverity(lib.evocore_classify_failure(fitness, thresh_arr))


__all__ = [
    'FailureSeverity',
    'NegativeStats',
    'NegativeLearning',
    'severity_string',
    'severity_from_string',
    'classify_failure',
]
