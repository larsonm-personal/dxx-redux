#!/usr/bin/env bash
set -euo pipefail

TARGET="both"
BUILD_TYPE="RelWithDebInfo"
BUILD_TYPE_EXPLICIT=0
RUN_TESTS=0
CLEAN=0
JOBS=""
GENERATOR=""
CMAKE_PATH=""
CTEST_PATH=""
NINJA_PATH=""
LIST_TOOLS=0
SANITIZER="none"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"
cd "$REPO_ROOT"

usage() {
    cat <<'EOF'
Usage: ./run-linux-build.sh [options]

Options:
  --target {both|d1|d2}   Select which game to build [default: both]
  --build-type TYPE       CMake configuration [default: RelWithDebInfo, Debug with --test]
  --test                  Build and run asset-free CTest tests in separate Debug trees
  --sanitizer {none|address}  Use a separate AddressSanitizer build tree
  --clean                 Delete the target build directory before configuring
  --jobs N                Positive parallel build job count [default with --test: 4]
  --generator NAME        Override the CMake generator
  --cmake PATH            Use an explicit cmake executable
  --ninja PATH            Use an explicit ninja executable
  --list-tools            Print resolved tool paths and exit
  -h, --help              Show this help
EOF
}

require_value() {
    if [[ $# -lt 2 || -z "$2" || "$2" == -* ]]; then
        echo "Missing value for $1" >&2
        exit 1
    fi
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
            echo "  ./android/get_deps/helpers/get_linux_build_prereqs.sh"
        fi
    fi
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --target)
        require_value "$@"
        TARGET="$2"
        shift 2
        ;;
    --build-type)
        require_value "$@"
        BUILD_TYPE="$2"
        BUILD_TYPE_EXPLICIT=1
        shift 2
        ;;
    --test)
        RUN_TESTS=1
        shift
        ;;
    --sanitizer)
        require_value "$@"
        SANITIZER="$2"
        shift 2
        ;;
    --clean)
        CLEAN=1
        shift
        ;;
    --jobs)
        require_value "$@"
        JOBS="$2"
        shift 2
        ;;
    --generator)
        require_value "$@"
        GENERATOR="$2"
        shift 2
        ;;
    --cmake)
        require_value "$@"
        CMAKE_PATH="$2"
        shift 2
        ;;
    --ninja)
        require_value "$@"
        NINJA_PATH="$2"
        shift 2
        ;;
    --list-tools)
        LIST_TOOLS=1
        shift
        ;;
    -h | --help)
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
both | d1 | d2)
    ;;
*)
    echo "Unsupported target '$TARGET'. Expected both, d1, or d2" >&2
    exit 1
    ;;
esac

case "$SANITIZER" in
none | address) ;;
*)
    echo "Unsupported sanitizer '$SANITIZER'. Expected none or address" >&2
    exit 1
    ;;
esac

