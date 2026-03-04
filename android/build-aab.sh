#!/bin/bash
# build-aab.sh — Build an AAB with all ABIs and copy to build-outputs/
# Usage: ./build-aab.sh [--release] [--clean]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Prompt for build type
echo ""
echo "Select build type:"
echo "  1) Debug"
echo "  2) Release (signed, for Play Console)"
echo ""
printf "Enter choice [1]: "
read -r CHOICE
if [ "$CHOICE" = "2" ]; then
    VARIANT="release"
else
    VARIANT="debug"
fi

# Source environment variables
source "$SCRIPT_DIR/set_vars.sh"

TASK="bundle$(echo "$VARIANT" | sed 's/.*/\u&/')"

echo ""
echo "Building AAB ($VARIANT) for armeabi-v7a, arm64-v8a, x86_64..."
echo ""
./gradlew "$TASK"

# Find the AAB
AAB_DIR="app/build/outputs/bundle/$VARIANT"
AAB=$(find "$AAB_DIR" -name '*.aab' -print -quit 2>/dev/null)
if [ -z "$AAB" ]; then
    echo "ERROR: AAB not found in $AAB_DIR" >&2
    exit 1
fi

# Copy to build-outputs/ with timestamp
OUT_DIR="build-outputs"
mkdir -p "$OUT_DIR"

TIMESTAMP=$(date +"%Y%m%d-%H%M%S")
OUT_NAME="dxx-redux-${VARIANT}-${TIMESTAMP}.aab"
OUT_PATH="$OUT_DIR/$OUT_NAME"

cp "$AAB" "$OUT_PATH"
SIZE=$(du -h "$OUT_PATH" | cut -f1)

echo ""
echo "AAB built successfully: $OUT_PATH ($SIZE)"
echo ""
echo "Press any key to exit..."
read -r -n1 -s
