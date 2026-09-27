#!/bin/bash
# Install PowerShell 7 on Linux hosts, using the pinned version when possible
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/../tool_versions.conf"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/platform.sh"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/resolve_dep_base.sh"

privilege_available() {
    [[ $EUID -eq 0 ]] \
        || { command -v sudo >/dev/null 2>&1 && { [[ -t 0 ]] || sudo -n true 2>/dev/null; }; }
}

run_with_privilege() {
    if [[ $EUID -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1 && { [[ -t 0 ]] || sudo -n true 2>/dev/null; }; then
        sudo "$@"
    else
        echo "ERROR: PowerShell installation needs root or usable sudo" >&2
        echo "Run this helper from a terminal with sudo, or use a distro with the user-local tarball path" >&2
        return 1
    fi
}

get_powershell_package_name() {
    case "$POWERSHELL_VERSION" in
    *preview* | *rc*) echo "powershell-preview" ;;
    *) echo "powershell" ;;
    esac
}

ensure_pwsh_command() {
    if command -v pwsh >/dev/null 2>&1; then
        return 0
    fi

    if command -v pwsh-preview >/dev/null 2>&1; then
        echo "Creating /usr/local/bin/pwsh -> pwsh-preview so repo scripts can use pwsh"
        run_with_privilege ln -sf "$(command -v pwsh-preview)" /usr/local/bin/pwsh
        hash -r
        return 0
    fi

    return 1
}

get_installed_pwsh_command() {
    if command -v pwsh >/dev/null 2>&1; then
        command -v pwsh
        return 0
    fi

    if command -v pwsh-preview >/dev/null 2>&1; then
        command -v pwsh-preview
        return 0
    fi

    return 1
}

get_pwsh_version() {
    local pwsh_command="$1"

    # shellcheck disable=SC2016
    "$pwsh_command" -NoProfile -NonInteractive -Command '$PSVersionTable.PSVersion.ToString()' 2>/dev/null
}

find_github_powershell_asset_url() {
    local asset_pattern="$1"
    local tag="v$POWERSHELL_VERSION"
    local api_url="https://api.github.com/repos/PowerShell/PowerShell/releases/tags/$tag"
    local release_json
    release_json="$(download_text "$api_url" 'Accept: application/vnd.github+json' 'User-Agent: dxx-redux-get-powershell')"
    echo "$release_json" \
        | grep -o '"browser_download_url": *"[^"]*"' \
        | sed 's/"browser_download_url": *"//;s/"$//' \
        | grep -E "$asset_pattern" \
        | head -1
}

install_deb() {
    local package_name="$1"
    local deb_file="$TMPDIR_LOCAL/powershell.deb"
    local asset_url
    privilege_available || {
        echo "ERROR: installing the PowerShell package needs root or usable sudo" >&2
        return 1
    }
    asset_url="$(find_github_powershell_asset_url "/${package_name}_.+\\.deb_amd64\\.deb$")"
    if [ -z "$asset_url" ]; then
        echo "ERROR: could not find a .deb asset for PowerShell $POWERSHELL_VERSION" >&2
        exit 1
    fi

    echo "Downloading PowerShell $POWERSHELL_VERSION .deb"
    download_file "$deb_file" "$asset_url"
    if command -v apt >/dev/null 2>&1; then
        run_with_privilege apt update
        run_with_privilege apt install -y "$deb_file"
    else
        run_with_privilege dpkg -i "$deb_file"
    fi
}

install_rpm() {
    local package_name="$1"
    local rpm_file="$TMPDIR_LOCAL/powershell.rpm"
    local asset_url
    privilege_available || {
        echo "ERROR: installing the PowerShell package needs root or usable sudo" >&2
        return 1
    }
    asset_url="$(find_github_powershell_asset_url "/${package_name}-.+\\.rh\\.x86_64\\.rpm$")"
    if [ -z "$asset_url" ]; then
        asset_url="$(find_github_powershell_asset_url "/${package_name}-.+\\.x86_64\\.rpm$")"
    fi
    if [ -z "$asset_url" ]; then
        echo "ERROR: could not find an .rpm asset for PowerShell $POWERSHELL_VERSION" >&2
        exit 1
    fi

    echo "Downloading PowerShell $POWERSHELL_VERSION .rpm"
    download_file "$rpm_file" "$asset_url"
    if command -v dnf >/dev/null 2>&1; then
        run_with_privilege dnf install -y "$rpm_file"
    elif command -v yum >/dev/null 2>&1; then
        run_with_privilege yum install -y "$rpm_file"
    else
        echo "ERROR: no supported rpm installer found" >&2
        exit 1
    fi
}

