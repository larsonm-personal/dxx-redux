#!/usr/bin/env bash
# Compile the API 26 x86_64 SDL12 target without Gradle or SDK license acceptance
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
build_dir=""
jobs=4
while [ "$#" -gt 0 ]; do
    case "$1" in
    --build-dir | --jobs)
        [ "$#" -ge 2 ] && [ -n "$2" ] || {
            echo "Missing value for $1" >&2
            exit 2
        }
        if [ "$1" = --build-dir ]; then build_dir="$2"; else jobs="$2"; fi
        shift 2
        ;;
    --help)
        echo "Usage: run_android_native_smoke.sh --build-dir PATH [--jobs N]"
        exit 0
        ;;
    *)
        echo "Unknown argument: $1" >&2
        exit 2
        ;;
    esac
done
if [ -z "$build_dir" ] || [[ ! "$jobs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Provide --build-dir PATH and a positive --jobs count" >&2
    exit 2
fi
# The wrapper selects pinned tools and checks readiness before entering this child
exec bash "$SCRIPT_DIR/../get_deps/build_tools.sh" --run -- bash -euo pipefail -c '
    unset CPATH LIBRARY_PATH CMAKE_PREFIX_PATH CC CXX LDFLAGS
    cmake -S "$1/app/src/main/cpp" -B "$2" \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI=x86_64 -DANDROID_PLATFORM=android-26 -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$2" --target SDL12 --parallel "$3"
    echo "Android native SDL12 smoke build complete; APK and launcher behavior are not qualified"
' bash "$SCRIPT_DIR/.." "$build_dir" "$jobs"
