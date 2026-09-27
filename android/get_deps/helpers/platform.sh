#!/bin/bash

get_host_os() {
    case "$(uname -s)" in
    Linux*) echo "linux" ;;
    Darwin*) echo "macos" ;;
    MINGW* | MSYS* | CYGWIN* | *_NT*) echo "windows" ;;
    *) echo "unknown" ;;
    esac
}

get_default_dependency_base() {
    case "$(get_host_os)" in
    windows) printf '%s\n' 'C:\local' ;;
    *) printf '%s\n' "$HOME/local" ;;
    esac
}

create_temp_file() {
    local prefix="$1"
    local parent="${2:-${TMPDIR:-/tmp}}"
    mktemp "$parent/$prefix.XXXXXX"
}

create_temp_dir() {
    local prefix="$1"
    local parent="${2:-${TMPDIR:-/tmp}}"
    mktemp -d "$parent/$prefix.XXXXXX"
}

get_sdk_cmdline_tools_os_token() {
    case "$(get_host_os)" in
    windows) echo "win" ;;
    linux) echo "linux" ;;
    macos) echo "mac" ;;
    *) return 1 ;;
    esac
}

get_ndk_archive_os_token() {
    case "$(get_host_os)" in
    windows) echo "windows" ;;
    linux) echo "linux" ;;
    macos) echo "darwin" ;;
    *) return 1 ;;
    esac
}

get_adoptium_os_token() {
    case "$(get_host_os)" in
    windows) echo "windows" ;;
    linux) echo "linux" ;;
    macos) echo "mac" ;;
    *) return 1 ;;
    esac
}

get_sdk_cmdline_tools_build_id() {
    local url="$1"
    if [[ "$url" =~ commandlinetools-(win|linux|mac)-([0-9]+)_latest\.zip ]]; then
        printf '%s\n' "${BASH_REMATCH[2]}"
        return 0
    fi
    return 1
}

get_sdk_cmdline_tools_url() {
    local build_id="$1"
    local os_token
    os_token="$(get_sdk_cmdline_tools_os_token)" || return 1
    printf 'https://dl.google.com/android/repository/commandlinetools-%s-%s_latest.zip\n' "$os_token" "$build_id"
}

get_ndk_download_url() {
    local ndk_version="$1"
    local os_token
    os_token="$(get_ndk_archive_os_token)" || return 1
    printf 'https://dl.google.com/android/repository/android-ndk-%s-%s.zip\n' "$ndk_version" "$os_token"
}

get_jdk_download_url() {
    local jdk_major="$1"
    local version="$2"
    local build="$3"
    local os_token archive_kind="tar.gz"
    os_token="$(get_adoptium_os_token)" || return 1
    [ "$os_token" != "windows" ] || archive_kind="zip"
    printf 'https://github.com/adoptium/temurin%s-binaries/releases/download/jdk-%s%%2B%s/OpenJDK%sU-jdk_x64_%s_hotspot_%s_%s.%s\n' \
        "$jdk_major" "$version" "$build" "$jdk_major" "$os_token" "$version" "$build" "$archive_kind"
}

