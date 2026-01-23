"""
Parameter synthesis for evocore Python bindings.

Provides parameter synthesis from multiple sources.
"""

from typing import Optional, List, Tuple
from enum import IntEnum
import numpy as np
from ..utils.error import check_error, EvocoreError


class SynthesisStrategy(IntEnum):
    """Synthesis strategy types."""
    AVERAGE = 0
    WEIGHTED = 1
    TREND = 2
    REGIME = 3
    ENSEMBLE = 4
    NEAREST = 5


class SynthesisRequest:
    """
    Request for parameter synthesis from multiple sources.

    Combines parameters from different contexts/sources using
    various strategies.

    Example:
        >>> req = SynthesisRequest(SynthesisStrategy.WEIGHTED, param_count=5, source_count=3)
        >>> req.add_source(0, params1, confidence=0.8, fitness=0.75)
        >>> req.add_source(1, params2, confidence=0.6, fitness=0.70)
        >>> result, confidence = req.execute()
    """

    __slots__ = ('_request', '_ffi', '_lib')

    def __init__(self, strategy: SynthesisStrategy, param_count: int,
                 source_count: int, *, _raw: bool = False):
        """
        Initialize synthesis request.

        Args:
            strategy: Synthesis strategy to use
            param_count: Number of parameters
            source_count: Number of parameter sources
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._request = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        self._request = lib.evocore_synthesis_request_create(
            int(strategy), param_count, source_count
        )
        if self._request == ffi.NULL:
            raise EvocoreError("Failed to create synthesis request")

    def __del__(self):
        """Clean up synthesis request."""
        if hasattr(self, '_request') and self._request is not None:
            self._lib.evocore_synthesis_request_free(self._request)

    @property
    def strategy(self) -> SynthesisStrategy:
        """Synthesis strategy."""
        return SynthesisStrategy(self._request.strategy)

    @property
    def param_count(self) -> int:
        """Number of parameters."""
        return self._request.target_param_count

    @property
    def source_count(self) -> int:
        """Number of sources."""
        return self._request.source_count

    @property
    def exploration_factor(self) -> float:
        """Exploration factor."""
        return self._request.exploration_factor

    @exploration_factor.setter
    def exploration_factor(self, value: float):
        self._request.exploration_factor = value

    @property
    def trend_strength(self) -> float:
        """Trend strength for TREND strategy."""
        return self._request.trend_strength

    @trend_strength.setter
    def trend_strength(self, value: float):
        self._request.trend_strength = value

    def add_source(self, index: int, parameters: np.ndarray,
                   confidence: float = 1.0, fitness: float = 0.0,
                   context_id: Optional[str] = None) -> bool:
        """
        Add a parameter source.

        Args:
            index: Source index
            parameters: Parameter values
            confidence: Confidence in these parameters
            fitness: Fitness associated with parameters
            context_id: Optional context identifier

        Returns:
            True if successful
        """
        params = np.asarray(parameters, dtype=np.float64)
        params_arr = self._ffi.new("double[]", params.tolist())

        ctx_id = context_id.encode() if context_id else self._ffi.NULL

        return self._lib.evocore_synthesis_add_source(
            self._request, index, params_arr, confidence, fitness, ctx_id
        )

    def execute(self, seed: Optional[int] = None) -> Tuple[np.ndarray, float]:
        """
        Execute synthesis.

        Args:
            seed: Optional random seed

        Returns:
            Tuple of (synthesized_parameters, confidence)
        """
        out_params = self._ffi.new("double[]", self.param_count)
        out_confidence = self._ffi.new("double *")
        seed_ptr = self._ffi.new("unsigned int *", seed if seed is not None else 0)

        success = self._lib.evocore_synthesis_execute(
            self._request, out_params, out_confidence, seed_ptr
        )

        if not success:
            raise EvocoreError("Synthesis execution failed")

        params = np.array([out_params[i] for i in range(self.param_count)])
        return params, out_confidence[0]

    def validate(self) -> bool:
        """
        Validate the synthesis request.

        Returns:
            True if valid
        """
        return self._lib.evocore_synthesis_validate(self._request)

    def __repr__(self) -> str:
        return (f"SynthesisRequest(strategy={self.strategy.name}, "
                f"params={self.param_count}, sources={self.source_count})")


class SimilarityMatrix:
    """
    Matrix of context similarities.

    Tracks pairwise similarity between contexts for transfer learning.
    """

    __slots__ = ('_matrix', '_ffi', '_lib', '_context_ids')

    def __init__(self, context_ids: List[str], *, _raw: bool = False):
        """
        Initialize similarity matrix.

        Args:
            context_ids: List of context identifiers
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._matrix = None
            self._ffi = None
            self._lib = None
            self._context_ids = []
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib
        self._context_ids = context_ids.copy()

        # Build context ID array
        ctx_array = ffi.new("char*[]", len(context_ids))
        ctx_bufs = []
        for i, ctx_id in enumerate(context_ids):
            buf = ffi.new("char[]", ctx_id.encode())
            ctx_bufs.append(buf)
            ctx_array[i] = buf

        self._ctx_bufs = ctx_bufs  # Keep reference
        self._matrix = lib.evocore_similarity_matrix_create(len(context_ids), ctx_array)

        if self._matrix == ffi.NULL:
            raise EvocoreError("Failed to create similarity matrix")

    def __del__(self):
        """Clean up similarity matrix."""
        if hasattr(self, '_matrix') and self._matrix is not None:
            self._lib.evocore_similarity_matrix_free(self._matrix)

    @property
    def context_count(self) -> int:
        """Number of contexts."""
        return len(self._context_ids)

    def update(self, context_a: str, context_b: str, similarity: float) -> bool:
        """
        Update similarity between two contexts.

        Args:
            context_a: First context
            context_b: Second context
            similarity: Similarity value (0.0 to 1.0)

        Returns:
            True if successful
        """
        return self._lib.evocore_similarity_update(
            self._matrix, context_a.encode(), context_b.encode(), similarity
        )

    def get(self, context_a: str, context_b: str) -> float:
        """
        Get similarity between two contexts.

        Args:
            context_a: First context
            context_b: Second context

        Returns:
            Similarity value
        """
        return self._lib.evocore_similarity_get(
            self._matrix, context_a.encode(), context_b.encode()
        )

    def find_nearest(self, target_context: str) -> Optional[str]:
        """
        Find the most similar context.

        Args:
            target_context: Context to find similar to

        Returns:
            Most similar context ID or None
        """
        result = self._lib.evocore_similarity_find_nearest(
            self._matrix, target_context.encode()
        )
        if result == self._ffi.NULL:
            return None
        return self._ffi.string(result).decode()

    def __repr__(self) -> str:
        return f"SimilarityMatrix(contexts={self.context_count})"


