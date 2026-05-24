#!/bin/bash
# get_shfmt.sh -- Download a pre-built shfmt binary if not present.
# Reads version from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/shfmt-$SHFMT_VERSION"

if [ -f "$DEST/shfmt.exe" ] || [ -f "$DEST/shfmt" ]; then
    echo "shfmt $SHFMT_VERSION already installed at $DEST"
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
    URL="https://github.com/mvdan/sh/releases/download/v${SHFMT_VERSION}/shfmt_v${SHFMT_VERSION}_windows_amd64.exe"
    DEST_NAME="shfmt.exe"
elif [ "$(uname -s)" = "Darwin" ]; then
    URL="https://github.com/mvdan/sh/releases/download/v${SHFMT_VERSION}/shfmt_v${SHFMT_VERSION}_darwin_amd64"
    DEST_NAME="shfmt"
else
    URL="https://github.com/mvdan/sh/releases/download/v${SHFMT_VERSION}/shfmt_v${SHFMT_VERSION}_linux_amd64"
    DEST_NAME="shfmt"
fi

echo "Downloading shfmt $SHFMT_VERSION..."
echo "  URL: $URL"

mkdir -p "$DEST"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" shfmt-XXXXXX)"
download_file "$TMPFILE" "$URL"
mv "$TMPFILE" "$DEST/$DEST_NAME"
chmod +x "$DEST/$DEST_NAME" 2>/dev/null || true
echo "shfmt $SHFMT_VERSION installed at $DEST"
