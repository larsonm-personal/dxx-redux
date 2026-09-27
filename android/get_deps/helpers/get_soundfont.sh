#!/bin/bash
# get_soundfont.sh - Download the pinned bundled GM soundfont into the assets
# directory if it is missing or has the wrong hash.
# Reads URL and SHA256 from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/verify_sha256.sh"

DEST="$SCRIPT_DIR/../../app/src/main/assets/gm.sf2"

if [ -L "$DEST" ]; then
    echo "ERROR: refusing linked soundfont destination: $DEST" >&2
    exit 1
fi
if [ -f "$DEST" ] && verify_sha256 "$DEST" "$SOUNDFONT_SHA256" "bundled soundfont"; then
    echo "Soundfont already present and verified: $DEST"
    exit 0
fi

# --- Download ---
mkdir -p "$(dirname "$DEST")"
assert_dependency_disk_space "$(dirname "$DEST")" 0
TMPFILE="$(create_temp_file .dxx-soundfont "$(dirname "$DEST")")"
trap 'rm -f "$TMPFILE"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "Downloading bundled soundfont (v${SOUNDFONT_VERSION})..."
download_file "$TMPFILE" "$SOUNDFONT_URL"

# Keep the existing asset until the replacement is verified
verify_sha256 "$TMPFILE" "$SOUNDFONT_SHA256" "downloaded soundfont"

mv "$TMPFILE" "$DEST"
echo "Soundfont installed: $DEST"
