"""
Statistics and monitoring for evocore Python bindings.

Provides evolution statistics tracking and progress reporting.
"""

from typing import Optional, Callable, Any, Dict
from dataclasses import dataclass
from ..core.population import Population
from .error import check_error, EvocoreError


@dataclass
class StatsConfig:
    """Configuration for statistics tracking."""
    improvement_threshold: float = 0.001
    stagnation_generations: int = 20
    diversity_threshold: float = 0.1
    track_timing: bool = True
    track_memory: bool = False
    track_diversity: bool = True


class Stats:
    """
    Evolution statistics tracker.

    Tracks fitness, convergence, diversity, and timing.

    Example:
        >>> stats = Stats()
        >>> stats.update(population)
        >>> print(f"Best: {stats.best_fitness}, Converged: {stats.is_converged}")
    """

    __slots__ = ('_stats', '_ffi', '_lib')

    def __init__(self, config: Optional[StatsConfig] = None, *, _raw: bool = False):
        """
        Initialize statistics tracker.

        Args:
            config: Statistics configuration
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._stats = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        if config:
            c_config = ffi.new("evocore_stats_config_t *")
            c_config.improvement_threshold = config.improvement_threshold
            c_config.stagnation_generations = config.stagnation_generations
            c_config.diversity_threshold = config.diversity_threshold
            c_config.track_timing = config.track_timing
            c_config.track_memory = config.track_memory
            c_config.track_diversity = config.track_diversity
        else:
            c_config = ffi.NULL

        self._stats = lib.evocore_stats_create(c_config)
        if self._stats == ffi.NULL:
            raise EvocoreError("Failed to create stats tracker")

    def __del__(self):
        if hasattr(self, '_stats') and self._stats is not None:
            self._lib.evocore_stats_free(self._stats)

    def update(self, population: Population) -> None:
        """
        Update statistics from population.

        Args:
            population: Population to gather stats from
        """
        err = self._lib.evocore_stats_update(self._stats, population._pop)
        check_error(err, self._lib)

    def record_operations(self, evaluations: int, mutations: int,
                          crossovers: int) -> None:
        """
        Record operation counts.

        Args:
            evaluations: Number of fitness evaluations
            mutations: Number of mutations
            crossovers: Number of crossovers
        """
        err = self._lib.evocore_stats_record_operations(
            self._stats, evaluations, mutations, crossovers
        )
        check_error(err, self._lib)

    @property
    def generation(self) -> int:
        """Current generation."""
        return self._stats.generation

    @property
    def best_fitness(self) -> float:
        """Best fitness."""
        return self._stats.best_fitness

    @property
    def avg_fitness(self) -> float:
        """Average fitness."""
        return self._stats.avg_fitness

    @property
    def worst_fitness(self) -> float:
        """Worst fitness."""
        return self._stats.worst_fitness

    @property
    def fitness_stddev(self) -> float:
        """Fitness standard deviation."""
        return self._stats.fitness_stddev

    @property
    def fitness_improvement(self) -> float:
        """Fitness improvement from last generation."""
        return self._stats.fitness_improvement

    @property
    def is_converged(self) -> bool:
        """Whether evolution has converged."""
        return self._lib.evocore_stats_is_converged(self._stats)

    @property
    def is_stagnant(self) -> bool:
        """Whether evolution is stagnant."""
        return self._lib.evocore_stats_is_stagnant(self._stats)

    @property
    def stagnant_generations(self) -> int:
        """Number of generations without improvement."""
        return self._stats.stagnant_generations

    @property
    def diversity(self) -> float:
        """Population diversity."""
        return self._stats.diversity

    @property
    def generation_time_ms(self) -> float:
        """Time for last generation in milliseconds."""
        return self._stats.generation_time_ms

    @property
    def total_time_ms(self) -> float:
        """Total evolution time in milliseconds."""
        return self._stats.total_time_ms

    @property
    def evaluation_count(self) -> int:
        """Total number of evaluations."""
        return self._stats.evaluation_count

    @property
    def mutation_count(self) -> int:
        """Total number of mutations."""
        return self._stats.mutation_count

    @property
    def crossover_count(self) -> int:
        """Total number of crossovers."""
        return self._stats.crossover_count

    @property
    def memory_used(self) -> int:
        """Current memory usage in bytes."""
        return self._stats.memory_used

    def to_dict(self) -> Dict[str, Any]:
        """
        Convert to dictionary.

        Returns:
            Dictionary of statistics
        """
        return {
            'generation': self.generation,
            'best_fitness': self.best_fitness,
            'avg_fitness': self.avg_fitness,
            'worst_fitness': self.worst_fitness,
            'fitness_stddev': self.fitness_stddev,
            'fitness_improvement': self.fitness_improvement,
            'is_converged': self.is_converged,
            'is_stagnant': self.is_stagnant,
            'stagnant_generations': self.stagnant_generations,
            'diversity': self.diversity,
            'generation_time_ms': self.generation_time_ms,
            'total_time_ms': self.total_time_ms,
            'evaluation_count': self.evaluation_count,
            'mutation_count': self.mutation_count,
            'crossover_count': self.crossover_count,
            'memory_used': self.memory_used,
        }

    def __repr__(self) -> str:
        return (f"Stats(gen={self.generation}, best={self.best_fitness:.4f}, "
                f"avg={self.avg_fitness:.4f})")


class ProgressReporter:
    """
    Progress reporting for evolution.

    Calls a callback function periodically during evolution.
    """

    def __init__(self, callback: Callable[[Stats], None],
                 every_n: int = 1, verbose: bool = False):
        """
        Initialize progress reporter.

        Args:
            callback: Function to call with Stats
            every_n: Report every N generations
            verbose: Enable verbose output
        """
        self._callback = callback
        self._every_n = every_n
        self._verbose = verbose
        self._last_reported = -1

    def report(self, stats: Stats) -> None:
        """
        Report progress if due.

        Args:
            stats: Current statistics
        """
        gen = stats.generation
        if gen - self._last_reported >= self._every_n:
            self._callback(stats)
            self._last_reported = gen

    def force_report(self, stats: Stats) -> None:
        """
        Force a progress report.

        Args:
            stats: Current statistics
        """
        self._callback(stats)
        self._last_reported = stats.generation


def print_progress(stats: Stats) -> None:
    """
    Default progress printer.

    Args:
        stats: Statistics to print
    """
    print(f"Gen {stats.generation:4d}: best={stats.best_fitness:.6f}, "
          f"avg={stats.avg_fitness:.6f}, div={stats.diversity:.4f}")


def compute_diversity(population: Population) -> float:
    """
    Compute population diversity.

    Args:
        population: Population to analyze

    Returns:
        Diversity measure
    """
    from .._evocore import lib
    return lib.evocore_stats_diversity(population._pop)


def fitness_distribution(population: Population) -> Dict[str, float]:
    """
    Get fitness distribution statistics.

    Args:
        population: Population to analyze

    Returns:
        Dictionary with min, max, mean, stddev
    """
    from .._evocore import ffi, lib

    out_min = ffi.new("double *")
    out_max = ffi.new("double *")
    out_mean = ffi.new("double *")
    out_stddev = ffi.new("double *")

    err = lib.evocore_stats_fitness_distribution(
        population._pop, out_min, out_max, out_mean, out_stddev
    )
    check_error(err, lib)

    return {
        'min': out_min[0],
        'max': out_max[0],
        'mean': out_mean[0],
        'stddev': out_stddev[0],
    }


__all__ = [
    'StatsConfig',
    'Stats',
    'ProgressReporter',
    'print_progress',
    'compute_diversity',
    'fitness_distribution',
]
