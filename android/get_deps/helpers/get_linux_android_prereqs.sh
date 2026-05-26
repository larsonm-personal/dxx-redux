#!/bin/bash
# Compatibility wrapper. Prefer get_linux_build_prereqs.sh for Linux hosts
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec bash "$SCRIPT_DIR/get_linux_build_prereqs.sh" "$@"
