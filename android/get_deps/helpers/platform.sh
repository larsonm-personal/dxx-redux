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
    local os_token
    os_token="$(get_adoptium_os_token)" || return 1
    printf 'https://api.adoptium.net/v3/binary/latest/%s/ga/%s/x64/jdk/hotspot/normal/eclipse?project=jdk\n' "$jdk_major" "$os_token"
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
        return 0
    fi

    if command -v wget >/dev/null 2>&1; then
        wget --progress=dot:giga -O "$destination" "$url"
        return 0
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
        return 0
    fi

    if command -v wget >/dev/null 2>&1; then
        local wget_args=()
        local header
        for header in "$@"; do
            wget_args+=(--header="$header")
        done
        wget -qO- "${wget_args[@]}" "$url"
        return 0
    fi

    echo "ERROR: neither curl nor wget is available for downloads" >&2
    return 1
}
