"""
evocore - Python bindings for the evocore evolutionary computation library.

This package provides high-performance Python bindings for the evocore C library
using cffi. It offers a comprehensive API for evolutionary computation including:

- Core evolution: Genome, Population, Domain, Fitness
- Learning systems: Context learning, Temporal learning, Negative learning
- Meta-evolution: Parameter adaptation and meta-population management
- Strategy: Exploration control, Bandit algorithms, Parameter synthesis
- Utilities: Configuration, Checkpointing, Statistics, Logging

Example:
    >>> from evocore import Genome, Population, ContextSystem
    >>>
    >>> # Create a population
    >>> pop = Population(capacity=100)
    >>> for _ in range(50):
    ...     genome = Genome(1024)
    ...     genome.randomize()
    ...     pop.add(genome, fitness=0.0)
    >>>
    >>> # Context-aware learning
    >>> ctx = ContextSystem([("asset", ["BTC", "ETH"])], param_count=5)
    >>> ctx.learn(["BTC"], [0.1, 0.2, 0.3, 0.4, 0.5], fitness=0.75)
    >>> params = ctx.sample(["BTC"], exploration=0.3)
"""

__version__ = "1.0.0"
__author__ = "Aion Project"

# Core imports
from .core import (
    Genome,
    Individual,
    Population,
    GenomeOps,
    Domain,
    DomainRegistry,
    FitnessFunc,
    FitnessEvaluator,
    make_fitness_func,
    combine_fitness,
    normalize_fitness,
)

# Learning imports
from .learning import (
    ContextStats,
    ContextSystem,
    BucketType,
    TemporalBucket,
    TemporalSystem,
    WeightedStats,
    WeightedArray,
    FailureSeverity,
    NegativeStats,
    NegativeLearning,
)

# Meta-evolution imports
from .meta import (
    MetaParams,
    MetaIndividual,
    MetaPopulation,
    meta_adapt,
    meta_evaluate,
)

# Strategy imports
from .strategy import (
    ExploreStrategy,
    Exploration,
    Bandit,
    SynthesisStrategy,
    SynthesisRequest,
    SimilarityMatrix,
)

# Utils imports
from .utils import (
    EvocoreError,
    Config,
    Checkpoint,
    CheckpointManager,
    Stats,
    StatsConfig,
    LogLevel,
    set_level as log_set_level,
)

__all__ = [
    # Version
    '__version__',

    # Core
    'Genome',
    'Individual',
    'Population',
    'GenomeOps',
    'Domain',
    'DomainRegistry',
    'FitnessFunc',
    'FitnessEvaluator',
    'make_fitness_func',
    'combine_fitness',
    'normalize_fitness',

    # Learning
    'ContextStats',
    'ContextSystem',
    'BucketType',
    'TemporalBucket',
    'TemporalSystem',
    'WeightedStats',
    'WeightedArray',
    'FailureSeverity',
    'NegativeStats',
    'NegativeLearning',

    # Meta
    'MetaParams',
    'MetaIndividual',
    'MetaPopulation',
    'meta_adapt',
    'meta_evaluate',

    # Strategy
    'ExploreStrategy',
    'Exploration',
    'Bandit',
    'SynthesisStrategy',
    'SynthesisRequest',
    'SimilarityMatrix',

    # Utils
    'EvocoreError',
    'Config',
    'Checkpoint',
    'CheckpointManager',
    'Stats',
    'StatsConfig',
    'LogLevel',
    'log_set_level',
]
