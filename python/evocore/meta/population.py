"""
Meta-population for evocore Python bindings.

Provides meta-evolution population management.
"""

from typing import Optional, List, Iterator
from .params import MetaParams
from ..utils.error import check_error, EvocoreError


class MetaIndividual:
    """
    An individual in the meta-evolution population.

    Contains meta-parameters and tracks fitness history.
    """

    __slots__ = ('_individual', '_ffi', '_lib', '_meta_pop')

    def __init__(self, individual_ptr, ffi, lib, meta_pop: "MetaPopulation"):
        self._individual = individual_ptr
        self._ffi = ffi
        self._lib = lib
        self._meta_pop = meta_pop

    @property
    def params(self) -> MetaParams:
        """Get the meta-parameters."""
        params = MetaParams(_raw=True)
        params._ffi = self._ffi
        params._lib = self._lib
        params._params = self._ffi.addressof(self._individual.params)
        return params

    @property
    def meta_fitness(self) -> float:
        """Meta-fitness score."""
        return self._individual.meta_fitness

    @property
    def generation(self) -> int:
        """Generation this individual was created."""
        return self._individual.generation

    def record_fitness(self, fitness: float) -> None:
        """
        Record a fitness observation.

        Args:
            fitness: Fitness value to record
        """
        err = self._lib.evocore_meta_individual_record_fitness(self._individual, fitness)
        check_error(err, self._lib)

    def average_fitness(self) -> float:
        """
        Get average fitness from history.

        Returns:
            Average fitness
        """
        return self._lib.evocore_meta_individual_average_fitness(self._individual)

    def improvement_trend(self) -> float:
        """
        Get fitness improvement trend.

        Returns:
            Trend value (positive = improving)
        """
        return self._lib.evocore_meta_individual_improvement_trend(self._individual)

    def __repr__(self) -> str:
        return (f"MetaIndividual(meta_fitness={self.meta_fitness:.4f}, "
                f"generation={self.generation})")


