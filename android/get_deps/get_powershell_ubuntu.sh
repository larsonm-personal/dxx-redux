#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"

if ! command -v lsb_release >/dev/null 2>&1; then
    echo "ERROR: lsb_release not found" >&2
    echo "Install lsb-release and re-run" >&2
    exit 1
fi

DISTRO="$(lsb_release -is 2>/dev/null || true)"
if [ "$DISTRO" != "Ubuntu" ]; then
    echo "ERROR: this helper only supports Ubuntu" >&2
    echo "Detected: ${DISTRO:-unknown}" >&2
    exit 1
fi

VERSION="$(lsb_release -rs)"
TMPDIR_LOCAL="$(mktemp -d -p "${TMPDIR:-/tmp}" pwsh-bootstrap-XXXXXX)"
DEB_FILE="$TMPDIR_LOCAL/packages-microsoft-prod.deb"
PWSH_DEB="$TMPDIR_LOCAL/powershell.deb"
trap 'rm -rf "$TMPDIR_LOCAL"' EXIT

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
        sudo ln -sf "$(command -v pwsh-preview)" /usr/local/bin/pwsh
        hash -r
        return 0
    fi

    return 1
}

find_github_powershell_deb_url() {
    local tag="v$POWERSHELL_VERSION"
    local api_url="https://api.github.com/repos/PowerShell/PowerShell/releases/tags/$tag"
    local package_name
    package_name="$(get_powershell_package_name)"
    local release_json
    release_json="$(download_text "$api_url" 'Accept: application/vnd.github+json' 'User-Agent: dxx-redux-get-powershell')"
    echo "$release_json" \
        | grep -o '"browser_download_url": *"[^"]*\.deb_amd64\.deb"' \
        | sed 's/"browser_download_url": *"//;s/"$//' \
        | grep "/${package_name}_" \
        | head -1
}

echo "Updating apt metadata"
sudo apt update

echo "Installing prerequisite packages"
sudo apt install -y wget apt-transport-https software-properties-common

echo "Downloading Microsoft package registration for Ubuntu $VERSION"
wget -q "https://packages.microsoft.com/config/ubuntu/${VERSION}/packages-microsoft-prod.deb" -O "$DEB_FILE"

echo "Registering Microsoft package feed"
sudo dpkg -i "$DEB_FILE"

echo "Refreshing apt metadata"
sudo apt update

echo "Installing PowerShell"

PACKAGE_NAME="$(get_powershell_package_name)"
if apt-cache show "$PACKAGE_NAME" >/dev/null 2>&1; then
    sudo apt install -y "$PACKAGE_NAME"
else
    echo "No apt package '$PACKAGE_NAME' is currently published for Ubuntu $VERSION" >&2
    echo "Falling back to the pinned GitHub .deb for PowerShell $POWERSHELL_VERSION"

    PWSH_URL="$(find_github_powershell_deb_url)"
    if [ -z "$PWSH_URL" ]; then
        echo "ERROR: could not find a .deb asset for PowerShell $POWERSHELL_VERSION" >&2
        exit 1
    fi

    download_file "$PWSH_DEB" "$PWSH_URL"
    sudo apt install -y "$PWSH_DEB"
fi

echo "Verifying pwsh"
if ! ensure_pwsh_command; then
    echo "ERROR: neither pwsh nor pwsh-preview is available after installation" >&2
    exit 1
fi

# shellcheck disable=SC2016
pwsh -NoProfile -Command '$PSVersionTable.PSVersion.ToString()'
