"""
Meta-evolution parameters for evocore Python bindings.

Provides meta-parameter management and adaptation.
"""

from typing import Optional, Dict
from ..utils.error import check_error, EvocoreError


class MetaParams:
    """
    Meta-evolution parameters.

    Controls mutation rates, selection pressure, population dynamics,
    and learning parameters for meta-evolution.

    Example:
        >>> params = MetaParams()
        >>> params.optimization_mutation_rate = 0.1
        >>> params.mutate()
        >>> print(params.exploration_factor)
    """

    __slots__ = ('_params', '_ffi', '_lib')

    # Parameter names for get/set
    PARAM_NAMES = [
        'optimization_mutation_rate',
        'variance_mutation_rate',
        'experimentation_rate',
        'elite_protection_ratio',
        'culling_ratio',
        'fitness_threshold_for_breeding',
        'target_population_size',
        'min_population_size',
        'max_population_size',
        'learning_rate',
        'exploration_factor',
        'confidence_threshold',
        'profitable_optimization_ratio',
        'profitable_random_ratio',
        'losing_optimization_ratio',
        'losing_random_ratio',
        'meta_mutation_rate',
        'meta_learning_rate',
        'meta_convergence_threshold',
        'negative_penalty_weight',
        'negative_decay_rate',
        'negative_capacity',
        'negative_similarity_threshold',
        'negative_forbidden_threshold',
    ]

    def __init__(self, *, _raw: bool = False):
        """
        Initialize meta parameters with defaults.

        Args:
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._params = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        self._params = ffi.new("evocore_meta_params_t *")
        lib.evocore_meta_params_init(self._params)

    def validate(self) -> None:
        """
        Validate parameters are within acceptable ranges.

        Raises:
            EvocoreError: If parameters are invalid
        """
        err = self._lib.evocore_meta_params_validate(self._params)
        check_error(err, self._lib)

    def mutate(self, seed: Optional[int] = None) -> None:
        """
        Mutate parameters randomly.

        Args:
            seed: Optional random seed
        """
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)
        self._lib.evocore_meta_params_mutate(self._params, seed_ptr)

    def clone(self) -> "MetaParams":
        """
        Create a copy of these parameters.

        Returns:
            New MetaParams with copied values
        """
        new = MetaParams(_raw=True)
        new._ffi = self._ffi
        new._lib = self._lib
        new._params = self._ffi.new("evocore_meta_params_t *")
        self._lib.evocore_meta_params_clone(self._params, new._params)
        return new

    def get(self, name: str) -> float:
        """
        Get parameter value by name.

        Args:
            name: Parameter name

        Returns:
            Parameter value
        """
        return self._lib.evocore_meta_params_get(self._params, name.encode())

    def set(self, name: str, value: float) -> None:
        """
        Set parameter value by name.

        Args:
            name: Parameter name
            value: New value
        """
        err = self._lib.evocore_meta_params_set(self._params, name.encode(), value)
        check_error(err, self._lib)

    def print(self) -> None:
        """Print parameters to console."""
        self._lib.evocore_meta_params_print(self._params)

    def to_dict(self) -> Dict[str, float]:
        """
        Convert to dictionary.

        Returns:
            Dictionary of parameter names to values
        """
        return {
            'optimization_mutation_rate': self.optimization_mutation_rate,
            'variance_mutation_rate': self.variance_mutation_rate,
            'experimentation_rate': self.experimentation_rate,
            'elite_protection_ratio': self.elite_protection_ratio,
            'culling_ratio': self.culling_ratio,
            'fitness_threshold_for_breeding': self.fitness_threshold_for_breeding,
            'target_population_size': self.target_population_size,
            'min_population_size': self.min_population_size,
            'max_population_size': self.max_population_size,
            'learning_rate': self.learning_rate,
            'exploration_factor': self.exploration_factor,
            'confidence_threshold': self.confidence_threshold,
            'profitable_optimization_ratio': self.profitable_optimization_ratio,
            'profitable_random_ratio': self.profitable_random_ratio,
            'losing_optimization_ratio': self.losing_optimization_ratio,
            'losing_random_ratio': self.losing_random_ratio,
            'meta_mutation_rate': self.meta_mutation_rate,
            'meta_learning_rate': self.meta_learning_rate,
            'meta_convergence_threshold': self.meta_convergence_threshold,
            'negative_learning_enabled': self.negative_learning_enabled,
            'negative_penalty_weight': self.negative_penalty_weight,
            'negative_decay_rate': self.negative_decay_rate,
            'negative_capacity': self.negative_capacity,
            'negative_similarity_threshold': self.negative_similarity_threshold,
            'negative_forbidden_threshold': self.negative_forbidden_threshold,
        }

    @classmethod
    def from_dict(cls, d: Dict[str, float]) -> "MetaParams":
        """
        Create from dictionary.

        Args:
            d: Dictionary of parameter names to values

        Returns:
            New MetaParams
        """
        params = cls()
        for name, value in d.items():
            if hasattr(params, name):
                setattr(params, name, value)
        return params

    # Mutation rates
    @property
    def optimization_mutation_rate(self) -> float:
        return self._params.optimization_mutation_rate

    @optimization_mutation_rate.setter
    def optimization_mutation_rate(self, value: float):
        self._params.optimization_mutation_rate = value

    @property
    def variance_mutation_rate(self) -> float:
        return self._params.variance_mutation_rate

    @variance_mutation_rate.setter
    def variance_mutation_rate(self, value: float):
        self._params.variance_mutation_rate = value

    @property
    def experimentation_rate(self) -> float:
        return self._params.experimentation_rate

    @experimentation_rate.setter
    def experimentation_rate(self, value: float):
        self._params.experimentation_rate = value

    # Selection pressure
    @property
    def elite_protection_ratio(self) -> float:
        return self._params.elite_protection_ratio

    @elite_protection_ratio.setter
    def elite_protection_ratio(self, value: float):
        self._params.elite_protection_ratio = value

    @property
    def culling_ratio(self) -> float:
        return self._params.culling_ratio

    @culling_ratio.setter
    def culling_ratio(self, value: float):
        self._params.culling_ratio = value

    @property
    def fitness_threshold_for_breeding(self) -> float:
        return self._params.fitness_threshold_for_breeding

    @fitness_threshold_for_breeding.setter
    def fitness_threshold_for_breeding(self, value: float):
        self._params.fitness_threshold_for_breeding = value

    # Population dynamics
    @property
    def target_population_size(self) -> int:
        return self._params.target_population_size

    @target_population_size.setter
    def target_population_size(self, value: int):
        self._params.target_population_size = value

    @property
    def min_population_size(self) -> int:
        return self._params.min_population_size

    @min_population_size.setter
    def min_population_size(self, value: int):
        self._params.min_population_size = value

    @property
    def max_population_size(self) -> int:
        return self._params.max_population_size

    @max_population_size.setter
    def max_population_size(self, value: int):
        self._params.max_population_size = value

    # Learning
    @property
    def learning_rate(self) -> float:
        return self._params.learning_rate

    @learning_rate.setter
    def learning_rate(self, value: float):
        self._params.learning_rate = value

    @property
    def exploration_factor(self) -> float:
        return self._params.exploration_factor

    @exploration_factor.setter
    def exploration_factor(self, value: float):
        self._params.exploration_factor = value

    @property
    def confidence_threshold(self) -> float:
        return self._params.confidence_threshold

    @confidence_threshold.setter
    def confidence_threshold(self, value: float):
        self._params.confidence_threshold = value

    # Breeding ratios
    @property
    def profitable_optimization_ratio(self) -> float:
        return self._params.profitable_optimization_ratio

    @profitable_optimization_ratio.setter
    def profitable_optimization_ratio(self, value: float):
        self._params.profitable_optimization_ratio = value

    @property
    def profitable_random_ratio(self) -> float:
        return self._params.profitable_random_ratio

    @profitable_random_ratio.setter
    def profitable_random_ratio(self, value: float):
        self._params.profitable_random_ratio = value

    @property
    def losing_optimization_ratio(self) -> float:
        return self._params.losing_optimization_ratio

    @losing_optimization_ratio.setter
    def losing_optimization_ratio(self, value: float):
        self._params.losing_optimization_ratio = value

    @property
    def losing_random_ratio(self) -> float:
        return self._params.losing_random_ratio

    @losing_random_ratio.setter
    def losing_random_ratio(self, value: float):
        self._params.losing_random_ratio = value

    # Meta-meta parameters
    @property
    def meta_mutation_rate(self) -> float:
        return self._params.meta_mutation_rate

    @meta_mutation_rate.setter
    def meta_mutation_rate(self, value: float):
        self._params.meta_mutation_rate = value

    @property
    def meta_learning_rate(self) -> float:
        return self._params.meta_learning_rate

    @meta_learning_rate.setter
    def meta_learning_rate(self, value: float):
        self._params.meta_learning_rate = value

    @property
    def meta_convergence_threshold(self) -> float:
        return self._params.meta_convergence_threshold

    @meta_convergence_threshold.setter
    def meta_convergence_threshold(self, value: float):
        self._params.meta_convergence_threshold = value

    # Negative learning
    @property
    def negative_learning_enabled(self) -> bool:
        return self._params.negative_learning_enabled

    @negative_learning_enabled.setter
    def negative_learning_enabled(self, value: bool):
        self._params.negative_learning_enabled = value

    @property
    def negative_penalty_weight(self) -> float:
        return self._params.negative_penalty_weight

    @negative_penalty_weight.setter
    def negative_penalty_weight(self, value: float):
        self._params.negative_penalty_weight = value

    @property
    def negative_decay_rate(self) -> float:
        return self._params.negative_decay_rate

    @negative_decay_rate.setter
    def negative_decay_rate(self, value: float):
        self._params.negative_decay_rate = value

    @property
    def negative_capacity(self) -> int:
        return self._params.negative_capacity

    @negative_capacity.setter
    def negative_capacity(self, value: int):
        self._params.negative_capacity = value

    @property
    def negative_similarity_threshold(self) -> float:
        return self._params.negative_similarity_threshold

    @negative_similarity_threshold.setter
    def negative_similarity_threshold(self, value: float):
        self._params.negative_similarity_threshold = value

    @property
    def negative_forbidden_threshold(self) -> float:
        return self._params.negative_forbidden_threshold

    @negative_forbidden_threshold.setter
    def negative_forbidden_threshold(self, value: float):
        self._params.negative_forbidden_threshold = value

    def __repr__(self) -> str:
        return (f"MetaParams(opt_mut={self.optimization_mutation_rate:.3f}, "
                f"explore={self.exploration_factor:.3f}, "
                f"pop_size={self.target_population_size})")


__all__ = ['MetaParams']
