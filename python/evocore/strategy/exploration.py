"""
Exploration control for evocore Python bindings.

Provides exploration rate scheduling and multi-armed bandit algorithms.
"""

from typing import Optional, List, Tuple
from enum import IntEnum
import numpy as np
from ..utils.error import EvocoreError


class ExploreStrategy(IntEnum):
    """Exploration strategy types."""
    FIXED = 0
    DECAY = 1
    ADAPTIVE = 2
    UCB1 = 3
    BOLTZMANN = 4


class Exploration:
    """
    Exploration rate controller.

    Manages exploration/exploitation balance with various strategies.

    Example:
        >>> exp = Exploration(ExploreStrategy.ADAPTIVE, base_rate=0.5)
        >>> exp.set_bounds(0.1, 0.9)
        >>> rate = exp.update(generation=10, best_fitness=0.8)
    """

    __slots__ = ('_exp', '_ffi', '_lib')

    def __init__(self, strategy: ExploreStrategy, base_rate: float = 0.5,
                 *, _raw: bool = False):
        """
        Initialize exploration controller.

        Args:
            strategy: Exploration strategy to use
            base_rate: Initial exploration rate
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._exp = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        self._exp = lib.evocore_exploration_create(int(strategy), base_rate)
        if self._exp == ffi.NULL:
            raise EvocoreError("Failed to create exploration controller")

    def __del__(self):
        """Clean up exploration controller."""
        if hasattr(self, '_exp') and self._exp is not None:
            self._lib.evocore_exploration_free(self._exp)

    @property
    def strategy(self) -> ExploreStrategy:
        """Current exploration strategy."""
        return ExploreStrategy(self._exp.strategy)

    @property
    def current_rate(self) -> float:
        """Current exploration rate."""
        return self._lib.evocore_exploration_get_rate(self._exp)

    @property
    def base_rate(self) -> float:
        """Base exploration rate."""
        return self._exp.base_rate

    def set_bounds(self, min_rate: float, max_rate: float) -> None:
        """
        Set rate bounds.

        Args:
            min_rate: Minimum exploration rate
            max_rate: Maximum exploration rate
        """
        self._lib.evocore_exploration_set_bounds(self._exp, min_rate, max_rate)

    def set_decay_rate(self, decay_rate: float) -> None:
        """
        Set decay rate for DECAY strategy.

        Args:
            decay_rate: Decay factor per generation
        """
        self._lib.evocore_exploration_set_decay_rate(self._exp, decay_rate)

    def set_temperature(self, temperature: float, cooling_rate: float) -> None:
        """
        Set temperature parameters for BOLTZMANN strategy.

        Args:
            temperature: Initial temperature
            cooling_rate: Cooling rate per step
        """
        self._lib.evocore_exploration_set_temperature(self._exp, temperature, cooling_rate)

    def set_ucb_c(self, ucb_c: float) -> None:
        """
        Set UCB constant for UCB1 strategy.

        Args:
            ucb_c: UCB exploration constant
        """
        self._lib.evocore_exploration_set_ucb_c(self._exp, ucb_c)

    def update(self, generation: int, best_fitness: float) -> float:
        """
        Update exploration rate based on progress.

        Args:
            generation: Current generation
            best_fitness: Best fitness achieved

        Returns:
            New exploration rate
        """
        return self._lib.evocore_exploration_update(self._exp, generation, best_fitness)

    def should_explore(self, seed: Optional[int] = None) -> bool:
        """
        Randomly decide whether to explore.

        Args:
            seed: Optional random seed

        Returns:
            True if should explore
        """
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)
        return self._lib.evocore_exploration_should_explore(self._exp, seed_ptr)

    def is_stagnant(self, threshold: int = 10) -> bool:
        """
        Check if evolution is stagnant.

        Args:
            threshold: Generations without improvement

        Returns:
            True if stagnant
        """
        return self._lib.evocore_exploration_is_stagnant(self._exp, threshold)

    def boost(self, factor: float = 1.5) -> None:
        """
        Boost exploration rate temporarily.

        Args:
            factor: Multiplier for exploration rate
        """
        self._lib.evocore_exploration_boost(self._exp, factor)

    def improvement_rate(self) -> float:
        """
        Get improvement rate.

        Returns:
            Rate of improvement
        """
        return self._lib.evocore_exploration_improvement_rate(self._exp)

    def reset(self) -> None:
        """Reset to initial state."""
        self._lib.evocore_exploration_reset(self._exp)

    def __repr__(self) -> str:
        return (f"Exploration(strategy={self.strategy.name}, "
                f"rate={self.current_rate:.3f})")


class Bandit:
    """
    Multi-armed bandit for action selection.

    Uses UCB1 algorithm to balance exploration and exploitation.

    Example:
        >>> bandit = Bandit(arm_count=4, ucb_c=2.0)
        >>> arm = bandit.select()
        >>> bandit.update(arm, reward=0.8)
    """

    __slots__ = ('_bandit', '_ffi', '_lib')

    def __init__(self, arm_count: int, ucb_c: float = 2.0, *, _raw: bool = False):
        """
        Initialize multi-armed bandit.

        Args:
            arm_count: Number of arms
            ucb_c: UCB exploration constant
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._bandit = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        self._bandit = lib.evocore_bandit_create(arm_count, ucb_c)
        if self._bandit == ffi.NULL:
            raise EvocoreError("Failed to create bandit")

    def __del__(self):
        """Clean up bandit."""
        if hasattr(self, '_bandit') and self._bandit is not None:
            self._lib.evocore_bandit_free(self._bandit)

    @property
    def arm_count(self) -> int:
        """Number of arms."""
        return self._lib.evocore_bandit_arm_count(self._bandit)

    @property
    def total_pulls(self) -> int:
        """Total number of arm pulls."""
        return self._bandit.total_pulls

    def select(self) -> int:
        """
        Select an arm using UCB1.

        Returns:
            Selected arm index
        """
        return self._lib.evocore_bandit_select_ucb(self._bandit)

    def update(self, arm: int, reward: float) -> None:
        """
        Update arm statistics after observing reward.

        Args:
            arm: Arm index
            reward: Observed reward
        """
        self._lib.evocore_bandit_update(self._bandit, arm, reward)

    def get_stats(self, arm: int) -> Tuple[int, float]:
        """
        Get statistics for an arm.

        Args:
            arm: Arm index

        Returns:
            Tuple of (pull_count, mean_reward)
        """
        count = self._ffi.new("size_t *")
        mean = self._ffi.new("double *")

        success = self._lib.evocore_bandit_get_stats(self._bandit, arm, count, mean)

        if not success:
            return (0, 0.0)

        return (count[0], mean[0])

    def reset(self) -> None:
        """Reset all arm statistics."""
        self._lib.evocore_bandit_reset(self._bandit)

    def __repr__(self) -> str:
        return f"Bandit(arms={self.arm_count}, pulls={self.total_pulls})"


def boltzmann_select(values: np.ndarray, temperature: float,
                     seed: Optional[int] = None) -> int:
    """
    Select index using Boltzmann distribution.

    Args:
        values: Array of values to select from
        temperature: Temperature parameter
        seed: Optional random seed

    Returns:
        Selected index
    """
    from .._evocore import ffi, lib

    values = np.asarray(values, dtype=np.float64)
    values_arr = ffi.new("double[]", values.tolist())
    seed_ptr = ffi.new("unsigned int *", seed if seed is not None else 0)

    return lib.evocore_boltzmann_select(values_arr, len(values), temperature, seed_ptr)


def cool_temperature(temperature: float, cooling_rate: float) -> float:
    """
    Apply temperature cooling.

    Args:
        temperature: Current temperature
        cooling_rate: Cooling rate

    Returns:
        New temperature
    """
    from .._evocore import lib
    return lib.evocore_cool_temperature(temperature, cooling_rate)


__all__ = [
    'ExploreStrategy',
    'Exploration',
    'Bandit',
    'boltzmann_select',
    'cool_temperature',
]
