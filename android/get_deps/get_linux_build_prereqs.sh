#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/platform.sh"

DRY_RUN=0

usage() {
    cat <<'EOF'
Usage: ./get_linux_build_prereqs.sh [--dry-run]

Install the native Linux desktop build prerequisites for the repo.

Supported distro families:
  - Debian / Ubuntu
  - Fedora
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
    echo "ERROR: this helper only supports Linux hosts" >&2
    exit 1
fi

if [[ ! -f /etc/os-release ]]; then
    echo "ERROR: /etc/os-release not found; cannot identify distro" >&2
    exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

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
    else
        run_install sudo "$@"
    fi
}

if [[ "${ID:-}" == "ubuntu" || "${ID:-}" == "debian" || " ${ID_LIKE:-} " == *" ubuntu "* || " ${ID_LIKE:-} " == *" debian "* ]]; then
    packages=(
        build-essential
        git
        cmake
        ninja-build
        pkgconf
        libphysfs-dev
        libsdl1.2-dev
        libsdl-mixer1.2-dev
        libpng-dev
        libglew-dev
    )
    install_with_privilege apt update
    install_with_privilege apt install -y "${packages[@]}"
    exit 0
fi

if [[ "${ID:-}" == "fedora" || " ${ID_LIKE:-} " == *" fedora "* || " ${ID_LIKE:-} " == *" rhel "* ]]; then
    packages=(
        make
        gcc-c++
        git
        cmake
        ninja-build
        pkgconf-pkg-config
        physfs-devel
        sdl12-compat-devel
        SDL_mixer-devel
        libpng-devel
        glew-devel
    )
    install_with_privilege dnf install -y "${packages[@]}"
    exit 0
fi

if [[ "${ID:-}" == "arch" || " ${ID_LIKE:-} " == *" arch "* ]]; then
    packages=(
        base-devel
        git
        cmake
        ninja
        pkgconf
        physfs
        sdl12-compat
        sdl_mixer
        libpng
        glew
    )
    install_with_privilege pacman -S --needed --noconfirm "${packages[@]}"
    exit 0
fi

echo "ERROR: unsupported Linux distro family" >&2
echo "Detected ID=${ID:-unknown} ID_LIKE=${ID_LIKE:-unknown}" >&2
echo "See README.md for the current manual package lists" >&2
exit 1