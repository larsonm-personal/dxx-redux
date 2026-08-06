#!/bin/bash
# Verify that dependency bootstrap temporary paths work with BSD and GNU mktemp
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
# shellcheck disable=SC1091
source "$REPO_ROOT/android/get_deps/helpers/platform.sh"

TEST_ROOT="$REPO_ROOT/android/temp/dependency temp test.$$"
TMPDIR="$TEST_ROOT/default temp"
CUSTOM_PARENT="$TEST_ROOT/custom stage"
mkdir -p "$TMPDIR" "$CUSTOM_PARENT"
trap 'rm -rf "$TEST_ROOT"' EXIT

first="$(create_temp_file repeated)"
second="$(create_temp_file repeated)"
directory="$(create_temp_dir extracted)"
[ "$first" != "$second" ]
[ -f "$first" ]
[ -f "$second" ]
[ -d "$directory" ]

# Model stock macOS mktemp: no GNU -p and the X run must end the template
mktemp() {
    local make_directory=0
    local template
    local output

    if [ "${1:-}" = "-d" ]; then
        make_directory=1
        shift
    fi
    if [ "$#" -ne 1 ]; then
        echo "BSD mktemp fixture received unsupported arguments: $*" >&2
        return 64
    fi
    template="$1"
    case "$template" in
    *XXXXXX) ;;
    *)
        echo "BSD mktemp fixture requires a final XXXXXX run: $template" >&2
        return 64
        ;;
    esac
    output="${template%XXXXXX}ABC123"
    if [ "$make_directory" -eq 1 ]; then
        (umask 077 && mkdir "$output")
    else
        (umask 077 && : >"$output")
    fi
    printf '%s\n' "$output"
}

mac_file="$(create_temp_file sdk.zip)"
mac_directory="$(create_temp_dir sdk-extract)"
staged_directory="$(create_temp_dir .jdk-21-stage "$CUSTOM_PARENT")"
[ "$mac_file" = "$TMPDIR/sdk.zip.ABC123" ]
[ "$mac_directory" = "$TMPDIR/sdk-extract.ABC123" ]
[ "$staged_directory" = "$CUSTOM_PARENT/.jdk-21-stage.ABC123" ]
[ -f "$mac_file" ]
[ -d "$mac_directory" ]
[ -d "$staged_directory" ]

for helper in "$REPO_ROOT"/android/get_deps/helpers/*.sh; do
    if [ "$helper" != "$REPO_ROOT/android/get_deps/helpers/platform.sh" ] && grep -q 'mktemp' "$helper"; then
        echo "Dependency helper bypasses portable temporary creation: $helper" >&2
        exit 1
    fi
done

echo "PASS: dependency temporary files use portable final-X templates"
