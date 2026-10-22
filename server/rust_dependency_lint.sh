#!/usr/bin/env bash
# Explicit upgrade operation; --pinned retains leading '=' requirements
# Pass cargo-upgrade options such as --dry-run or --incompatible deliberately
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
source ./rust_tools.conf
python3 check_exact_dependencies.py
cargo upgrade --pinned "$@"
python3 check_exact_dependencies.py
# A dry run must leave the lockfile and build tree alone
for arg in "$@"; do [[ "$arg" != --dry-run ]] || exit 0; done
cargo update
cargo +"$UDEPS_TOOLCHAIN" udeps --all-targets --locked
