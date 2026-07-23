#!/usr/bin/env bash
set -euo pipefail

TARGET="both"
BUILD_TYPE="RelWithDebInfo"
CLEAN=0
JOBS=""
GENERATOR=""
CMAKE_PATH=""
NINJA_PATH=""
LIST_TOOLS=0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
cd "$REPO_ROOT"

usage() {
    cat <<'EOF'
Usage: ./run-linux-build.sh [options]

Options:
  --target {both|d1|d2}   Select which game to build [default: both]
  --build-type TYPE       CMake build type [default: RelWithDebInfo]
  --clean                 Delete the target build directory before configuring
  --jobs N                Parallel build job count
  --generator NAME        Override the CMake generator
  --cmake PATH            Use an explicit cmake executable
  --ninja PATH            Use an explicit ninja executable
  --list-tools            Print resolved tool paths and exit
  -h, --help              Show this help
EOF
}

read_dependency_base() {
    local dep_base_file="$REPO_ROOT/dependency_base.txt"
    if [[ ! -f "$dep_base_file" ]]; then
        return 1
    fi

    local first_line
    first_line="$(head -n 1 "$dep_base_file" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    if [[ -z "$first_line" ]]; then
        return 1
    fi

    printf '%s\n' "$first_line"
}

find_executable() {
    local name="$1"
    shift

    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
        return 0
    fi

    local candidate
    for candidate in "$@"; do
        if [[ -n "$candidate" && -x "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

get_android_sdk_cmake_bin_dirs() {
    local dep_base="$1"
    local sdk_cmake_root="$dep_base/android-sdk/cmake"

    if [[ ! -d "$sdk_cmake_root" ]]; then
        return 0
    fi

    find "$sdk_cmake_root" -mindepth 2 -maxdepth 2 -type d -name bin | sort -r
}

show_linux_hint() {
    if [[ -f /etc/os-release ]]; then
        # shellcheck disable=SC1091
        source /etc/os-release
        if [[ "${ID:-}" == "ubuntu" || "${ID:-}" == "debian" || " ${ID_LIKE:-} " == *" ubuntu "* || " ${ID_LIKE:-} " == *" debian "* ]]; then
            echo ""
            echo "Hint: install the native desktop build prerequisites"
            echo "  ./android/get_deps/get_linux_build_prereqs.sh"
        fi
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)
            TARGET="$2"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --generator)
            GENERATOR="$2"
            shift 2
            ;;
        --cmake)
            CMAKE_PATH="$2"
            shift 2
            ;;
        --ninja)
            NINJA_PATH="$2"
            shift 2
            ;;
        --list-tools)
            LIST_TOOLS=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

case "$TARGET" in
    both|d1|d2)
        ;;
    *)
        echo "Unsupported target '$TARGET'. Expected both, d1, or d2" >&2
        exit 1
        ;;
esac

DEP_BASE=""
if DEP_BASE="$(read_dependency_base 2>/dev/null)"; then
    :
fi

cmake_candidates=()
ninja_candidates=()
if [[ -n "$DEP_BASE" ]]; then
    while IFS= read -r bin_dir; do
        cmake_candidates+=("$bin_dir/cmake")
        ninja_candidates+=("$bin_dir/ninja")
    done < <(get_android_sdk_cmake_bin_dirs "$DEP_BASE")
fi

if [[ -n "$CMAKE_PATH" ]]; then
    if [[ ! -x "$CMAKE_PATH" ]]; then
        echo "cmake executable not found: $CMAKE_PATH" >&2
        exit 1
    fi
else
    CMAKE_PATH="$(find_executable cmake "${cmake_candidates[@]}")" || {
        echo "cmake not found in PATH or dependency_base.txt managed SDK CMake" >&2
        exit 1
    }
fi

if [[ -n "$NINJA_PATH" ]]; then
    if [[ ! -x "$NINJA_PATH" ]]; then
        echo "ninja executable not found: $NINJA_PATH" >&2
        exit 1
    fi
else
    NINJA_PATH="$(find_executable ninja "${ninja_candidates[@]}")" || true
fi

if [[ -z "$GENERATOR" && -n "$NINJA_PATH" ]]; then
    GENERATOR="Ninja"
fi

if [[ "$LIST_TOOLS" -eq 1 ]]; then
    echo "dep base: ${DEP_BASE:-<none>}"
    echo "cmake: $CMAKE_PATH"
    echo "ninja: ${NINJA_PATH:-<none>}"
    echo "cc: $(command -v cc || echo '<none>')"
    echo "c++: $(command -v c++ || echo '<none>')"
    exit 0
fi

echo "Using cmake: $CMAKE_PATH"
if [[ -n "$NINJA_PATH" ]]; then
    echo "Using ninja: $NINJA_PATH"
fi
if [[ -n "$DEP_BASE" ]]; then
    echo "Dependency base: $DEP_BASE"
fi

cmake_dir="$(dirname "$CMAKE_PATH")"
export PATH="$cmake_dir:$PATH"
if [[ -n "$NINJA_PATH" ]]; then
    ninja_dir="$(dirname "$NINJA_PATH")"
    export PATH="$ninja_dir:$PATH"
fi

build_one() {
    local game="$1"
    local build_dir_name build_dir

    if [[ "$game" == "d1" ]]; then
        build_dir_name="buildd1"
    else
        build_dir_name="buildd2"
    fi
    build_dir="$REPO_ROOT/$build_dir_name"

    if [[ "$CLEAN" -eq 1 && -d "$build_dir" ]]; then
        rm -rf "$build_dir"
    fi

    local configure_args=(
        -S "$game"
        -B "$build_dir"
        -D "CMAKE_BUILD_TYPE=$BUILD_TYPE"
    )
    if [[ -n "$GENERATOR" ]]; then
        configure_args=(-G "$GENERATOR" "${configure_args[@]}")
    fi

    echo "Configuring $game ($BUILD_TYPE)"
    if ! "$CMAKE_PATH" "${configure_args[@]}"; then
        echo "CMake configure failed for $game" >&2
        show_linux_hint
        return 1
    fi

    local build_args=(--build "$build_dir" --parallel)
    if [[ -n "$JOBS" ]]; then
        build_args+=("$JOBS")
    fi

    echo "Building $game"
    "$CMAKE_PATH" "${build_args[@]}"

    local source_revision stamp_dir
    source_revision="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || true)"
    if [[ -n "$source_revision" ]]; then
        stamp_dir="$REPO_ROOT/temp/input_demo_build_stamps"
        mkdir -p "$stamp_dir"
        printf '%s' "$source_revision" >"$stamp_dir/$game.stamp"
    fi
}

if [[ "$TARGET" == "both" ]]; then
    build_one d1
    build_one d2
else
    build_one "$TARGET"
fi

echo "Linux build complete for $TARGET"
