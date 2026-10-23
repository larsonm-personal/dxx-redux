#!/usr/bin/env bash
# Check or install pinned user-local tools, or run a command with those tools
set -eo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
action="${1:---check}"
if [ "$#" -gt 0 ]; then shift; fi
case "$action" in
--check | --install) [ "$#" -eq 0 ] || {
    echo "Unexpected arguments" >&2
    exit 2
} ;;
--run)
    [ "${1:-}" = -- ] && [ "$#" -gt 1 ] || {
        echo "Use --run -- COMMAND [ARGS...]" >&2
        exit 2
    }
    shift
    ;;
--help)
    echo "Usage: build_tools.sh [--check | --install | --run -- COMMAND [ARGS...]]"
    echo "DXX_DEPENDENCY_BASE selects tools when dependency_base.txt is absent"
    echo "DXX_INSTALL_TIMEOUT_SECONDS bounds installation (default 5400, maximum 7200)"
    exit 0
    ;;
*)
    echo "Unknown action: $action" >&2
    exit 2
    ;;
esac
export DXX_DEPENDENCY_READ_ONLY=1
source "$SCRIPT_DIR/helpers/resolve_dep_base.sh"
configured_base="$LOCAL_DIR"
if [ -f "$SCRIPT_DIR/../../dependency_base.txt" ]; then
    configured_base="$(head -1 "$SCRIPT_DIR/../../dependency_base.txt" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
fi
if [ -n "${DXX_DEPENDENCY_BASE:-}" ] && [ "${DXX_DEPENDENCY_BASE%/}" != "${LOCAL_DIR%/}" ] && [ "${DXX_DEPENDENCY_BASE%/}" != "${configured_base%/}" ]; then
    echo "DXX_DEPENDENCY_BASE conflicts with dependency_base.txt ($LOCAL_DIR); select one dependency base" >&2
    exit 2
fi
# Select managed tools only in this process and its children
source "$SCRIPT_DIR/tool_versions.conf"
export JAVA_HOME="$LOCAL_DIR/jdk-$JDK_MAJOR"
export ANDROID_HOME="$LOCAL_DIR/android-sdk" ANDROID_SDK_ROOT="$LOCAL_DIR/android-sdk"
export ANDROID_NDK_ROOT="$LOCAL_DIR/android-ndk-$NDK_VERSION"
export GRADLE_USER_HOME="${GRADLE_USER_HOME:-$LOCAL_DIR/gradle-cache}"
export PATH="$LOCAL_DIR/cmake-$CMAKE_VERSION/bin:$JAVA_HOME/bin:$PATH"
if [ "$action" = --install ]; then
    budget="${DXX_INSTALL_TIMEOUT_SECONDS:-5400}"
    if [[ ! "$budget" =~ ^[1-9][0-9]{0,3}$ ]] || ((budget > 7200)); then
        echo "DXX_INSTALL_TIMEOUT_SECONDS must be 1..7200" >&2
        exit 2
    fi
    timer="$(command -v timeout || command -v gtimeout || true)"
    if [ -z "$timer" ] || ! "$timer" --version | head -1 | grep -q 'GNU coreutils'; then
        echo "Installation requires GNU timeout (or gtimeout) for the total deadline" >&2
        exit 2
    fi
    export GET_ALL_RUNNING=1
    unset DXX_DEPENDENCY_READ_ONLY
    deadline=$((SECONDS + budget))
    for installer in get_jdk get_sdk get_ndk get_cmake get_gradle_wrapper; do
        remaining=$((deadline - SECONDS))
        if ((remaining <= 0)); then
            echo "Tool installation deadline exhausted before $installer" >&2
            exit 124
        fi
        printf 'Installing %s (%s seconds remaining)\n' "$installer" "$remaining"
        "$timer" --kill-after=5 "$remaining" bash "$SCRIPT_DIR/helpers/$installer.sh"
    done
fi
bash "$SCRIPT_DIR/check_build_prereqs.sh" --scope bootstrap
if [ "$action" = --run ]; then exec "$@"; fi
