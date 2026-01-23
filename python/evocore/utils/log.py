"""
Logging utilities for evocore Python bindings.

Controls the evocore C library logging.
"""

from enum import IntEnum


class LogLevel(IntEnum):
    """Logging levels."""
    TRACE = 0
    DEBUG = 1
    INFO = 2
    WARN = 3
    ERROR = 4
    FATAL = 5


def set_level(level: LogLevel) -> None:
    """
    Set logging level.

    Args:
        level: Minimum level to log
    """
    from .._evocore import lib
    lib.evocore_log_set_level(int(level))


def get_level() -> LogLevel:
    """
    Get current logging level.

    Returns:
        Current log level
    """
    from .._evocore import lib
    return LogLevel(lib.evocore_log_get_level())


def set_file(path: str, enabled: bool = True) -> bool:
    """
    Enable logging to file.

    Args:
        path: Path to log file
        enabled: Enable or disable file logging

    Returns:
        True if successful
    """
    from .._evocore import lib
    return lib.evocore_log_set_file(enabled, path.encode())


def set_color(enabled: bool) -> None:
    """
    Enable/disable colored output.

    Args:
        enabled: Enable colors
    """
    from .._evocore import lib
    lib.evocore_log_set_color(enabled)


def close() -> None:
    """Close log file if open."""
    from .._evocore import lib
    lib.evocore_log_close()


__all__ = ['LogLevel', 'set_level', 'get_level', 'set_file', 'set_color', 'close']
