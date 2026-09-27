#!/bin/bash
# build.sh - Source environment vars and run the Gradle build.
# Works under Windows Git Bash and Linux Bash.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ANDROID_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ANDROID_DIR"

# Set JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT
source "$SCRIPT_DIR/set_vars.sh"

echo ""

# Pass through any arguments (e.g. assembleRelease, clean)
TASK="${1:-assembleDebug}"
shift 2>/dev/null || true

echo "Running: ./gradlew $TASK $*"
./gradlew "$TASK" "$@"