def param_distance(params1: np.ndarray, params2: np.ndarray) -> float:
    """
    Compute Euclidean distance between parameter sets.

    Args:
        params1: First parameter set
        params2: Second parameter set

    Returns:
        Distance value
    """
    from .._evocore import ffi, lib

    p1 = np.asarray(params1, dtype=np.float64)
    p2 = np.asarray(params2, dtype=np.float64)

    if len(p1) != len(p2):
        raise ValueError("Parameter arrays must have same length")

    arr1 = ffi.new("double[]", p1.tolist())
    arr2 = ffi.new("double[]", p2.tolist())

    return lib.evocore_param_distance(arr1, arr2, len(p1))


def param_similarity(params1: np.ndarray, params2: np.ndarray,
                     max_distance: float = 1.0) -> float:
    """
    Compute similarity between parameter sets.

    Args:
        params1: First parameter set
        params2: Second parameter set
        max_distance: Maximum distance for normalization

    Returns:
        Similarity value (0.0 to 1.0)
    """
    from .._evocore import ffi, lib

    p1 = np.asarray(params1, dtype=np.float64)
    p2 = np.asarray(params2, dtype=np.float64)

    if len(p1) != len(p2):
        raise ValueError("Parameter arrays must have same length")

    arr1 = ffi.new("double[]", p1.tolist())
    arr2 = ffi.new("double[]", p2.tolist())

    return lib.evocore_param_similarity(arr1, arr2, len(p1), max_distance)


def transfer_params(source_params: np.ndarray, source_context: str,
                    target_context: str, similarity_matrix: SimilarityMatrix,
                    adjustment_factor: float = 0.5) -> Optional[np.ndarray]:
    """
    Transfer parameters from one context to another.

    Args:
        source_params: Parameters from source context
        source_context: Source context ID
        target_context: Target context ID
        similarity_matrix: Similarity matrix
        adjustment_factor: How much to adjust based on similarity

    Returns:
        Transferred parameters or None if failed
    """
    from .._evocore import ffi, lib

    params = np.asarray(source_params, dtype=np.float64)
    params_arr = ffi.new("double[]", params.tolist())
    out_params = ffi.new("double[]", len(params))

    success = lib.evocore_transfer_params(
        params_arr, source_context.encode(), target_context.encode(),
        similarity_matrix._matrix, len(params), out_params, adjustment_factor
    )

    if not success:
        return None

    return np.array([out_params[i] for i in range(len(params))])


def synthesis_strategy_name(strategy: SynthesisStrategy) -> str:
    """
    Get name of synthesis strategy.

    Args:
        strategy: Synthesis strategy

    Returns:
        Strategy name
    """
    from .._evocore import ffi, lib
    ptr = lib.evocore_synthesis_strategy_name(int(strategy))
    if ptr == ffi.NULL:
        return "unknown"
    return ffi.string(ptr).decode()


__all__ = [
    'SynthesisStrategy',
    'SynthesisRequest',
    'SimilarityMatrix',
    'param_distance',
    'param_similarity',
    'transfer_params',
    'synthesis_strategy_name',
]
