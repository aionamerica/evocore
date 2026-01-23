"""
Utilities module for evocore.

Provides infrastructure:
- Config: Configuration file handling
- Persist: Checkpointing and serialization
- Stats: Statistics tracking
- Log: Logging control
- Error: Exception classes
"""

from .config import ConfigType, ConfigEntry, Config
from .persist import (
    SerialFormat,
    SerialOptions,
    Checkpoint,
    CheckpointManager,
    list_checkpoints,
    checkpoint_info,
    checksum,
    checksum_validate,
)
from .stats import (
    StatsConfig,
    Stats,
    ProgressReporter,
    print_progress,
    compute_diversity,
    fitness_distribution,
)
from .log import LogLevel, set_level, get_level, set_file, set_color, close
from .error import (
    EvocoreError,
    EvocoreNullPointerError,
    EvocoreOutOfMemoryError,
    EvocoreInvalidArgumentError,
    EvocoreGenomeError,
    EvocorePopulationError,
    EvocoreConfigError,
    EvocoreFileError,
    check_error,
    check_bool,
)

__all__ = [
    # Config
    'ConfigType',
    'ConfigEntry',
    'Config',
    # Persist
    'SerialFormat',
    'SerialOptions',
    'Checkpoint',
    'CheckpointManager',
    'list_checkpoints',
    'checkpoint_info',
    'checksum',
    'checksum_validate',
    # Stats
    'StatsConfig',
    'Stats',
    'ProgressReporter',
    'print_progress',
    'compute_diversity',
    'fitness_distribution',
    # Log
    'LogLevel',
    'set_level',
    'get_level',
    'set_file',
    'set_color',
    'close',
    # Error
    'EvocoreError',
    'EvocoreNullPointerError',
    'EvocoreOutOfMemoryError',
    'EvocoreInvalidArgumentError',
    'EvocoreGenomeError',
    'EvocorePopulationError',
    'EvocoreConfigError',
    'EvocoreFileError',
    'check_error',
    'check_bool',
]
