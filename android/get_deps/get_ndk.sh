#!/bin/bash
# get_ndk.sh — Download Android NDK r27d to C:/local/android-ndk-r27d if not present.
set -e

INSTALL_DIR="/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="/mnt/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="C:/local"

DEST="$INSTALL_DIR/android-ndk-r27d"

if [ -f "$DEST/build/cmake/android.toolchain.cmake" ]; then
    echo "Android NDK r27d already installed at $DEST"
    exit 0
fi

URL="https://dl.google.com/android/repository/android-ndk-r27d-windows.zip"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" ndk-XXXXXX.zip)"

echo "Downloading Android NDK r27d (~1.1 GB)..."
curl -fSL --progress-bar -o "$TMPFILE" "$URL"

echo "Extracting to $INSTALL_DIR..."
unzip -q -o "$TMPFILE" -d "$INSTALL_DIR"

rm -f "$TMPFILE"

if [ -f "$DEST/build/cmake/android.toolchain.cmake" ]; then
    echo "Android NDK r27d installed at $DEST"
else
    echo "ERROR: extraction succeeded but toolchain file not found at expected path."
    exit 1
fi
