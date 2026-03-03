#!/bin/bash
# get_sdk.sh — Download and install Android SDK command-line tools to C:/local/android-sdk if not present.
# After running this, run finalize.sh to accept licenses and install platform packages.
set -e

INSTALL_DIR="/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="/mnt/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="C:/local"

DEST="$INSTALL_DIR/android-sdk"

if [ -x "$DEST/cmdline-tools/latest/bin/sdkmanager" ] || [ -f "$DEST/cmdline-tools/latest/bin/sdkmanager.bat" ]; then
    echo "Android SDK already installed at $DEST"
    exit 0
fi

# Google's latest command-line tools (Windows)
URL="https://dl.google.com/android/repository/commandlinetools-win-11076708_latest.zip"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" sdk-XXXXXX.zip)"

echo "Downloading Android SDK command-line tools..."
curl -fSL --progress-bar -o "$TMPFILE" "$URL"

echo "Extracting to $DEST..."
mkdir -p "$DEST"
unzip -q -o "$TMPFILE" -d "$DEST"

# The zip extracts as cmdline-tools/; move into the "latest" layout
if [ -d "$DEST/cmdline-tools" ] && [ ! -d "$DEST/cmdline-tools/latest" ]; then
    mv "$DEST/cmdline-tools" "$DEST/cmdline-tools-temp"
    mkdir -p "$DEST/cmdline-tools/latest"
    mv "$DEST/cmdline-tools-temp"/* "$DEST/cmdline-tools/latest/"
    rmdir "$DEST/cmdline-tools-temp"
fi

rm -f "$TMPFILE"
echo "Android SDK command-line tools installed at $DEST"
echo "Run finalize.sh next to accept licenses and install platform packages."
