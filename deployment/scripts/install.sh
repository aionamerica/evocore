#!/bin/bash
# evocore Installation Script
# Installs evocore library and headers to the system

set -e

VERSION="0.4.0"
PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../../.. && pwd)"

echo "=== evocore v${VERSION} Installation ==="
echo "Install prefix: ${PREFIX}"
echo "Build directory: ${BUILD_DIR}"

# Detect OS
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
else
    PLATFORM="unknown"
    echo "Warning: Unknown platform $OSTYPE"
fi

# Check if running as root
if [[ $EUID -ne 0 ]]; then
    echo "Warning: Not running as root. Installation may require sudo."
    echo "Re-running with sudo..."
    exec sudo "$0" "$@"
fi

# Create directories
echo "Creating directories..."
mkdir -p "${PREFIX}/include/evocore"
mkdir -p "${PREFIX}/lib"
mkdir -p "${PREFIX}/bin"
mkdir -p "${PREFIX}/share/doc/evocore"
mkdir -p "/var/log/evocore"
mkdir -p "/var/lib/evocore/checkpoints"

# Build if not already built
if [[ ! -f "${BUILD_DIR}/build/libevocore.a" ]]; then
    echo "Building evocore..."
    cd "${BUILD_DIR}"
    make
    cd -
fi

# Install library
echo "Installing library..."
cp "${BUILD_DIR}/build/libevocore.a" "${PREFIX}/lib/"

# Install headers
echo "Installing headers..."
cp "${BUILD_DIR}/include/evocore/"*.h "${PREFIX}/include/evocore/"

# Update library cache if on Linux
if [[ "$PLATFORM" == "linux" ]]; then
    echo "Updating library cache..."
    ldconfig
fi

# Create pkg-config file
cat > "${PREFIX}/lib/pkgconfig/evocore.pc" << EOF
prefix=${PREFIX}
exec_prefix=\${prefix}
libdir=\${exec_prefix}/lib
includedir=\${prefix}/include

Name: evocore
Description: Meta-Evolutionary Framework
Version: ${VERSION}
Libs: -L\${libdir} -levocore -lm -lpthread -lrt
Cflags: -I\${includedir}
EOF

echo ""
echo "=== Installation Complete ==="
echo "Library: ${PREFIX}/lib/libevocore.a"
echo "Headers: ${PREFIX}/include/evocore/"
echo "Pkg-config: ${PREFIX}/lib/pkgconfig/evocore.pc"
echo ""
echo "To use evocore in your project:"
echo "  pkg-config --cflags --libs evocore"
echo ""