is_windows_target_path() {
    local target_path="${1:-$LOCAL_DIR}"
    case "$target_path" in
    [A-Za-z]:[\\/]* | /mnt/[A-Za-z]/* | /[A-Za-z]/*) return 0 ;;
    esac

    [ "$(get_host_os)" = "windows" ]
}

get_platform_executable_name() {
    local tool_name="$1"
    if is_windows_target_path "${2:-$LOCAL_DIR}"; then
        printf '%s.exe\n' "$tool_name"
    else
        printf '%s\n' "$tool_name"
    fi
}

get_platform_batch_name() {
    local tool_name="$1"
    if is_windows_target_path "${2:-$LOCAL_DIR}"; then
        printf '%s.bat\n' "$tool_name"
    else
        printf '%s\n' "$tool_name"
    fi
}

download_file() {
    local destination="$1"
    local url="$2"

    if command -v curl >/dev/null 2>&1; then
        curl -fSL --progress-bar -o "$destination" "$url"
        return $?
    fi

    if command -v wget >/dev/null 2>&1; then
        wget --progress=dot:giga -O "$destination" "$url"
        return $?
    fi

    echo "ERROR: neither curl nor wget is available for downloads" >&2
    return 1
}

download_text() {
    local url="$1"
    shift

    if command -v curl >/dev/null 2>&1; then
        local curl_args=()
        local header
        for header in "$@"; do
            curl_args+=(-H "$header")
        done
        curl -fsSL "${curl_args[@]}" "$url"
        return $?
    fi

    if command -v wget >/dev/null 2>&1; then
        local wget_args=()
        local header
        for header in "$@"; do
            wget_args+=(--header="$header")
        done
        wget -qO- "${wget_args[@]}" "$url"
        return $?
    fi

    echo "ERROR: neither curl nor wget is available for downloads" >&2
    return 1
}

# Check the reported version before reusing or publishing executable tools
verify_dependency_tool_version() {
    local expected="$1" mode="$2"
    shift 2
    local output actual
    output="$("$@")" || return 1
    if [[ ! "$output" =~ (^|[[:space:]])v?([0-9]+(\.[0-9]+)+([+-][[:alnum:].-]+)?)($|[[:space:]]) ]]; then
        echo "ERROR: tool did not report a version: $output" >&2
        return 1
    fi
    actual="${BASH_REMATCH[2]}"
    if [ "$actual" != "$expected" ]; then
        if [ "$mode" != prefix ] || [[ "$actual" != "$expected".* ]]; then
            echo "ERROR: expected tool version $expected, found $actual" >&2
            return 1
        fi
    fi
    printf '%s\n' "$output"
}

# Publish a validated, staged directory and restore the old install on failure
publish_dependency_directory() {
    local staged="$1" destination="$2"
    local backup=""
    if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ]; then
        destination="$(cd "$(dirname "$destination")" && pwd -P)/$(basename "$destination")"
        if [ "$destination" != "$DEPENDENCY_INSTALL_DESTINATION" ]; then
            echo "ERROR: publication destination does not match the held installer lock" >&2
            return 1
        fi
    fi
    if [ ! -d "$staged" ] || [ -L "$staged" ] || [ -L "$destination" ]; then
        echo "ERROR: refusing invalid or linked installation path: $destination" >&2
        return 1
    fi
    if [ -e "$destination" ]; then
        if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ]; then
            backup="$DEPENDENCY_INSTALL_STATE/work/previous"
        else
            backup="$(create_temp_dir .dxx-install-backup "$(dirname "$destination")")" || return 1
            rmdir "$backup" || return 1
        fi
        if ! mv "$destination" "$backup"; then return 1; fi
    fi
    if ! mv "$staged" "$destination"; then
        if [ -n "$backup" ]; then
            if ! mv "$backup" "$destination"; then
                echo "ERROR: restore $backup to $destination before retrying" >&2
            fi
        fi
        return 1
    fi
    if [ -n "$backup" ]; then rm -rf "$backup"; fi
}

# Change the journal format atomically so older checkouts cannot misread in-place work
set_dependency_install_format() {
    local temporary="$DEPENDENCY_INSTALL_STATE/format.new"
    if [ -L "$temporary" ] || { [ -e "$temporary" ] && [ ! -f "$temporary" ]; }; then return 1; fi
    printf '%s\n' "$1" >"$temporary" || return 1
    mv "$temporary" "$DEPENDENCY_INSTALL_STATE/format"
}

# Recover only our fixed transaction, while holding the shared destination-volume lock
recover_dependency_install() {
    local work="$DEPENDENCY_INSTALL_STATE/work" leaf destination in_place=0 marker
    if [ ! -e "$work" ] && [ ! -L "$work" ]; then
        if [ "$(cat "$DEPENDENCY_INSTALL_STATE/format")" = dxx-install-v2 ]; then set_dependency_install_format dxx-install-v1 || return 1; fi
        return 0
    fi
    if [ ! -d "$work" ] || [ -L "$work" ]; then
        echo "ERROR: unsafe dependency transaction directory: $work" >&2
        return 1
    fi
    if [ -e "$work/mode" ] || [ -L "$work/mode" ]; then
        if [ -L "$work/mode" ] || [ ! -f "$work/mode" ] || [ "$(cat "$work/mode")" != in-place ]; then return 1; fi
        in_place=1
        for marker in installing committed; do
            if [ -L "$work/$marker" ] || { [ -e "$work/$marker" ] && [ ! -f "$work/$marker" ]; }; then return 1; fi
        done
    fi
    if [ "$in_place" = 1 ] || [ -e "$work/previous" ] || [ -L "$work/previous" ]; then
        if [ ! -f "$work/destination" ] || [ -L "$work/destination" ]; then return 1; fi
        leaf="$(cat "$work/destination")" || return 1
        if [[ ! "$leaf" =~ ^[[:alnum:]][[:alnum:]._+-]*$ ]]; then
            echo "ERROR: invalid dependency recovery destination" >&2
            return 1
        fi
        destination="$(dirname "$DEPENDENCY_INSTALL_STATE")/$leaf"
        if [ -L "$destination" ] || [ -L "$work/previous" ] || { [ -e "$work/previous" ] && [ ! -d "$work/previous" ]; }; then
            echo "ERROR: unsafe dependency recovery paths" >&2
            return 1
        fi
        if [ -e "$destination" ] && [ ! -d "$destination" ]; then
            echo "ERROR: unexpected file at dependency recovery destination" >&2
            return 1
        fi
        if [ "$in_place" = 1 ] && [ ! -f "$work/committed" ] && { [ -d "$work/previous" ] || [ -f "$work/installing" ]; }; then
            rm -rf "$destination" || return 1
        fi
        if [ ! -e "$destination" ] && [ -d "$work/previous" ]; then
            # A second interruption after restoration must not delete the restored tree
            if [ "$in_place" = 1 ]; then rm -f "$work/installing" || return 1; fi
            mv "$work/previous" "$destination" || return 1
        fi
    fi
    rm -rf "$work" || return 1
    if [ "$(cat "$DEPENDENCY_INSTALL_STATE/format")" = dxx-install-v2 ]; then set_dependency_install_format dxx-install-v1 || return 1; fi
}

# Python venv launchers embed their final path, so they cannot be staged and moved
# Journal each destructive step before installation; only validated installs commit
prepare_in_place_dependency_install() {
    local destination="$1"
    if [ -z "${DEPENDENCY_INSTALL_STATE:-}" ]; then return 1; fi
    destination="$(cd "$(dirname "$destination")" && pwd -P)/$(basename "$destination")"
    if [ "$destination" != "$DEPENDENCY_INSTALL_DESTINATION" ] || [ -L "$destination" ]; then return 1; fi
    # Older archive-only recovery would mistake a partial final-path tree for success
    set_dependency_install_format dxx-install-v2 || return 1
    prepare_dependency_workspace "$destination"
    printf '%s\n' in-place >"$DEPENDENCY_WORK_DIR/mode"
    if [ -e "$destination" ]; then
        if [ ! -d "$destination" ]; then return 1; fi
        mv "$destination" "$DEPENDENCY_WORK_DIR/previous" || return 1
    fi
    touch "$DEPENDENCY_WORK_DIR/installing" || return 1
    mkdir "$destination"
}

commit_in_place_dependency_install() {
    if [ -z "${DEPENDENCY_INSTALL_STATE:-}" ] || [ ! -f "$DEPENDENCY_WORK_DIR/installing" ]; then return 1; fi
    touch "$DEPENDENCY_WORK_DIR/committed"
}

finish_dependency_install() {
    local status=$?
    recover_dependency_install || status=1
    exit "$status"
}

# Linux flock descriptors stay open in installer children, including after parent SIGKILL
# Keep the lock inode permanently: unlinking it would permit two simultaneous owners
begin_dependency_install() {
    local destination="$1" parent timeout="${DXX_DEPENDENCY_LOCK_TIMEOUT_SECONDS:-60}"
    if [ "$(get_host_os)" != linux ]; then return 0; fi
    if ! command -v flock >/dev/null 2>&1; then
        echo "ERROR: Linux dependency installation requires flock (util-linux)" >&2
        return 1
    fi
    if [[ ! "$timeout" =~ ^[0-9]{1,4}$ ]] || [ "$timeout" -gt 3600 ]; then
        echo "ERROR: dependency lock timeout must be 0..3600 seconds" >&2
        return 1
    fi
    parent="$(cd "$(dirname "$destination")" && pwd -P)" || return 1
    DEPENDENCY_INSTALL_DESTINATION="$parent/$(basename "$destination")"
    DEPENDENCY_INSTALL_STATE="$parent/.dxx-install-state"
    if [ -L "$DEPENDENCY_INSTALL_STATE" ]; then return 1; fi
    mkdir -p "$DEPENDENCY_INSTALL_STATE" || return 1
    if [ -L "$DEPENDENCY_INSTALL_STATE/lock" ] || { [ -e "$DEPENDENCY_INSTALL_STATE/lock" ] && [ ! -f "$DEPENDENCY_INSTALL_STATE/lock" ]; }; then return 1; fi
    exec {DEPENDENCY_INSTALL_LOCK_FD}>>"$DEPENDENCY_INSTALL_STATE/lock" || return 1
    if ! flock -w "$timeout" "$DEPENDENCY_INSTALL_LOCK_FD"; then
        echo "ERROR: another dependency installer owns $parent; retry after it finishes" >&2
        return 1
    fi
    if [ ! -e "$DEPENDENCY_INSTALL_STATE/format" ]; then
        if [ -n "$(find "$DEPENDENCY_INSTALL_STATE" -mindepth 1 -maxdepth 1 ! -name lock -print -quit)" ]; then
            echo "ERROR: refusing unrecognized dependency transaction state" >&2
            return 1
        fi
        printf '%s\n' dxx-install-v1 >"$DEPENDENCY_INSTALL_STATE/format"
    fi
    if [ -L "$DEPENDENCY_INSTALL_STATE/format" ] || [ ! -f "$DEPENDENCY_INSTALL_STATE/format" ]; then return 1; fi
    case "$(cat "$DEPENDENCY_INSTALL_STATE/format")" in
    dxx-install-v1 | dxx-install-v2) ;;
    *) return 1 ;;
    esac
    recover_dependency_install || return 1
    trap finish_dependency_install EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
}

# Keep a reserve in addition to estimated archive and unpacked installation bytes
assert_dependency_disk_space() {
    local directory="$1" expansion_gb="${2:-0}"
    local reserve_gb="${DXX_DEPENDENCY_MIN_FREE_GB:-4}"
    local disk_info available_kb required_kb
    if [[ ! "$reserve_gb" =~ ^[0-9]{1,7}$ ]] || [[ ! "$expansion_gb" =~ ^[0-9]{1,7}$ ]]; then
        echo "ERROR: dependency disk space limits must be whole GiB values" >&2
        return 1
    fi
    reserve_gb=$((10#$reserve_gb))
    expansion_gb=$((10#$expansion_gb))
    if [ "$reserve_gb" -lt 1 ] || [ "$reserve_gb" -gt 1048576 ] || [ "$expansion_gb" -gt 1048576 ]; then
        echo "ERROR: dependency disk space limit is out of range" >&2
        return 1
    fi
    disk_info="$(df -Pk "$directory")" || return 1
    available_kb="$(printf '%s\n' "$disk_info" | awk 'NR == 2 {print $4}')"
    if [[ ! "$available_kb" =~ ^[0-9]+$ ]]; then
        echo "ERROR: cannot determine free space for $directory" >&2
        return 1
    fi
    required_kb=$(((reserve_gb + expansion_gb) * 1024 * 1024))
    if [ "$available_kb" -lt "$required_kb" ]; then
        echo "ERROR: insufficient space at $directory: $available_kb KiB free, $required_kb KiB required for installation and reserve" >&2
        return 1
    fi
}

# Use one owned workspace on the destination volume for small tool installers
# Call in the installer process, after any early already-installed return
prepare_dependency_workspace() {
    local destination="$1"
    assert_dependency_disk_space "$(dirname "$destination")" "${2:-1}"
    if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ]; then
        DEPENDENCY_WORK_DIR="$DEPENDENCY_INSTALL_STATE/work"
        mkdir "$DEPENDENCY_WORK_DIR" || return 1
        printf '%s\n' "$(basename "$destination")" >"$DEPENDENCY_WORK_DIR/destination"
    else
        DEPENDENCY_WORK_DIR="$(create_temp_dir .dxx-tool-stage "$(dirname "$destination")")" || return 1
        trap 'rm -rf "$DEPENDENCY_WORK_DIR"' EXIT
        trap 'exit 130' INT
        trap 'exit 143' TERM
    fi
    DEPENDENCY_STAGE_DIR="$DEPENDENCY_WORK_DIR/install"
    # Used by the calling installer after this helper returns
    # shellcheck disable=SC2034
    DEPENDENCY_ARCHIVE="$DEPENDENCY_WORK_DIR/archive"
    mkdir "$DEPENDENCY_STAGE_DIR"
}
