"""
Configuration utilities for evocore Python bindings.

Provides configuration file loading and access.
"""

from typing import Optional, Dict, Any, List
from enum import IntEnum
from .error import check_error, EvocoreError


class ConfigType(IntEnum):
    """Configuration value types."""
    STRING = 0
    INT = 1
    DOUBLE = 2
    BOOL = 3


class ConfigEntry:
    """
    A configuration entry.

    Wraps evocore_config_entry_t.
    """

    __slots__ = ('key', 'value', 'type')

    def __init__(self, key: str, value: str, entry_type: ConfigType):
        self.key = key
        self.value = value
        self.type = entry_type

    def as_string(self) -> str:
        """Get value as string."""
        return self.value

    def as_int(self) -> int:
        """Get value as integer."""
        return int(self.value)

    def as_float(self) -> float:
        """Get value as float."""
        return float(self.value)

    def as_bool(self) -> bool:
        """Get value as boolean."""
        return self.value.lower() in ('true', '1', 'yes', 'on')

    def __repr__(self) -> str:
        return f"ConfigEntry(key='{self.key}', value='{self.value}', type={self.type.name})"


class Config:
    """
    Configuration file handler.

    Loads and provides access to INI-style configuration files.

    Example:
        >>> config = Config.load("config.ini")
        >>> value = config.get_string("section", "key", "default")
        >>> rate = config.get_double("evolution", "mutation_rate", 0.01)
    """

    __slots__ = ('_config', '_ffi', '_lib')

    def __init__(self, *, _raw: bool = False):
        """
        Initialize configuration (use Config.load() instead).

        Args:
            _raw: Internal flag for alternative construction
        """
        if _raw:
            self._config = None
            self._ffi = None
            self._lib = None
            return

        self._config = None
        self._ffi = None
        self._lib = None

    def __del__(self):
        """Clean up configuration."""
        if hasattr(self, '_config') and self._config is not None:
            self._lib.evocore_config_free(self._config)

    @classmethod
    def load(cls, path: str) -> "Config":
        """
        Load configuration from file.

        Args:
            path: Path to configuration file

        Returns:
            Loaded Config object
        """
        from .._evocore import ffi, lib

        config_ptr = ffi.new("evocore_config_t **")
        err = lib.evocore_config_load(path.encode(), config_ptr)
        check_error(err, lib)

        obj = cls(_raw=True)
        obj._config = config_ptr[0]
        obj._ffi = ffi
        obj._lib = lib

        return obj

    def get_string(self, section: str, key: str, default: str = "") -> str:
        """
        Get string value.

        Args:
            section: Configuration section
            key: Configuration key
            default: Default value if not found

        Returns:
            String value
        """
        result = self._lib.evocore_config_get_string(
            self._config, section.encode(), key.encode(), default.encode()
        )
        if result == self._ffi.NULL:
            return default
        return self._ffi.string(result).decode()

    def get_int(self, section: str, key: str, default: int = 0) -> int:
        """
        Get integer value.

        Args:
            section: Configuration section
            key: Configuration key
            default: Default value if not found

        Returns:
            Integer value
        """
        return self._lib.evocore_config_get_int(
            self._config, section.encode(), key.encode(), default
        )

    def get_double(self, section: str, key: str, default: float = 0.0) -> float:
        """
        Get double value.

        Args:
            section: Configuration section
            key: Configuration key
            default: Default value if not found

        Returns:
            Float value
        """
        return self._lib.evocore_config_get_double(
            self._config, section.encode(), key.encode(), default
        )

    def get_bool(self, section: str, key: str, default: bool = False) -> bool:
        """
        Get boolean value.

        Args:
            section: Configuration section
            key: Configuration key
            default: Default value if not found

        Returns:
            Boolean value
        """
        return self._lib.evocore_config_get_bool(
            self._config, section.encode(), key.encode(), default
        )

    def has_key(self, section: str, key: str) -> bool:
        """
        Check if key exists.

        Args:
            section: Configuration section
            key: Configuration key

        Returns:
            True if key exists
        """
        return self._lib.evocore_config_has_key(
            self._config, section.encode(), key.encode()
        )

    def section_size(self, section: str) -> int:
        """
        Get number of entries in a section.

        Args:
            section: Configuration section

        Returns:
            Number of entries
        """
        return self._lib.evocore_config_section_size(self._config, section.encode())

    def get_entry(self, section: str, index: int) -> Optional[ConfigEntry]:
        """
        Get entry by index in a section.

        Args:
            section: Configuration section
            index: Entry index

        Returns:
            ConfigEntry or None
        """
        entry_ptr = self._lib.evocore_config_get_entry(
            self._config, section.encode(), index
        )
        if entry_ptr == self._ffi.NULL:
            return None

        return ConfigEntry(
            key=self._ffi.string(entry_ptr.key).decode() if entry_ptr.key != self._ffi.NULL else "",
            value=self._ffi.string(entry_ptr.value).decode() if entry_ptr.value != self._ffi.NULL else "",
            entry_type=ConfigType(entry_ptr.type)
        )

    def get_section(self, section: str) -> Dict[str, str]:
        """
        Get all entries in a section as dictionary.

        Args:
            section: Configuration section

        Returns:
            Dictionary of key-value pairs
        """
        result = {}
        size = self.section_size(section)
        for i in range(size):
            entry = self.get_entry(section, i)
            if entry:
                result[entry.key] = entry.value
        return result

    def __repr__(self) -> str:
        return f"Config(loaded={self._config is not None})"


__all__ = ['ConfigType', 'ConfigEntry', 'Config']
