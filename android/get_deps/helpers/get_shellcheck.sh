#!/bin/bash
# get_shellcheck.sh -- Download a pre-built shellcheck binary if not present.
# Reads version from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/shellcheck-$SHELLCHECK_VERSION"

if [ -f "$DEST/shellcheck.exe" ] || [ -f "$DEST/shellcheck" ]; then
    echo "shellcheck $SHELLCHECK_VERSION already installed at $DEST"
    exit 0
fi

# Pick URL for the current platform
_is_windows_target() {
    case "$DEST" in
    /mnt/[a-z]/*) return 0 ;;
    esac
    case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN* | *_NT*) return 0 ;;
    esac
    return 1
}

if _is_windows_target; then
    URL="https://github.com/koalaman/shellcheck/releases/download/v${SHELLCHECK_VERSION}/shellcheck-v${SHELLCHECK_VERSION}.zip"
    echo "Downloading shellcheck $SHELLCHECK_VERSION..."
    echo "  URL: $URL"
    mkdir -p "$DEST"
    TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" shellcheck-XXXXXX.zip)"
    download_file "$TMPFILE" "$URL"
    TMPDIR2="$(mktemp -d -p "${TMPDIR:-/tmp}" shellcheck-extract-XXXXXX)"
    unzip -q "$TMPFILE" -d "$TMPDIR2"
    mv "$TMPDIR2/shellcheck.exe" "$DEST/shellcheck.exe"
    rm -rf "$TMPFILE" "$TMPDIR2"
elif [ "$(uname -s)" = "Darwin" ]; then
    URL="https://github.com/koalaman/shellcheck/releases/download/v${SHELLCHECK_VERSION}/shellcheck-v${SHELLCHECK_VERSION}.darwin.x86_64.tar.xz"
    echo "Downloading shellcheck $SHELLCHECK_VERSION..."
    echo "  URL: $URL"
    mkdir -p "$DEST"
    TMPFILE="$(mktemp shellcheck-XXXXXX.tar.xz)"
    download_file "$TMPFILE" "$URL"
    tar -xJf "$TMPFILE" -C "$DEST" --strip-components=1 "shellcheck-v${SHELLCHECK_VERSION}/shellcheck"
    rm -f "$TMPFILE"
else
    URL="https://github.com/koalaman/shellcheck/releases/download/v${SHELLCHECK_VERSION}/shellcheck-v${SHELLCHECK_VERSION}.linux.x86_64.tar.xz"
    echo "Downloading shellcheck $SHELLCHECK_VERSION..."
    echo "  URL: $URL"
    mkdir -p "$DEST"
    TMPFILE="$(mktemp shellcheck-XXXXXX.tar.xz)"
    download_file "$TMPFILE" "$URL"
    tar -xJf "$TMPFILE" -C "$DEST" --strip-components=1 "shellcheck-v${SHELLCHECK_VERSION}/shellcheck"
    rm -f "$TMPFILE"
fi

chmod +x "$DEST/shellcheck"* 2>/dev/null || true
echo "shellcheck $SHELLCHECK_VERSION installed at $DEST"
