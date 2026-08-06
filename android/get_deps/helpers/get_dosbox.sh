#!/bin/bash
# get_dosbox.sh - Download and install DOSBox-X if not present.
# Reads version from tool_versions.conf.
# DOSBox-X is used to run DOS shareware demo installers for file extraction.
# Releases: https://github.com/joncampbell123/dosbox-x/releases
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"
source "$SCRIPT_DIR/verify_sha256.sh"

INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/$DOSBOX_DIR_NAME"

if [ -d "$DEST" ] && [ -f "$DEST/dosbox-x.exe" ]; then
    verify_sha256 "$DEST/dosbox-x.exe" "$DOSBOX_EXE_SHA256" "cached dosbox-x.exe"
    echo "Verified DOSBox-X $DOSBOX_VERSION already installed at $DEST"
    exit 0
fi

echo "Download URL: $DOSBOX_URL"
TMPFILE="$(create_temp_file dosbox.zip)"

echo "Downloading DOSBox-X $DOSBOX_VERSION..."
download_file "$TMPFILE" "$DOSBOX_URL"
verify_sha256 "$TMPFILE" "$DOSBOX_ARCHIVE_SHA256" "DOSBox-X package"

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
TMPDIR2="$(create_temp_dir dosbox-extract)"
if command -v unzip >/dev/null 2>&1; then
    unzip -q -o "$TMPFILE" -d "$TMPDIR2"
else
    # Fall back to PowerShell on Windows
    powershell.exe -NoProfile -Command "Expand-Archive -Path '$(to_win_path "$TMPFILE")' -DestinationPath '$(to_win_path "$TMPDIR2")' -Force"
fi

# Select the repository-pinned build inside the archive
DOSBOX_EXE="$TMPDIR2/$DOSBOX_EXE_RELATIVE_PATH"
if [ ! -f "$DOSBOX_EXE" ]; then
    echo "ERROR: Could not find pinned $DOSBOX_EXE_RELATIVE_PATH in extracted archive"
    echo "Contents:"
    find "$TMPDIR2" -maxdepth 3 -type f | head -20
    rm -rf "$TMPDIR2" "$TMPFILE"
    exit 1
fi
verify_sha256 "$DOSBOX_EXE" "$DOSBOX_EXE_SHA256" "staged dosbox-x.exe"

# Move the directory containing dosbox-x.exe to the destination
DOSBOX_DIR=$(dirname "$DOSBOX_EXE")
mkdir -p "$DEST"
cp -r "$DOSBOX_DIR"/* "$DEST"/

rm -rf "$TMPDIR2" "$TMPFILE"

if [ -f "$DEST/dosbox-x.exe" ]; then
    verify_sha256 "$DEST/dosbox-x.exe" "$DOSBOX_EXE_SHA256" "installed dosbox-x.exe"
    echo "DOSBox-X $DOSBOX_VERSION installed and verified at $DEST"
else
    echo "ERROR: installation failed - dosbox-x.exe not found at $DEST"
    exit 1
fi
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