if [[ -n "$JOBS" && ! "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid --jobs '$JOBS'. Expected a positive integer" >&2
    exit 1
fi
if [[ "$RUN_TESTS" -eq 1 ]]; then
    if [[ "$BUILD_TYPE_EXPLICIT" -eq 0 ]]; then
        BUILD_TYPE="Debug"
    elif [[ "$BUILD_TYPE" != "Debug" ]]; then
        echo "--test requires --build-type Debug to keep test assertions active" >&2
        exit 1
    fi
    JOBS="${JOBS:-4}"
fi

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
    if [[ ! -f "$CMAKE_PATH" || ! -x "$CMAKE_PATH" ]]; then
        echo "cmake executable not found: $CMAKE_PATH" >&2
        exit 1
    fi
else
    CMAKE_PATH="$(find_executable cmake "${cmake_candidates[@]}")" || {
        echo "cmake not found in PATH or dependency_base.txt managed SDK CMake" >&2
        show_linux_hint >&2
        exit 1
    }
fi
CMAKE_PATH="$(cd "$(dirname "$CMAKE_PATH")" && pwd)/${CMAKE_PATH##*/}"

if [[ "$RUN_TESTS" -eq 1 ]]; then
    # Prefer the CTest shipped with the selected CMake, including managed SDK tools
    CTEST_PATH="$(dirname "$CMAKE_PATH")/ctest"
    if [[ ! -f "$CTEST_PATH" || ! -x "$CTEST_PATH" ]]; then
        CTEST_PATH="$(find_executable ctest)" || {
            echo "ctest not found beside selected cmake or in PATH" >&2
            exit 1
        }
    fi
fi

if [[ -n "$NINJA_PATH" ]]; then
    if [[ ! -f "$NINJA_PATH" || ! -x "$NINJA_PATH" ]]; then
        echo "ninja executable not found: $NINJA_PATH" >&2
        exit 1
    fi
else
    NINJA_PATH="$(find_executable ninja "${ninja_candidates[@]}")" || true
fi
if [[ -n "$NINJA_PATH" ]]; then
    NINJA_PATH="$(cd "$(dirname "$NINJA_PATH")" && pwd)/${NINJA_PATH##*/}"
fi

if [[ -z "$GENERATOR" && -n "$NINJA_PATH" ]]; then
    GENERATOR="Ninja"
fi

if [[ "$LIST_TOOLS" -eq 1 ]]; then
    echo "dep base: ${DEP_BASE:-<none>}"
    echo "cmake: $CMAKE_PATH"
    if [[ "$RUN_TESTS" -eq 1 ]]; then echo "ctest: $CTEST_PATH"; fi
    echo "ninja: ${NINJA_PATH:-<none>}"
    echo "cc: $(command -v cc || echo '<none>')"
    echo "c++: $(command -v c++ || echo '<none>')"
    exit 0
fi

echo "Using cmake: $CMAKE_PATH"
if [[ "$RUN_TESTS" -eq 1 ]]; then echo "Using ctest: $CTEST_PATH"; fi
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
    if [[ "$RUN_TESTS" -eq 1 ]]; then build_dir_name+="-test"; fi
    local stamp_name="$game"
    if [[ "$SANITIZER" == "address" ]]; then
        build_dir_name+="-asan"
        stamp_name+="-asan"
        # Instrumented trees are reusable, but a first build needs replacement headroom
        # shellcheck disable=SC1091
        source "$REPO_ROOT/android/get_deps/helpers/platform.sh"
        assert_dependency_disk_space "$REPO_ROOT" 4
    fi
    build_dir="$REPO_ROOT/$build_dir_name"

    if [[ "$CLEAN" -eq 1 && -d "$build_dir" ]]; then
        rm -rf "$build_dir"
    fi

    local configure_args=(
        -S "$game"
        -B "$build_dir"
        -D "CMAKE_BUILD_TYPE=$BUILD_TYPE"
        -D "DXX_SANITIZERS=$(if [[ "$SANITIZER" == address ]]; then echo address; fi)"
    )
    if [[ "$RUN_TESTS" -eq 1 || "$SANITIZER" == address ]]; then configure_args+=(-D "BUILD_TESTING=ON"); fi
    if [[ "$RUN_TESTS" -eq 1 ]]; then
        # Keep assertions and all native targets while bounding Debug information size
        configure_args+=(
            -D "CMAKE_C_FLAGS_DEBUG=-O0 -g1" -D "CMAKE_CXX_FLAGS_DEBUG=-O0 -g1"
            -D "OPENGL=ON" -D "SDLMIXER=ON" -D "UDP=ON" -D "OpenGL_GL_PREFERENCE=LEGACY"
        )
    fi
    if [[ -n "$GENERATOR" ]]; then
        configure_args=(-G "$GENERATOR" "${configure_args[@]}")
    fi
    if [[ "$GENERATOR" == Ninja* && -n "$NINJA_PATH" ]]; then
        configure_args+=(-D "CMAKE_MAKE_PROGRAM=$NINJA_PATH")
    fi

    echo "Configuring $game ($BUILD_TYPE)"
    if ! "$CMAKE_PATH" "${configure_args[@]}"; then
        echo "CMake configure failed for $game" >&2
        show_linux_hint
        return 1
    fi

    local build_args=(--build "$build_dir" --config "$BUILD_TYPE" --parallel)
    if [[ -n "$JOBS" ]]; then
        build_args+=("$JOBS")
    fi

    echo "Building $game"
    "$CMAKE_PATH" "${build_args[@]}"

    if [[ "$RUN_TESTS" -eq 1 ]]; then
        echo "Testing $game"
        # Some fixtures share filenames; retain serial execution even with parallel builds
        SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy \
            "$CTEST_PATH" --test-dir "$build_dir" -C "$BUILD_TYPE" \
            --output-on-failure --no-tests=error --parallel 1 --timeout 120 \
            --output-junit "$build_dir/ctest-results.xml"
        # Replay freshness stamps describe the normal host binaries, not these test trees
        return
    fi

    local source_revision stamp_dir
    source_revision="$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || true)"
    if [[ -n "$source_revision" ]]; then
        stamp_dir="$REPO_ROOT/temp/input_demo_build_stamps"
        mkdir -p "$stamp_dir"
        printf '%s' "$source_revision" >"$stamp_dir/$stamp_name.stamp"
    fi
}

if [[ "$TARGET" == "both" ]]; then
    build_one d1
    build_one d2
else
    build_one "$TARGET"
fi

if [[ "$RUN_TESTS" -eq 1 ]]; then
    echo "Linux build and tests complete for $TARGET"
else
    echo "Linux build complete for $TARGET"
fi
