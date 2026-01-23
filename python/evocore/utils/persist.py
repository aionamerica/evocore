"""
Persistence utilities for evocore Python bindings.

Provides checkpointing and serialization.
"""

from typing import Optional, List, Dict, Any
from enum import IntEnum
from datetime import datetime
from ..core.genome import Genome
from ..core.population import Population
from ..meta.params import MetaParams
from ..meta.population import MetaPopulation
from .error import check_error, EvocoreError


class SerialFormat(IntEnum):
    """Serialization formats."""
    JSON = 0
    BINARY = 1
    MSGPACK = 2


class SerialOptions:
    """
    Serialization options.

    Example:
        >>> opts = SerialOptions(format=SerialFormat.JSON, pretty_print=True)
    """

    __slots__ = ('format', 'include_metadata', 'pretty_print', 'compression_level')

    def __init__(self, format: SerialFormat = SerialFormat.JSON,
                 include_metadata: bool = True,
                 pretty_print: bool = False,
                 compression_level: int = 0):
        """
        Initialize serialization options.

        Args:
            format: Serialization format
            include_metadata: Include metadata in output
            pretty_print: Pretty print JSON output
            compression_level: Compression level (0=none)
        """
        self.format = format
        self.include_metadata = include_metadata
        self.pretty_print = pretty_print
        self.compression_level = compression_level

    def _to_c(self, ffi):
        """Convert to C struct."""
        opts = ffi.new("evocore_serial_options_t *")
        opts.format = int(self.format)
        opts.include_metadata = self.include_metadata
        opts.pretty_print = self.pretty_print
        opts.compression_level = self.compression_level
        return opts


class Checkpoint:
    """
    Evolution checkpoint for saving/restoring state.

    Example:
        >>> checkpoint = Checkpoint.create(population, meta_population)
        >>> checkpoint.save("checkpoint.json")
        >>> restored = Checkpoint.load("checkpoint.json")
    """

    __slots__ = ('_checkpoint', '_ffi', '_lib')

    def __init__(self, *, _raw: bool = False):
        if _raw:
            self._checkpoint = None
            self._ffi = None
            self._lib = None
            return

        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib
        self._checkpoint = ffi.new("evocore_checkpoint_t *")

    def __del__(self):
        if hasattr(self, '_checkpoint') and self._checkpoint is not None:
            self._lib.evocore_checkpoint_free(self._checkpoint)

    @property
    def version(self) -> int:
        """Checkpoint version."""
        return self._checkpoint.version

    @property
    def timestamp(self) -> datetime:
        """Checkpoint creation time."""
        return datetime.fromtimestamp(self._checkpoint.timestamp)

    @property
    def population_size(self) -> int:
        """Population size at checkpoint."""
        return self._checkpoint.population_size

    @property
    def generation(self) -> int:
        """Generation at checkpoint."""
        return self._checkpoint.generation

    @property
    def best_fitness(self) -> float:
        """Best fitness at checkpoint."""
        return self._checkpoint.best_fitness

    @property
    def avg_fitness(self) -> float:
        """Average fitness at checkpoint."""
        return self._checkpoint.avg_fitness

    @classmethod
    def create(cls, population: Population, meta_pop: Optional[MetaPopulation] = None,
               domain: Any = None) -> "Checkpoint":
        """
        Create checkpoint from current state.

        Args:
            population: Population to checkpoint
            meta_pop: Optional meta-population
            domain: Optional domain

        Returns:
            New Checkpoint
        """
        from .._evocore import ffi, lib

        checkpoint = cls()

        domain_ptr = domain._domain if domain else ffi.NULL
        meta_ptr = meta_pop._meta_pop if meta_pop else ffi.NULL

        err = lib.evocore_checkpoint_create(
            checkpoint._checkpoint, population._pop, domain_ptr, meta_ptr
        )
        check_error(err, lib)

        return checkpoint

    def save(self, filepath: str, options: Optional[SerialOptions] = None) -> None:
        """
        Save checkpoint to file.

        Args:
            filepath: Path to save to
            options: Serialization options
        """
        opts = options._to_c(self._ffi) if options else self._ffi.NULL
        err = self._lib.evocore_checkpoint_save(
            self._checkpoint, filepath.encode(), opts
        )
        check_error(err, self._lib)

    @classmethod
    def load(cls, filepath: str) -> "Checkpoint":
        """
        Load checkpoint from file.

        Args:
            filepath: Path to load from

        Returns:
            Loaded Checkpoint
        """
        from .._evocore import ffi, lib

        checkpoint = cls()
        err = lib.evocore_checkpoint_load(filepath.encode(), checkpoint._checkpoint)
        check_error(err, lib)

        return checkpoint

    def restore(self, population: Population,
                meta_pop: Optional[MetaPopulation] = None,
                domain: Any = None) -> None:
        """
        Restore state from checkpoint.

        Args:
            population: Population to restore into
            meta_pop: Optional meta-population to restore
            domain: Optional domain
        """
        domain_ptr = domain._domain if domain else self._ffi.NULL
        meta_ptr = meta_pop._meta_pop if meta_pop else self._ffi.NULL

        err = self._lib.evocore_checkpoint_restore(
            self._checkpoint, population._pop, domain_ptr, meta_ptr
        )
        check_error(err, self._lib)

    def __repr__(self) -> str:
        return (f"Checkpoint(generation={self.generation}, "
                f"best_fitness={self.best_fitness:.4f})")


