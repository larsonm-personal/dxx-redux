#!/usr/bin/env bash
# Shared host selection and environment for SDK package and AVD tools
initialize_sdk_environment() {
    SDK_DIR="$LOCAL_DIR/android-sdk"
    local tools_dir="$SDK_DIR/cmdline-tools/latest"
    if [ ! -d "$(dirname "$tools_dir")" ]; then
        echo 'ERROR: Android command-line tools are missing; run get_sdk.sh first' >&2
        return 1
    fi
    # Share the command-line installer lock before resolving or executing its tools
    begin_dependency_install "$tools_dir" || return 1
    if [ -z "${JAVA_HOME:-}" ]; then
        local android_dir
        android_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" || return 1
        source "$android_dir/helpers/set_vars.sh"
    fi
    if [ "$(get_host_os)" = windows ] && command -v cygpath >/dev/null 2>&1; then
        ANDROID_HOME="$(cygpath -w "$SDK_DIR")" || return 1
    else
        ANDROID_HOME="$SDK_DIR"
    fi
    export ANDROID_HOME
    export ANDROID_SDK_ROOT="$ANDROID_HOME"
}

resolve_sdk_tool() {
    local name="$1" candidate="$SDK_DIR/cmdline-tools/latest/bin/$1"
    case "$(get_host_os)" in
    windows)
        candidate="$candidate.bat"
        if [ -f "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
        ;;
    linux)
        if [ -x "$candidate" ]; then
            printf '%s\n' "$candidate"
            return 0
        fi
        ;;
    *)
        echo 'ERROR: SDK provisioning currently supports Windows and Linux hosts' >&2
        return 1
        ;;
    esac
    echo "ERROR: $name not found for this host; run get_sdk.sh first" >&2
    return 1
}

sdk_package_has_metadata() {
    local directory="$SDK_DIR/${1//;/\/}"
    [ -s "$directory/package.xml" ] && [ -s "$directory/source.properties" ]
}
