#!/bin/bash
# Install Linux host packages needed by bootstrap, native builds, and tests
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/platform.sh"

DRY_RUN=0

usage() {
    cat <<'EOF'
Usage: ./get_linux_build_prereqs.sh [--dry-run]

Install Linux host tools used by the Android bootstrap, native desktop builds,
extract helpers, Rust server tests, and input-demo regression tests.

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
    echo "ERROR: this helper only supports Linux hosts" >&2
    exit 1
fi

if [[ ! -f /etc/os-release ]]; then
    echo "ERROR: /etc/os-release not found; cannot identify distro" >&2
    exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

missing_commands=()
for command_name in unzip zip tar xz gzip git python3 cc c++ make cmake cargo rustc; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        missing_commands+=("$command_name")
    fi
done
if ! command -v ninja >/dev/null 2>&1 && ! command -v ninja-build >/dev/null 2>&1; then
    missing_commands+=("ninja")
fi
if ! command -v pkg-config >/dev/null 2>&1 && ! command -v pkgconf >/dev/null 2>&1; then
    missing_commands+=("pkg-config")
fi
if command -v pkg-config >/dev/null 2>&1 || command -v pkgconf >/dev/null 2>&1; then
    pkg_config_cmd="$(command -v pkg-config || command -v pkgconf)"
    for module_name in physfs sdl SDL_mixer libpng glew; do
        if ! "$pkg_config_cmd" --exists "$module_name" >/dev/null 2>&1; then
            missing_commands+=("pkg-config:$module_name")
        fi
    done
else
    missing_commands+=("pkg-config:physfs" "pkg-config:sdl" "pkg-config:SDL_mixer" "pkg-config:libpng" "pkg-config:glew")
fi
if ! command -v curl >/dev/null 2>&1 && ! command -v wget >/dev/null 2>&1; then
    missing_commands+=("curl-or-wget")
fi
if command -v python3 >/dev/null 2>&1 && ! python3 -c 'import venv, ensurepip' >/dev/null 2>&1; then
    missing_commands+=("python3-venv")
fi

if [[ "${#missing_commands[@]}" -eq 0 ]]; then
    echo "Linux bootstrap/build/test prerequisites already present"
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
    elif [[ "$DRY_RUN" -eq 1 ]] && command -v sudo >/dev/null 2>&1; then
        run_install sudo "$@"
    elif command -v sudo >/dev/null 2>&1 && { [[ -t 0 ]] || sudo -n true 2>/dev/null; }; then
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
        build-essential
        cmake
        ninja-build
        pkgconf
        cargo
        rustc
        libphysfs-dev
        libsdl1.2-dev
        libsdl-mixer1.2-dev
        libpng-dev
        libglew-dev
        libgl1-mesa-dev
        libglu1-mesa-dev
        zlib1g-dev
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
        make
        gcc
        gcc-c++
        cmake
        ninja-build
        pkgconf-pkg-config
        cargo
        rust
        physfs-devel
        SDL-devel
        SDL_mixer-devel
        libpng-devel
        glew-devel
        mesa-libGL-devel
        mesa-libGLU-devel
        zlib-devel
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
        base-devel
        cmake
        ninja
        pkgconf
        rust
        physfs
        sdl12-compat
        sdl_mixer
        libpng
        glew
        glu
        mesa
        zlib
    )
    install_with_privilege pacman -S --needed --noconfirm "${packages[@]}"
    exit 0
fi

echo "ERROR: unsupported Linux distro family" >&2
echo "Detected ID=${ID:-unknown} ID_LIKE=${ID_LIKE:-unknown}" >&2
exit 1
