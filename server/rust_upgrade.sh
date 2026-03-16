#!/usr/bin/env bash

# Exit trap: always pause unless called from parent script
trap 'if [[ -z "$SKIP_PAUSE" ]]; then echo ""; echo "Press any key to exit..."; read -r -n1 -s; fi' EXIT

set -e
cd -- "$(dirname -- "$(readlink -f -- "$0")")"    # change to script dir

rustup update stable
rustup toolchain install nightly
cargo install cargo-udeps --locked #"as of rust 1.41, will do an update if available"
cargo install cargo-edit
cargo install bindgen-cli
rustup component add clippy

# print versions to a file
rustc --version > rust_version.txt  # > overwrite
rustup --version >> rust_version.txt # this also prints comments but they don't get put into the file
cargo --version >> rust_version.txt # >> append
cargo clippy --version >> rust_version.txt
cargo udeps --version >> rust_version.txt
cargo upgrade --version >> rust_version.txt
bindgen --version >> rust_version.txt

