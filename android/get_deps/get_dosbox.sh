#!/bin/bash
# get_dosbox.sh — Download and install DOSBox-X to C:/local/ if not present.
# Reads version from tool_versions.conf.
# DOSBox-X is used to run DOS shareware demo installers for file extraction.
# Releases: https://github.com/joncampbell123/dosbox-x/releases
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"

INSTALL_DIR="/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="/mnt/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="C:/local"

DEST="$INSTALL_DIR/$DOSBOX_DIR_NAME"


if [ -d "$DEST" ] && [ -f "$DEST/dosbox-x.exe" ]; then
    echo "DOSBox-X $DOSBOX_VERSION already installed at $DEST"
    exit 0
fi

# Query GitHub API for the win64 MinGW zip asset URL
TAG="dosbox-x-v${DOSBOX_VERSION}"
echo "Finding DOSBox-X $DOSBOX_VERSION download URL (tag: $TAG)..."
RELEASE_JSON=$(curl -sL "https://api.github.com/repos/joncampbell123/dosbox-x/releases/tags/$TAG")

ASSET_URL=$(echo "$RELEASE_JSON" | grep -o '"browser_download_url": *"[^"]*mingw-win64[^"]*\.zip"' | head -1 | sed 's/"browser_download_url": *"//;s/"$//')

if [ -z "$ASSET_URL" ]; then
    echo "ERROR: Could not find DOSBox-X $DOSBOX_VERSION win64 download URL."
    echo "Check https://github.com/joncampbell123/dosbox-x/releases for valid versions"
    echo "and update DOSBOX_VERSION in tool_versions.conf."
    exit 1
fi

echo "Download URL: $ASSET_URL"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" dosbox-XXXXXX.zip)"

echo "Downloading DOSBox-X $DOSBOX_VERSION..."
curl -fSL --progress-bar -o "$TMPFILE" "$ASSET_URL"

# Helper: convert a POSIX path to a Windows path for powershell.exe
to_win_path() {
    if command -v wslpath >/dev/null 2>&1; then
        wslpath -w "$1"
    elif command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        echo "$1" | sed 's|^/mnt/\([a-z]\)/|\1:/|; s|^/\([a-z]\)/|\1:/|; s|/|\\|g'
    fi
}

echo "Extracting..."
TMPDIR2="$(mktemp -d -p "${TMPDIR:-/tmp}" dosbox-extract-XXXXXX)"
if command -v unzip >/dev/null 2>&1; then
    unzip -q -o "$TMPFILE" -d "$TMPDIR2"
else
    # Fall back to PowerShell on Windows
    powershell.exe -NoProfile -Command "Expand-Archive -Path '$(to_win_path "$TMPFILE")' -DestinationPath '$(to_win_path "$TMPDIR2")' -Force"
fi

# Find dosbox-x.exe in the extracted contents
DOSBOX_EXE=$(find "$TMPDIR2" -name "dosbox-x.exe" -type f | head -1)
if [ -z "$DOSBOX_EXE" ]; then
    echo "ERROR: Could not find dosbox-x.exe in extracted archive."
    echo "Contents:"
    find "$TMPDIR2" -maxdepth 3 -type f | head -20
    rm -rf "$TMPDIR2" "$TMPFILE"
    exit 1
fi

# Move the directory containing dosbox-x.exe to the destination
DOSBOX_DIR=$(dirname "$DOSBOX_EXE")
mkdir -p "$DEST"
cp -r "$DOSBOX_DIR"/* "$DEST"/

rm -rf "$TMPDIR2" "$TMPFILE"

if [ -f "$DEST/dosbox-x.exe" ]; then
    echo "DOSBox-X $DOSBOX_VERSION installed at $DEST"
else
    echo "ERROR: installation failed — dosbox-x.exe not found at $DEST"
    exit 1
fi
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
