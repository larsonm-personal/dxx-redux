#!/bin/bash
# Install Linux host packages needed by android/get_deps bootstrap scripts
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/platform.sh"

DRY_RUN=0

usage() {
    cat <<'EOF'
Usage: ./get_linux_android_prereqs.sh [--dry-run]

Install small host tools used by the Android dependency bootstrap scripts

Supported distro families:
  - Debian / Ubuntu
  - Fedora / RHEL
  - Arch Linux

Options:
  --dry-run   Print the install command without running it
  -h, --help  Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --dry-run)
        DRY_RUN=1
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

if [[ "$(get_host_os)" != "linux" ]]; then
    echo "Nothing to install: host is not Linux"
    exit 0
fi

if [[ ! -f /etc/os-release ]]; then
    echo "ERROR: /etc/os-release not found; cannot identify distro" >&2
    exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

missing_commands=()
for command_name in unzip zip tar xz gzip git python3; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        missing_commands+=("$command_name")
    fi
done
if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
    missing_commands+=("curl-or-wget")
fi
if command -v python3 >/dev/null 2>&1 && ! python3 -c 'import venv, ensurepip' >/dev/null 2>&1; then
    missing_commands+=("python3-venv")
fi

if [[ "${#missing_commands[@]}" -eq 0 ]]; then
    echo "Linux Android bootstrap prerequisites already present"
    exit 0
fi

printf 'Missing prerequisite commands/features:'
printf ' %s' "${missing_commands[@]}"
printf '\n'

run_install() {
    local -a cmd=("$@")

    printf 'Resolved install command:'
    printf ' %q' "${cmd[@]}"
    printf '\n'

    if [[ "$DRY_RUN" -eq 1 ]]; then
        return 0
    fi

    "${cmd[@]}"
}

install_with_privilege() {
    if [[ $EUID -eq 0 ]]; then
        run_install "$@"
    elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
        run_install sudo "$@"
    else
        echo "ERROR: missing host prerequisites and sudo is not available non-interactively" >&2
        echo "Run this helper from a terminal with sudo, or install the listed packages manually" >&2
        exit 1
    fi
}

if [[ "${ID:-}" == "ubuntu" || "${ID:-}" == "debian" || " ${ID_LIKE:-} " == *" ubuntu "* || " ${ID_LIKE:-} " == *" debian "* ]]; then
    packages=(
        ca-certificates
        curl
        wget
        unzip
        zip
        tar
        xz-utils
        gzip
        git
        python3
        python3-venv
    )
    install_with_privilege apt update
    install_with_privilege apt install -y "${packages[@]}"
    exit 0
fi

if [[ "${ID:-}" == "fedora" || " ${ID_LIKE:-} " == *" fedora "* || " ${ID_LIKE:-} " == *" rhel "* ]]; then
    packages=(
        ca-certificates
        curl
        wget
        unzip
        zip
        tar
        xz
        gzip
        git
        python3
        python3-pip
    )
    install_with_privilege dnf install -y "${packages[@]}"
    exit 0
fi

if [[ "${ID:-}" == "arch" || " ${ID_LIKE:-} " == *" arch "* ]]; then
    packages=(
        ca-certificates
        curl
        wget
        unzip
        zip
        tar
        xz
        gzip
        git
        python
    )
    install_with_privilege pacman -S --needed --noconfirm "${packages[@]}"
    exit 0
fi

echo "ERROR: unsupported Linux distro family" >&2
echo "Detected ID=${ID:-unknown} ID_LIKE=${ID_LIKE:-unknown}" >&2
exit 1
