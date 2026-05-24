#!/bin/bash
# get_soundfont.sh - Download the GM soundfont (TimGM6mb.sf2) into the assets
# directory if it is missing or has the wrong hash.
# Reads URL and SHA256 from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"

DEST="$SCRIPT_DIR/../../app/src/main/assets/gm.sf2"

# --- Check if already present with correct hash ---
if [ -f "$DEST" ]; then
    ACTUAL=$(sha256sum "$DEST" | awk '{print $1}')
    if [ "$ACTUAL" = "$SOUNDFONT_SHA256" ]; then
        echo "Soundfont already present and verified: $DEST"
        exit 0
    fi
    echo "Soundfont hash mismatch (expected ${SOUNDFONT_SHA256:0:16}..., got ${ACTUAL:0:16}...)"
    echo "Re-downloading..."
    rm -f "$DEST"
fi

# --- Download ---
mkdir -p "$(dirname "$DEST")"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" sf2-XXXXXX)"

echo "Downloading TimGM6mb.sf2 (v${SOUNDFONT_VERSION})..."
download_file "$TMPFILE" "$SOUNDFONT_URL"

# --- Verify hash ---
ACTUAL=$(sha256sum "$TMPFILE" | awk '{print $1}')
if [ "$ACTUAL" != "$SOUNDFONT_SHA256" ]; then
    echo "ERROR: SHA256 mismatch after download!"
    echo "  Expected: $SOUNDFONT_SHA256"
    echo "  Got:      $ACTUAL"
    rm -f "$TMPFILE"
    exit 1
fi

mv "$TMPFILE" "$DEST"
echo "Soundfont installed: $DEST"
