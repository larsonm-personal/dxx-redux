#!/usr/bin/env bash
# Select exact releases in ../rust-toolchain.toml and rust_tools.conf before running
# Discover candidates from the official Rust manifests and crates.io, then review
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./rust_tools.conf
export PATH="${CARGO_HOME:-$HOME/.cargo}/bin:$PATH"
bash ../android/get_deps/helpers/get_rust.sh
rustup toolchain install "$UDEPS_TOOLCHAIN" --profile minimal --no-self-update
cargo install cargo-udeps --version "=$CARGO_UDEPS_VERSION" --locked
cargo install cargo-edit --version "=$CARGO_EDIT_VERSION" --locked
cargo install bindgen-cli --version "=$BINDGEN_VERSION" --locked

# Replace inventory only after all installs and version checks succeed
inventory="$(mktemp)"
trap 'rm -f "$inventory"' EXIT
{
    rustc --version
    rustup --version
    cargo --version
    cargo clippy --version
    cargo fmt --version
    cargo udeps --version
    cargo upgrade --version
    bindgen --version
    rustup run "$UDEPS_TOOLCHAIN" rustc --version
    printf 'udeps toolchain %s\n' "$UDEPS_TOOLCHAIN"
} >"$inventory"
cp "$inventory" rust_version.txt
