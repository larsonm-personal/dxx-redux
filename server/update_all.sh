#!/usr/bin/env bash

# Exit trap: show pause when this script exits (error or success)
trap 'echo ""; echo "Press any key to exit..."; read -r -n1 -s' EXIT

set -e
cd -- "$(dirname -- "$(readlink -f -- "$0")")"    # change to script dir

# Disable individual script pauses; rely on our trap instead
export SKIP_PAUSE=1

./rust_upgrade.sh
./rust_lint.sh
./rust_dependency_lint.sh

echo ""
echo "All upgrades and linting complete!"
