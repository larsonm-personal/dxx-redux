#!/usr/bin/env bash
# Service-only invocations must not bypass production configuration/verification
set -euo pipefail
exec bash "$(dirname -- "$(readlink -f -- "$0")")/install.sh" "$@"
