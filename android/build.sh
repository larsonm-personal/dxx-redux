#!/bin/bash
# build.sh - Source environment vars and run the Gradle build.
# Works under Git Bash, MSYS2, or WSL.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Set JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT
source "$SCRIPT_DIR/set_vars.sh"

echo ""

# Pass through any arguments (e.g. assembleRelease, clean)
TASK="${1:-assembleDebug}"
shift 2>/dev/null || true

echo "Running: ./gradlew $TASK $*"
./gradlew "$TASK" "$@"
echo ""
echo "Press any key to exit..."
read -r -n1 -s
