#!/bin/bash
# get_all.sh — One-shot: download JDK, SDK, NDK, then finalize (accept licenses, install platform packages).
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "=== Step 1/4: JDK ==="
bash "$SCRIPT_DIR/get_jdk.sh"

echo ""
echo "=== Step 2/4: Android SDK ==="
bash "$SCRIPT_DIR/get_sdk.sh"

echo ""
echo "=== Step 3/4: Android NDK ==="
bash "$SCRIPT_DIR/get_ndk.sh"

echo ""
echo "=== Step 4/4: Finalize (licenses & platform packages) ==="
bash "$SCRIPT_DIR/finalize.sh"

echo ""
echo "=== All dependencies installed. ==="
echo "Source ../set_vars.sh to set JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT."
