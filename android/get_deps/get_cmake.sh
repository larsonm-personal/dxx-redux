#!/bin/bash
# get_cmake.sh - Download and install CMake if not present.
# Reads version/URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

CMAKE_DIR_NAME="cmake-$CMAKE_VERSION"
INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/$CMAKE_DIR_NAME"

if [ -d "$DEST" ] && [ -x "$DEST/bin/cmake" -o -x "$DEST/bin/cmake.exe" ]; then
    echo "CMake $CMAKE_VERSION already installed at $DEST"
    "$DEST/bin/cmake" --version | head -1
    exit 0
fi

URL="$CMAKE_URL"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" cmake-XXXXXX.zip)"

echo "Downloading CMake $CMAKE_VERSION..."
curl -fSL --progress-bar -o "$TMPFILE" "$URL"

echo "Extracting to $DEST..."
# The zip contains a folder like cmake-3.31.6-windows-x86_64/ at the top level
unzip -q -o "$TMPFILE" -d "$INSTALL_DIR"
# Rename the extracted folder to the canonical name.
if [ ! -d "$DEST" ]; then
    for _d in "$INSTALL_DIR"/cmake-${CMAKE_VERSION}*; do
        if [ -d "$_d" ]; then
            mv "$_d" "$DEST"
            break
        fi
    done
fi

rm -f "$TMPFILE"
echo "CMake $CMAKE_VERSION installed at $DEST"
"$DEST/bin/cmake" --version | head -1
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi

