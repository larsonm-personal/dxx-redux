#!/bin/bash
# get_sdk.sh - Download and install Android SDK command-line tools if not present.
# Reads URL from tool_versions.conf.
# After running this, run finalize.sh to accept licenses and install platform packages.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/android-sdk"

if [ -x "$DEST/cmdline-tools/latest/bin/sdkmanager" ] || [ -f "$DEST/cmdline-tools/latest/bin/sdkmanager.bat" ]; then
    echo "Android SDK already installed at $DEST"
    exit 0
fi

URL="$SDK_CMDLINE_TOOLS_URL"
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
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
