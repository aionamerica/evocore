#!/bin/bash
# evocore Uninstallation Script

set -e

PREFIX="${PREFIX:-/usr/local}"

echo "=== evocore Uninstallation ==="
echo "Install prefix: ${PREFIX}"

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "Re-running with sudo..."
    exec sudo "$0" "$@"
fi

# Remove installed files
echo "Removing files..."
rm -f "${PREFIX}/lib/libevocore.a"
rm -f "${PREFIX}/lib/pkgconfig/evocore.pc"
rm -rf "${PREFIX}/include/evocore"

# Update library cache if on Linux
if command -v ldconfig &> /dev/null; then
    echo "Updating library cache..."
    ldconfig
fi

echo "Uninstallation complete."
