"""
Learning systems module for evocore.

Provides adaptive parameter learning across different contexts and time:
- ContextSystem: Multi-dimensional context-aware learning
- TemporalSystem: Time-bucketed learning with trend detection
- WeightedStats: Weighted statistics tracking
- NegativeLearning: Failure avoidance through penalty tracking
"""

from .context import ContextStats, ContextSystem
from .temporal import BucketType, TemporalBucket, TemporalSystem
from .weighted import WeightedStats, WeightedArray
from .negative import (
    FailureSeverity,
    NegativeStats,
    NegativeLearning,
    severity_string,
    severity_from_string,
    classify_failure,
)

__all__ = [
    # Context learning
    'ContextStats',
    'ContextSystem',
    # Temporal learning
    'BucketType',
    'TemporalBucket',
    'TemporalSystem',
    # Weighted statistics
    'WeightedStats',
    'WeightedArray',
    # Negative learning
    'FailureSeverity',
    'NegativeStats',
    'NegativeLearning',
    'severity_string',
    'severity_from_string',
    'classify_failure',
]
