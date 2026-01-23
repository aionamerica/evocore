"""
Error handling for evocore Python bindings.

Provides exception classes and error checking utilities.
"""

from typing import Optional


class EvocoreError(Exception):
    """Base exception for evocore errors."""

    def __init__(self, message: str, code: Optional[int] = None):
        super().__init__(message)
        self.code = code


class EvocoreNullPointerError(EvocoreError):
    """Null pointer error."""
    pass


class EvocoreOutOfMemoryError(EvocoreError):
    """Out of memory error."""
    pass


class EvocoreInvalidArgumentError(EvocoreError):
    """Invalid argument error."""
    pass


class EvocoreGenomeError(EvocoreError):
    """Genome-related error."""
    pass


class EvocorePopulationError(EvocoreError):
    """Population-related error."""
    pass


class EvocoreConfigError(EvocoreError):
    """Configuration error."""
    pass


class EvocoreFileError(EvocoreError):
    """File I/O error."""
    pass


# Error code to exception mapping
_ERROR_MAP = {
    -2: (EvocoreNullPointerError, "Null pointer"),
    -3: (EvocoreOutOfMemoryError, "Out of memory"),
    -4: (EvocoreInvalidArgumentError, "Invalid argument"),
    -5: (EvocoreError, "Not implemented"),
    -10: (EvocoreGenomeError, "Genome empty"),
    -11: (EvocoreGenomeError, "Genome too large"),
    -12: (EvocoreGenomeError, "Genome invalid"),
    -20: (EvocorePopulationError, "Population empty"),
    -21: (EvocorePopulationError, "Population full"),
    -22: (EvocorePopulationError, "Population size error"),
    -30: (EvocoreConfigError, "Config not found"),
    -31: (EvocoreConfigError, "Config parse error"),
    -32: (EvocoreConfigError, "Config invalid"),
    -40: (EvocoreFileError, "File not found"),
    -41: (EvocoreFileError, "File read error"),
    -42: (EvocoreFileError, "File write error"),
}


def check_error(code: int, lib=None) -> None:
    """
    Check error code and raise appropriate exception if error.

    Args:
        code: Error code from C function
        lib: Optional cffi lib object for error string lookup

    Raises:
        EvocoreError: If code indicates an error
    """
    # Success codes
    if code >= 0:
        return

    # Try to get error string from library
    message = None
    if lib is not None:
        try:
            from .._evocore import ffi
            msg_ptr = lib.evocore_error_string(code)
            if msg_ptr != ffi.NULL:
                message = ffi.string(msg_ptr).decode('utf-8')
        except Exception:
            pass

    # Look up in our error map
    if code in _ERROR_MAP:
        exc_class, default_msg = _ERROR_MAP[code]
        raise exc_class(message or default_msg, code)

    # Unknown error
    raise EvocoreError(message or f"Unknown error code: {code}", code)


def check_bool(result: bool, message: str = "Operation failed") -> None:
    """
    Check boolean result and raise exception if False.

    Args:
        result: Boolean result from C function
        message: Error message to use

    Raises:
        EvocoreError: If result is False
    """
    if not result:
        raise EvocoreError(message)


__all__ = [
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