class MetaPopulation:
    """
    Population of meta-evolution individuals.

    Manages multiple parameter configurations and evolves them
    based on their effectiveness.

    Example:
        >>> meta_pop = MetaPopulation(size=10)
        >>> best = meta_pop.best()
        >>> meta_pop.evolve()
    """

    __slots__ = ('_meta_pop', '_ffi', '_lib')

    def __init__(self, size: int = 10, seed: Optional[int] = None, *, _raw: bool = False):
        """
        Initialize meta-population.

        Args:
            size: Number of meta-individuals
            seed: Optional random seed
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._meta_pop = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        self._meta_pop = ffi.new("evocore_meta_population_t *")
        seed_ptr = ffi.new("unsigned int *", seed if seed is not None else 0)

        err = lib.evocore_meta_population_init(self._meta_pop, size, seed_ptr)
        check_error(err, lib)

    def __del__(self):
        """Clean up meta-population."""
        if hasattr(self, '_meta_pop') and self._meta_pop is not None:
            self._lib.evocore_meta_population_cleanup(self._meta_pop)

    @property
    def count(self) -> int:
        """Number of individuals."""
        return self._meta_pop.count

    @property
    def current_generation(self) -> int:
        """Current generation."""
        return self._meta_pop.current_generation

    @property
    def best_meta_fitness(self) -> float:
        """Best meta-fitness achieved."""
        return self._meta_pop.best_meta_fitness

    @property
    def best_params(self) -> MetaParams:
        """Get the best parameters found."""
        params = MetaParams(_raw=True)
        params._ffi = self._ffi
        params._lib = self._lib
        params._params = self._ffi.addressof(self._meta_pop.best_params)
        return params

    @property
    def initialized(self) -> bool:
        """Whether population is initialized."""
        return self._meta_pop.initialized

    def best(self) -> Optional[MetaIndividual]:
        """
        Get the best individual.

        Returns:
            Best meta-individual or None
        """
        ind_ptr = self._lib.evocore_meta_population_best(self._meta_pop)
        if ind_ptr == self._ffi.NULL:
            return None
        return MetaIndividual(ind_ptr, self._ffi, self._lib, self)

    def get(self, index: int) -> Optional[MetaIndividual]:
        """
        Get individual by index.

        Args:
            index: Index of individual

        Returns:
            MetaIndividual or None
        """
        if index < 0 or index >= self.count:
            return None
        return MetaIndividual(
            self._ffi.addressof(self._meta_pop.individuals[index]),
            self._ffi, self._lib, self
        )

    def evolve(self, seed: Optional[int] = None) -> None:
        """
        Evolve the meta-population.

        Creates new generation through selection and mutation.

        Args:
            seed: Optional random seed
        """
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)
        err = self._lib.evocore_meta_population_evolve(self._meta_pop, seed_ptr)
        check_error(err, self._lib)

    def sort(self) -> None:
        """Sort population by meta-fitness (descending)."""
        self._lib.evocore_meta_population_sort(self._meta_pop)

    def converged(self, threshold: float = 0.01, generations: int = 10) -> bool:
        """
        Check if population has converged.

        Args:
            threshold: Convergence threshold
            generations: Number of generations to check

        Returns:
            True if converged
        """
        return self._lib.evocore_meta_population_converged(
            self._meta_pop, threshold, generations
        )

    def __len__(self) -> int:
        """Return population size."""
        return self.count

    def __iter__(self) -> Iterator[MetaIndividual]:
        """Iterate over individuals."""
        for i in range(self.count):
            ind = self.get(i)
            if ind is not None:
                yield ind

    def __getitem__(self, index: int) -> MetaIndividual:
        """Get individual by index."""
        if index < 0:
            index = self.count + index
        ind = self.get(index)
        if ind is None:
            raise IndexError(f"Index {index} out of range")
        return ind

    def __repr__(self) -> str:
        return (f"MetaPopulation(count={self.count}, generation={self.current_generation}, "
                f"best_fitness={self.best_meta_fitness:.4f})")


# Meta-adaptation functions

def meta_adapt(params: MetaParams, recent_fitness: List[float], improvement: bool) -> None:
    """
    Adapt parameters based on recent performance.

    Args:
        params: Parameters to adapt
        recent_fitness: List of recent fitness values
        improvement: Whether improvement was observed
    """
    from .._evocore import ffi, lib
    fitness_arr = ffi.new("double[]", recent_fitness)
    lib.evocore_meta_adapt(params._params, fitness_arr, len(recent_fitness), improvement)


def meta_suggest_mutation_rate(diversity: float, params: MetaParams) -> None:
    """
    Suggest mutation rate based on diversity.

    Args:
        diversity: Current population diversity
        params: Parameters to update
    """
    from .._evocore import lib
    lib.evocore_meta_suggest_mutation_rate(diversity, params._params)


def meta_suggest_selection_pressure(fitness_stddev: float, params: MetaParams) -> None:
    """
    Suggest selection pressure based on fitness variance.

    Args:
        fitness_stddev: Standard deviation of fitness
        params: Parameters to update
    """
    from .._evocore import lib
    lib.evocore_meta_suggest_selection_pressure(fitness_stddev, params._params)


def meta_evaluate(params: MetaParams, best_fitness: float, avg_fitness: float,
                  diversity: float, generations: int) -> float:
    """
    Evaluate meta-fitness of parameters.

    Args:
        params: Parameters to evaluate
        best_fitness: Best fitness achieved
        avg_fitness: Average fitness
        diversity: Population diversity
        generations: Number of generations

    Returns:
        Meta-fitness score
    """
    from .._evocore import lib
    return lib.evocore_meta_evaluate(
        params._params, best_fitness, avg_fitness, diversity, generations
    )


def meta_learn_outcome(mutation_rate: float, exploration_factor: float,
                       fitness: float, learning_rate: float) -> None:
    """
    Learn from an evolution outcome.

    Args:
        mutation_rate: Mutation rate used
        exploration_factor: Exploration factor used
        fitness: Resulting fitness
        learning_rate: Learning rate
    """
    from .._evocore import lib
    lib.evocore_meta_learn_outcome(mutation_rate, exploration_factor, fitness, learning_rate)


def meta_get_learned_params(min_samples: int = 10) -> Optional[tuple]:
    """
    Get learned parameter suggestions.

    Args:
        min_samples: Minimum samples required

    Returns:
        Tuple of (mutation_rate, exploration_factor) or None
    """
    from .._evocore import ffi, lib

    mut_rate = ffi.new("double *")
    explore = ffi.new("double *")

    success = lib.evocore_meta_get_learned_params(mut_rate, explore, min_samples)

    if not success:
        return None

    return (mut_rate[0], explore[0])


def meta_reset_learning() -> None:
    """Reset all learned parameters."""
    from .._evocore import lib
    lib.evocore_meta_reset_learning()


__all__ = [
    'MetaIndividual',
    'MetaPopulation',
    'meta_adapt',
    'meta_suggest_mutation_rate',
    'meta_suggest_selection_pressure',
    'meta_evaluate',
    'meta_learn_outcome',
    'meta_get_learned_params',
    'meta_reset_learning',
]
