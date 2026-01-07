#!/bin/bash
# evocore Health Check Script
# Verifies installation and runs basic tests

set -e

PREFIX="${PREFIX:-/usr/local}"
BUILD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"

echo "=== evocore Health Check ==="

# Check library
echo -n "Library... "
if [[ -f "${PREFIX}/lib/libevocore.a" ]]; then
    echo "OK ($(ls -lh "${PREFIX}/lib/libevocore.a" | awk '{print $5}'))"
else
    echo "MISSING"
    exit 1
fi

# Check headers
echo -n "Headers... "
if [[ -d "${PREFIX}/include/evocore" ]]; then
    COUNT=$(ls "${PREFIX}/include/evocore/"*.h 2>/dev/null | wc -l)
    echo "OK (${COUNT} headers)"
else
    echo "MISSING"
    exit 1
fi

# Check pkg-config
echo -n "Pkg-config... "
if [[ -f "${PREFIX}/lib/pkgconfig/evocore.pc" ]]; then
    echo "OK"
else
    echo "NOT INSTALLED"
fi

# Compile test program
echo -n "Compile test... "
TEST_C="/tmp/evocore_test_$$.c"
cat > "$TEST_C" << 'EOF'
#include "evocore/evocore.h"
#include <stdio.h>

int main(void) {
    printf("evocore version: %s\n", EVOCORE_VERSION_STRING);
    return 0;
}
EOF

if gcc "$TEST_C" -o "/tmp/evocore_test$$" -I"${PREFIX}/include" -L"${PREFIX}/lib" -levocore -lm -lpthread -lrt 2>/dev/null; then
    if "/tmp/evocore_test$$"; then
        rm -f "$TEST_C" "/tmp/evocore_test$$"
        echo "OK"
    else
        echo "FAIL (runtime)"
        rm -f "$TEST_C" "/tmp/evocore_test$$"
        exit 1
    fi
else
    echo "FAIL (compile)"
    rm -f "$TEST_C"
    exit 1
fi

# Run unit tests if available
if [[ -f "${BUILD_DIR}/build/test_genome" ]]; then
    echo -n "Unit tests... "
    if cd "${BUILD_DIR}" && make test > /tmp/evocore_test_output.log 2>&1; then
        echo "OK"
    else
        echo "FAIL (see /tmp/evocore_test_output.log)"
        cat /tmp/evocore_test_output.log
        exit 1
    fi
else
    echo "Unit tests... NOT BUILT"
fi

echo ""
echo "=== Health Check Passed ==="
