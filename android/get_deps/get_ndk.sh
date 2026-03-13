#!/bin/bash
# get_ndk.sh - Download Android NDK if not present.
# Reads version/URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/android-ndk-$NDK_VERSION"

if [ -f "$DEST/build/cmake/android.toolchain.cmake" ]; then
    echo "Android NDK $NDK_VERSION already installed at $DEST"
    exit 0
fi

URL="$NDK_URL"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" ndk-XXXXXX.zip)"

echo "Downloading Android NDK $NDK_VERSION (~1.1 GB)..."
curl -fSL --progress-bar -o "$TMPFILE" "$URL"

echo "Extracting to $INSTALL_DIR..."
unzip -q -o "$TMPFILE" -d "$INSTALL_DIR"

rm -f "$TMPFILE"

if [ -f "$DEST/build/cmake/android.toolchain.cmake" ]; then
    echo "Android NDK $NDK_VERSION installed at $DEST"
else
    echo "ERROR: extraction succeeded but toolchain file not found at expected path."
    if [ -z "$GET_ALL_RUNNING" ]; then
        echo ""
        echo "Press any key to exit..."
        read -r -n1 -s
    fi
    exit 1
fi
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