install_tarball() {
    local archive_file="$TMPDIR_LOCAL/powershell.tar.gz"
    local install_dir="$INSTALL_DIR"
    if [ -L "$install_dir" ]; then
        echo "ERROR: refusing linked PowerShell installation: $install_dir" >&2
        return 1
    fi
    if [ -x "$install_dir/pwsh" ] && [ "$(get_pwsh_version "$install_dir/pwsh")" = "$POWERSHELL_VERSION" ]; then
        echo "Using validated PowerShell at $install_dir"
    else
        local asset_url
        asset_url="$(find_github_powershell_asset_url "/powershell-.+-linux-x64\\.tar\\.gz$")"
        if [ -z "$asset_url" ]; then
            echo "ERROR: could not find a Linux tarball for PowerShell $POWERSHELL_VERSION" >&2
            exit 1
        fi

        echo "Downloading PowerShell $POWERSHELL_VERSION Linux tarball"
        download_file "$archive_file" "$asset_url"
        tar -xzf "$archive_file" -C "$DEPENDENCY_STAGE_DIR"
        chmod +x "$DEPENDENCY_STAGE_DIR/pwsh"
        if [ "$(get_pwsh_version "$DEPENDENCY_STAGE_DIR/pwsh")" != "$POWERSHELL_VERSION" ]; then
            echo "ERROR: staged PowerShell does not match $POWERSHELL_VERSION" >&2
            return 1
        fi
        publish_dependency_directory "$DEPENDENCY_STAGE_DIR" "$install_dir"
    fi

    if privilege_available; then
        echo "Creating /usr/local/bin/pwsh -> $install_dir/pwsh"
        run_with_privilege ln -sf "$install_dir/pwsh" /usr/local/bin/pwsh
    else
        mkdir -p "$HOME/.local/bin"
        ln -sf "$install_dir/pwsh" "$HOME/.local/bin/pwsh"
        echo "Created $HOME/.local/bin/pwsh"
        echo "Add $HOME/.local/bin to PATH for future shells"
        export PATH="$HOME/.local/bin:$PATH"
        hash -r
    fi
}

if [ "$(get_host_os)" != "linux" ]; then
    echo "ERROR: this helper installs PowerShell only on Linux hosts" >&2
    echo "Install PowerShell manually, then re-run the dependency bootstrap" >&2
    exit 1
fi

INSTALL_DIR="$LOCAL_DIR/powershell-$POWERSHELL_VERSION"
# Recover downloads and interrupted tarball publication before any cached return
begin_dependency_install "$INSTALL_DIR"

if existing_command="$(get_installed_pwsh_command)" && existing_version="$(get_pwsh_version "$existing_command")"; then
    if [ "$existing_version" = "$POWERSHELL_VERSION" ]; then
        echo "Using existing pwsh $existing_version"
        ensure_pwsh_command >/dev/null
        exit 0
    fi

    echo "Existing pwsh is $existing_version; installing pinned PowerShell $POWERSHELL_VERSION"
fi

if [ ! -f /etc/os-release ]; then
    echo "ERROR: /etc/os-release not found; cannot identify distro" >&2
    exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

if [ "$(uname -m)" != x86_64 ]; then
    echo "ERROR: pinned PowerShell bootstrap currently supports Linux x86-64 only" >&2
    exit 1
fi
prepare_dependency_workspace "$INSTALL_DIR"
TMPDIR_LOCAL="$DEPENDENCY_WORK_DIR"
PACKAGE_NAME="$(get_powershell_package_name)"
if [[ "${ID:-}" == "ubuntu" || "${ID:-}" == "debian" || " ${ID_LIKE:-} " == *" ubuntu "* || " ${ID_LIKE:-} " == *" debian "* ]]; then
    install_deb "$PACKAGE_NAME"
elif [[ "${ID:-}" == "fedora" || " ${ID_LIKE:-} " == *" fedora "* || " ${ID_LIKE:-} " == *" rhel "* ]]; then
    install_rpm "$PACKAGE_NAME"
else
    install_tarball
fi

echo "Verifying pwsh"
if ! ensure_pwsh_command; then
    echo "ERROR: neither pwsh nor pwsh-preview is available after installation" >&2
    exit 1
fi

installed_version="$(get_pwsh_version pwsh)"
if [ "$installed_version" != "$POWERSHELL_VERSION" ]; then
    echo "ERROR: PowerShell on PATH is $installed_version, expected $POWERSHELL_VERSION" >&2
    exit 1
fi
echo "$installed_version"
