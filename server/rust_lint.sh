#!/usr/bin/env bash

# Exit trap: always pause unless called from parent script
trap 'if [[ -z "$SKIP_PAUSE" ]]; then echo ""; echo "Press any key to exit..."; read -r -n1 -s; fi' EXIT

set -e
cd -- "$(dirname -- "$(readlink -f -- "$0")")" # change to script dir

cargo fmt
cargo fix --locked --allow-dirty
cargo clippy --locked --fix --allow-dirty

cargo build --locked
cargo test --locked

read -r -p "run yolo fixes? (y/N): " answer
if [[ $answer == [Yy] ]]; then
    __CARGO_FIX_YOLO=1 cargo clippy --locked --fix --allow-dirty
fi
