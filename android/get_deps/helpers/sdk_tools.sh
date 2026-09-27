#!/usr/bin/env bash
# Shared host selection and environment for SDK package and AVD tools
# Re-enter Windows shell producers under the shared PowerShell SDK writer lock
run_sdk_writer_if_needed() {
    if [ "$(get_host_os)" != windows ] || [ -n "${DXX_SDK_WRITER_PARENT_PID:-}" ]; then return 0; fi
    local helpers_dir repo_root powershell wrapper status=0
    helpers_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || return 1
    repo_root="$(cd "$helpers_dir/../../.." && pwd)" || return 1
    powershell="$(command -v pwsh || command -v powershell.exe)" || {
        echo 'ERROR: PowerShell is required for SDK writer locking' >&2
        return 1
    }
    wrapper="$helpers_dir/invoke_sdk_writer.ps1"
    if command -v cygpath >/dev/null 2>&1; then
        wrapper="$(cygpath -w "$wrapper")" || return 1
        repo_root="$(cygpath -w "$repo_root")" || return 1
    fi
    "$powershell" -NoProfile -NonInteractive -File "$wrapper" -RepoRoot "$repo_root" -ScriptName "$(basename "$0")" || status=$?
    if [ "$status" -eq 0 ] && [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
        echo 'Press any key to exit'
        read -r -n1 -s
    fi
    exit "$status"
}

initialize_sdk_environment() {
    run_sdk_writer_if_needed
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

# Full bootstrap performs retention once after all producers finish
complete_sdk_provisioning() {
    if [ -n "${GET_ALL_RUNNING:-}" ]; then return 0; fi
    if [ "$(get_host_os)" != linux ]; then
        echo 'Standalone SDK retention is not yet enabled on this host'
        return 0
    fi
    local helpers_dir repo_root powershell
    helpers_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" || return 1
    repo_root="$(cd "$helpers_dir/../../.." && pwd)" || return 1
    powershell="$(command -v pwsh)" || {
        echo 'ERROR: PowerShell is required for SDK retention; run get_powershell.sh and retry' >&2
        return 1
    }
    # Finish recovery under the installer lock, then let the cleaner take that lock
    recover_dependency_install || return 1
    exec {DEPENDENCY_INSTALL_LOCK_FD}>&-
    unset DEPENDENCY_INSTALL_LOCK_FD DEPENDENCY_INSTALL_STATE DEPENDENCY_INSTALL_DESTINATION
    trap - EXIT
    "$powershell" -NoProfile -NonInteractive -File "$helpers_dir/../clean-sdk-packages.ps1" -RepoRoot "$repo_root" -RegisterCurrent -Apply
}
