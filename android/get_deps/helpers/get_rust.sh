#!/usr/bin/env bash
# Install the declared Rust toolchain, never discover new releases during a build
set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
# shellcheck source=../../../server/rust_tools.conf
source "$REPO_ROOT/server/rust_tools.conf"
: "${RUSTUP_VERSION:?Missing rustup pin}"
RUST_VERSION="$(sed -n 's/^channel = "\([0-9][0-9.]*\)"$/\1/p' "$REPO_ROOT/rust-toolchain.toml")"
[[ "$RUST_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Invalid Rust pin" >&2
    exit 1
}

CARGO_BIN="${CARGO_HOME:-$HOME/.cargo}/bin"
export PATH="$CARGO_BIN:$PATH"
if ! command -v rustup >/dev/null 2>&1 || [[ "$(rustup --version 2>/dev/null | awk '{print $2}')" != "$RUSTUP_VERSION" ]]; then
    installer=rustup-init
    case "$(uname -s):$(uname -m)" in
    Linux:x86_64) host=x86_64-unknown-linux-gnu ;;
    Linux:aarch64) host=aarch64-unknown-linux-gnu ;;
    Darwin:x86_64) host=x86_64-apple-darwin ;;
    Darwin:arm64) host=aarch64-apple-darwin ;;
    MINGW*:x86_64 | MSYS*:x86_64 | CYGWIN*:x86_64)
        host=x86_64-pc-windows-msvc
        installer=rustup-init.exe
        ;;
    *)
        echo "Install rustup $RUSTUP_VERSION for this platform before continuing" >&2
        exit 1
        ;;
    esac
    installer_dir="$(mktemp -d)"
    trap 'rm -rf "$installer_dir"' EXIT
    url="https://static.rust-lang.org/rustup/archive/$RUSTUP_VERSION/$host/$installer"
    curl --proto '=https' --tlsv1.2 -fsSL "$url" -o "$installer_dir/$installer"
    curl --proto '=https' --tlsv1.2 -fsSL "$url.sha256" -o "$installer_dir/$installer.sha256"
    checksum="$(awk 'NR == 1 { print $1 }' "$installer_dir/$installer.sha256")"
    [[ "$checksum" =~ ^[0-9a-fA-F]{64}$ ]] || {
        echo "Invalid rustup checksum" >&2
        exit 1
    }
    # Some platform checksum files contain only the digest, without a filename
    printf '%s  %s\n' "$checksum" "$installer" >"$installer_dir/checksum.txt"
    (cd "$installer_dir" && if command -v sha256sum >/dev/null 2>&1; then sha256sum -c checksum.txt; else shasum -a 256 -c checksum.txt; fi)
    chmod +x "$installer_dir/$installer"
    "$installer_dir/$installer" -y --no-modify-path --profile minimal --default-toolchain "$RUST_VERSION"
fi
rustup toolchain install "$RUST_VERSION" --profile minimal --component clippy,rustfmt --no-self-update
rustup default "$RUST_VERSION"
rustup run "$RUST_VERSION" rustc --version
rustup run "$RUST_VERSION" cargo --version
