"""
Meta-evolution module for evocore.

Provides meta-level parameter adaptation:
- MetaParams: Meta-evolution parameters
- MetaPopulation: Population of parameter configurations
- Adaptation functions for self-tuning evolution
"""

from .params import MetaParams
from .population import (
    MetaIndividual,
    MetaPopulation,
    meta_adapt,
    meta_suggest_mutation_rate,
    meta_suggest_selection_pressure,
    meta_evaluate,
    meta_learn_outcome,
    meta_get_learned_params,
    meta_reset_learning,
)

__all__ = [
    'MetaParams',
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