class CheckpointManager:
    """
    Automatic checkpoint management.

    Handles periodic checkpointing with configurable retention.

    Example:
        >>> manager = CheckpointManager(directory="checkpoints", every_n=10)
        >>> manager.update(population, generation=15)
    """

    __slots__ = ('_manager', '_ffi', '_lib', '_config')

    def __init__(self, directory: str = "checkpoints",
                 every_n: int = 10,
                 max_checkpoints: int = 5,
                 compress: bool = False,
                 enabled: bool = True):
        """
        Initialize checkpoint manager.

        Args:
            directory: Directory to save checkpoints
            every_n: Save every N generations
            max_checkpoints: Maximum checkpoints to keep
            compress: Enable compression
            enabled: Enable automatic checkpointing
        """
        from .._evocore import ffi, lib
        self._ffi = ffi
        self._lib = lib

        config = ffi.new("evocore_auto_checkpoint_config_t *")
        config.enabled = enabled
        config.every_n_generations = every_n
        config.directory = ffi.new("char[]", directory.encode())
        config.max_checkpoints = max_checkpoints
        config.compress = compress

        self._config = config
        self._manager = lib.evocore_checkpoint_manager_create(config)

        if self._manager == ffi.NULL:
            raise EvocoreError("Failed to create checkpoint manager")

    def __del__(self):
        if hasattr(self, '_manager') and self._manager is not None:
            self._lib.evocore_checkpoint_manager_destroy(self._manager)

    def update(self, population: Population,
               meta_pop: Optional[MetaPopulation] = None,
               domain: Any = None) -> None:
        """
        Update checkpoint manager (may trigger checkpoint save).

        Args:
            population: Current population
            meta_pop: Optional meta-population
            domain: Optional domain
        """
        domain_ptr = domain._domain if domain else self._ffi.NULL
        meta_ptr = meta_pop._meta_pop if meta_pop else self._ffi.NULL

        err = self._lib.evocore_checkpoint_manager_update(
            self._manager, population._pop, domain_ptr, meta_ptr
        )
        check_error(err, self._lib)


def list_checkpoints(directory: str) -> List[str]:
    """
    List checkpoint files in directory.

    Args:
        directory: Directory to list

    Returns:
        List of checkpoint file paths
    """
    from .._evocore import ffi, lib

    count = ffi.new("int *")
    result = lib.evocore_checkpoint_list(directory.encode(), count)

    if result == ffi.NULL:
        return []

    checkpoints = []
    for i in range(count[0]):
        if result[i] != ffi.NULL:
            checkpoints.append(ffi.string(result[i]).decode())

    lib.evocore_checkpoint_list_free(result, count[0])
    return checkpoints


def checkpoint_info(filepath: str) -> Dict[str, Any]:
    """
    Get information about a checkpoint file.

    Args:
        filepath: Path to checkpoint

    Returns:
        Dictionary with checkpoint info
    """
    checkpoint = Checkpoint.load(filepath)
    return {
        'version': checkpoint.version,
        'timestamp': checkpoint.timestamp,
        'generation': checkpoint.generation,
        'population_size': checkpoint.population_size,
        'best_fitness': checkpoint.best_fitness,
        'avg_fitness': checkpoint.avg_fitness,
    }


def checksum(data: bytes) -> int:
    """
    Compute checksum of data.

    Args:
        data: Data to checksum

    Returns:
        Checksum value
    """
    from .._evocore import lib
    return lib.evocore_checksum(data, len(data))


def checksum_validate(data: bytes, expected: int) -> bool:
    """
    Validate checksum of data.

    Args:
        data: Data to validate
        expected: Expected checksum

    Returns:
        True if checksum matches
    """
    from .._evocore import lib
    return lib.evocore_checksum_validate(data, len(data), expected)


__all__ = [
    'SerialFormat',
    'SerialOptions',
    'Checkpoint',
    'CheckpointManager',
    'list_checkpoints',
    'checkpoint_info',
    'checksum',
    'checksum_validate',
]
