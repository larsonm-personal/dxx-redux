#!/bin/bash
# get_all.sh - One-shot: download JDK, SDK, NDK, Gradle wrapper, emulator, code quality tools, then finalize.
set -e

wait_for_key() {
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
}
trap wait_for_key EXIT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export GET_ALL_RUNNING=1 # tell sub-scripts to skip their own wait-for-key

echo "=== Step 1/9: JDK ==="
bash "$SCRIPT_DIR/get_jdk.sh"

echo ""
echo "=== Step 2/9: Android SDK ==="
bash "$SCRIPT_DIR/get_sdk.sh"

echo ""
echo "=== Step 3/9: Android NDK ==="
bash "$SCRIPT_DIR/get_ndk.sh"

# Export JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT so remaining steps
# (finalize, emulator, avd) inherit them without re-detecting.
source "$SCRIPT_DIR/../set_vars.sh"

echo ""
echo "=== Step 4/9: Gradle wrapper ==="
bash "$SCRIPT_DIR/get_gradle_wrapper.sh"

echo ""
echo "=== Step 5/9: Finalize (licenses & platform packages) ==="
bash "$SCRIPT_DIR/finalize.sh"

echo ""
echo "=== Step 6/9: Emulator & system image ==="
bash "$SCRIPT_DIR/get_emulator.sh"

echo ""
echo "=== Step 7/9: Create AVD ==="
bash "$SCRIPT_DIR/create_avd.sh"

echo ""
echo "=== Step 8/9: clang-format ==="
bash "$SCRIPT_DIR/get_clang_format.sh"

echo ""
echo "=== Step 9/9: ktlint ==="
bash "$SCRIPT_DIR/get_ktlint.sh"

echo ""
echo "=== Step 10/10: cmake-format / cmake-lint ==="
bash "$SCRIPT_DIR/get_cmake_format.sh"

echo ""
echo "=== All dependencies installed. ==="
echo "Source ../set_vars.sh to set JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT"
echo "Run ../run_emulator.sh to build, launch emulator, and test"
