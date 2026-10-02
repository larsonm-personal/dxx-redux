#!/usr/bin/env bash
# Run Linux installer fixtures on a native filesystem, away from Windows tool destinations
set -euo pipefail

linux_pwsh="$1"
runner="$2"
shift 2
repo_root="$(cd "$(dirname "$runner")/../.." && pwd)"
workspace="$(mktemp -d /tmp/dxx-dependency-fixtures.XXXXXX)"

cleanup() {
    local status="$1" resolved
    resolved="$(readlink -f "$workspace")"
    if [[ "$resolved" == /tmp/dxx-dependency-fixtures.* && "$(dirname "$resolved")" == /tmp ]]; then
        rm -rf -- "$resolved"
    else
        echo "Unexpected Linux fixture workspace: $resolved" >&2
        exit 1
    fi
    exit "$status"
}
trap 'cleanup "$?"' EXIT

mkdir -p "$workspace/android/tests"
cp -R "$repo_root/android/get_deps" "$workspace/android/get_deps"
cp -R "$repo_root/android/helpers" "$workspace/android/helpers"
cp "$runner" "$workspace/android/tests/test_dependency_install.ps1"
for fixture in "$@"; do
    cp "$repo_root/android/tests/$fixture" "$workspace/android/tests/$fixture"
done
export PATH="${linux_pwsh%/*}:$PATH"
"$linux_pwsh" -NoProfile -NonInteractive -File "$workspace/android/tests/test_dependency_install.ps1"
