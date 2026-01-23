"""
Strategy module for evocore.

Provides exploration control and parameter synthesis:
- Exploration: Exploration rate scheduling
- Bandit: Multi-armed bandit for action selection
- Synthesis: Parameter synthesis from multiple sources
"""

from .exploration import (
    ExploreStrategy,
    Exploration,
    Bandit,
    boltzmann_select,
    cool_temperature,
)
from .synthesis import (
    SynthesisStrategy,
    SynthesisRequest,
    SimilarityMatrix,
    param_distance,
    param_similarity,
    transfer_params,
    synthesis_strategy_name,
)

__all__ = [
    # Exploration
    'ExploreStrategy',
    'Exploration',
    'Bandit',
    'boltzmann_select',
    'cool_temperature',
    # Synthesis
    'SynthesisStrategy',
    'SynthesisRequest',
    'SimilarityMatrix',
    'param_distance',
    'param_similarity',
    'transfer_params',
    'synthesis_strategy_name',
]
