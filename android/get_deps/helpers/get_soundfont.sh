#!/bin/bash
# get_soundfont.sh - Download the pinned bundled GM soundfont into the assets
# directory if it is missing or has the wrong hash.
# Reads URL and SHA256 from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/verify_sha256.sh"

ASSETS_DIR="$SCRIPT_DIR/../../app/src/main/assets"
DEST="$ASSETS_DIR/gm.sf2"

if [ -L "$ASSETS_DIR" ]; then
    echo "ERROR: refusing linked assets directory: $ASSETS_DIR" >&2
    exit 1
fi
mkdir -p "$ASSETS_DIR"
# Own the assets installation workspace, keeping recovery files outside the APK
begin_dependency_install "$ASSETS_DIR"
if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ] && [ "$(stat -c %d "$ASSETS_DIR")" != "$(stat -c %d "$DEPENDENCY_INSTALL_STATE")" ]; then
    echo "ERROR: soundfont staging and assets must be on the same filesystem" >&2
    exit 1
fi

if [ -L "$DEST" ] || { [ -e "$DEST" ] && [ ! -f "$DEST" ]; }; then
    echo "ERROR: refusing non-regular soundfont destination: $DEST" >&2
    exit 1
fi
if [ -f "$DEST" ] && verify_sha256 "$DEST" "$SOUNDFONT_SHA256" "bundled soundfont"; then
    echo "Soundfont already present and verified: $DEST"
    exit 0
fi

# --- Download ---
if [ -n "${DEPENDENCY_INSTALL_STATE:-}" ]; then
    prepare_dependency_workspace "$ASSETS_DIR" 0
    TMPFILE="$DEPENDENCY_ARCHIVE"
else
    assert_dependency_disk_space "$ASSETS_DIR" 0
    TMPFILE="$(create_temp_file .dxx-soundfont "$ASSETS_DIR")"
    trap 'rm -f "$TMPFILE"' EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
fi

echo "Downloading bundled soundfont (v${SOUNDFONT_VERSION})..."
download_file "$TMPFILE" "$SOUNDFONT_URL"

# Keep the existing asset until the replacement is verified
verify_sha256 "$TMPFILE" "$SOUNDFONT_SHA256" "downloaded soundfont"

mv "$TMPFILE" "$DEST"
echo "Soundfont installed: $DEST"
