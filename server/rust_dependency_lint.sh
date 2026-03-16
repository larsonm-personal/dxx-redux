#!/usr/bin/env bash

# Exit trap: always pause unless called from parent script
trap 'if [[ -z "$SKIP_PAUSE" ]]; then echo ""; echo "Press any key to exit..."; read -r -n1 -s; fi' EXIT

set -e
cd -- "$(dirname -- "$(readlink -f -- "$0")")"    # change to script dir

read -p "Fix cargo registry for 'cargo upgrade'? (y/N): " answer
if [[ $answer == [Yy] ]]; then
    # see https://github.com/killercup/cargo-edit/issues/879#issuecomment-1826103193
    # as of June 2025 this isn't needed anymore but I can't tell if it's fully resolved
    echo "Fixing cargo registry - this needs to download non-sparse copies of everything, it's a few gigabytes"
    rm -rf ~/.cargo/registry/
    CARGO_REGISTRIES_CRATES_IO_PROTOCOL=git cargo fetch
else
    echo "Skipping cargo registry fix"
fi

rm -f ./.Cargo.lock
CARGO_REGISTRIES_CRATES_IO_PROTOCOL=git cargo upgrade --pinned
cargo +nightly udeps --all-targets
