#!/usr/bin/env bash
# Build and run CUE parser + ISO reader tests (Linux/Mac).
#
# Usage:  ./run_cue_iso_tests.sh
#
# Builds from android/app/src/main/cpp/ directory.

set -e
cd "$(dirname "$0")/app/src/main/cpp"

echo "Building test_cue_iso..."
cc -DTEST_STANDALONE -I. -Wall -Wextra -o test_cue_iso \
    test_cue_iso.c cue_parser.c iso9660_reader.c

echo "Running tests..."
./test_cue_iso
rc=$?

# Clean up
rm -rf test_fixtures test_cue_iso
exit $rc
