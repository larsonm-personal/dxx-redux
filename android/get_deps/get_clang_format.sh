#!/bin/bash
# get_clang_format.sh -- Download a pre-built clang-format binary if not present.
# Reads version from tool_versions.conf.
# Source: https://github.com/muttleyxd/clang-tools-static-binaries
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/clang-format-$CLANG_FORMAT_VERSION"

if [ -f "$DEST/clang-format.exe" ] || [ -f "$DEST/clang-format" ]; then
    echo "clang-format $CLANG_FORMAT_VERSION already installed at $DEST"
    exit 0
fi

# Pick the right binary name for the current platform.
# When running in WSL but targeting a Windows filesystem (/mnt/), use Windows binary.
_is_windows_target() {
    case "$DEST" in
        /mnt/[a-z]/*) return 0 ;;  # WSL path to Windows drive
    esac
    case "$(uname -s)" in
        MINGW*|MSYS*|CYGWIN*|*_NT*) return 0 ;;
    esac
    return 1
}

if _is_windows_target; then
    ASSET_NAME="clang-format-${CLANG_FORMAT_VERSION}_windows-amd64.exe"
    DEST_NAME="clang-format.exe"
elif [ "$(uname -s)" = "Darwin" ]; then
    ASSET_NAME="clang-format-${CLANG_FORMAT_VERSION}_macosx-amd64"
    DEST_NAME="clang-format"
else
    ASSET_NAME="clang-format-${CLANG_FORMAT_VERSION}_linux-amd64"
    DEST_NAME="clang-format"
fi

URL="https://github.com/muttleyxd/clang-tools-static-binaries/releases/download/${CLANG_FORMAT_RELEASE_TAG}/${ASSET_NAME}"

echo "Downloading clang-format $CLANG_FORMAT_VERSION..."
echo "  URL: $URL"

mkdir -p "$DEST"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" clang-format-XXXXXX)"

curl -fSL --progress-bar -o "$TMPFILE" "$URL"
mv "$TMPFILE" "$DEST/$DEST_NAME"
chmod +x "$DEST/$DEST_NAME"

echo "clang-format $CLANG_FORMAT_VERSION installed at $DEST/$DEST_NAME"
"$DEST/$DEST_NAME" --version

if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
