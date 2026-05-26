#!/bin/bash
# get_unar.sh - Download unar (The Unarchiver CLI) for Windows if not present.
# unar handles StuffIt Installer v2 (STi2) archives found on Mac CD images.
# Reads URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/$UNAR_DIR_NAME"

if [ "$(get_host_os)" = "linux" ]; then
    if command -v unar >/dev/null 2>&1 && command -v lsar >/dev/null 2>&1; then
        echo "Using host unar from PATH"
        exit 0
    fi

    echo "ERROR: unar is not installed on this Linux host" >&2
    echo "Install it with: sudo apt install unar" >&2
    exit 1
fi

if [ -f "$DEST/unar.exe" ]; then
    echo "unar already installed at $DEST"
    exit 0
fi

echo "Downloading unar from $UNAR_URL..."
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" unar-XXXXXX.zip)"
download_file "$TMPFILE" "$UNAR_URL"

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
mkdir -p "$DEST"
if command -v unzip >/dev/null 2>&1; then
    unzip -q -o "$TMPFILE" -d "$DEST"
else
    powershell.exe -NoProfile -Command "Expand-Archive -Path '$(to_win_path "$TMPFILE")' -DestinationPath '$(to_win_path "$DEST")' -Force"
fi

rm -f "$TMPFILE"

# Clean up macOS metadata if present
rm -rf "$DEST/__MACOSX"

if [ -f "$DEST/unar.exe" ]; then
    echo "unar installed at $DEST"
else
    echo "ERROR: unar.exe not found after extraction"
    exit 1
fi

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
