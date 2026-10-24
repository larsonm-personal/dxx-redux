#!/usr/bin/env bash
# Compatibility entry point for public installs and updates
set -euo pipefail
exec bash "$(dirname -- "$(readlink -f -- "$0")")/install.sh" "$@"
