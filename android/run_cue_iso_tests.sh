#!/usr/bin/env bash
# Build and run CUE parser + ISO reader tests (Linux/Mac).
#
# Usage:  ./run_cue_iso_tests.sh
#
# Uses CMake to build test executables in android/tests/build/.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SRC_DIR="$SCRIPT_DIR/app/src/main/cpp/extract"
BUILD_DIR="$SCRIPT_DIR/tests/build"

# Configure + build
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring cmake..."
    cmake -S "$SRC_DIR" -B "$BUILD_DIR"
fi
echo "Building..."
cmake --build "$BUILD_DIR"

# Run tests via CTest
echo ""
echo "Running tests..."
cd "$BUILD_DIR"
ctest --output-on-failure
rc=$?

# Clean up test fixtures
rm -rf "$SRC_DIR/test_fixtures"
exit $rc
