#!/bin/bash
# get_all.sh — One-shot: download JDK, SDK, NDK, emulator, then finalize and create AVD.
set -e

wait_for_key() {
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
}
trap wait_for_key EXIT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export GET_ALL_RUNNING=1   # tell sub-scripts to skip their own wait-for-key

echo "=== Step 1/6: JDK ==="
bash "$SCRIPT_DIR/get_jdk.sh"

echo ""
echo "=== Step 2/6: Android SDK ==="
bash "$SCRIPT_DIR/get_sdk.sh"

echo ""
echo "=== Step 3/6: Android NDK ==="
bash "$SCRIPT_DIR/get_ndk.sh"

echo ""
echo "=== Step 4/6: Finalize (licenses & platform packages) ==="
bash "$SCRIPT_DIR/finalize.sh"

echo ""
echo "=== Step 5/6: Emulator & system image ==="
bash "$SCRIPT_DIR/get_emulator.sh"

echo ""
echo "=== Step 6/6: Create AVD ==="
bash "$SCRIPT_DIR/create_avd.sh"

echo ""
echo "=== All dependencies installed. ==="
echo "Source ../set_vars.sh to set JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT."
echo "Run ../run_emulator.sh to build, launch emulator, and test."
