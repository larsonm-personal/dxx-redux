#!/bin/bash
# Install or update the user Rust toolchain used by server/test helpers.
set -euo pipefail

MIN_CARGO_VERSION="${MIN_CARGO_VERSION:-1.78.0}"

version_ge() {
    local have="$1" need="$2"
    printf '%s\n%s\n' "$need" "$have" | sort -V -C
}

get_cargo_version() {
    if ! command -v cargo >/dev/null 2>&1; then
        return 1
    fi
    cargo --version | awk '{print $2}'
}

install_or_update_rustup() {
    if command -v rustup >/dev/null 2>&1; then
        rustup update stable
    else
        if command -v curl >/dev/null 2>&1; then
            curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --default-toolchain stable
        elif command -v wget >/dev/null 2>&1; then
            wget -qO- https://sh.rustup.rs | sh -s -- -y --default-toolchain stable
        else
            echo "ERROR: curl or wget is required to install rustup" >&2
            exit 1
        fi
    fi
}

toolchain_usable() {
    cargo --version >/dev/null 2>&1 && rustc --version >/dev/null 2>&1
}

if [ -f "$HOME/.cargo/env" ]; then
    # shellcheck disable=SC1091
    source "$HOME/.cargo/env"
fi

current_version="$(get_cargo_version || true)"
if [ -n "$current_version" ] && version_ge "$current_version" "$MIN_CARGO_VERSION" && toolchain_usable; then
    echo "Rust toolchain already usable: cargo $current_version"
    exit 0
fi

if [ -n "$current_version" ]; then
    echo "Installed cargo $current_version is too old; installing/updating rustup stable"
else
    echo "Cargo not found; installing rustup stable"
fi

install_or_update_rustup

if [ -f "$HOME/.cargo/env" ]; then
    # shellcheck disable=SC1091
    source "$HOME/.cargo/env"
fi

if ! rustup default stable; then
    rustup toolchain uninstall stable >/dev/null 2>&1 || true
    rustup toolchain install stable
    rustup default stable
fi
cargo --version
rustc --version
