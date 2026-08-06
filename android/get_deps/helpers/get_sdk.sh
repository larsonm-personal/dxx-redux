#!/bin/bash
# get_sdk.sh - Download and install Android SDK command-line tools if not present.
# Reads URL from tool_versions.conf.
# After running this, run finalize.sh to accept licenses and install platform packages.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/android-sdk"

URL="$SDK_CMDLINE_TOOLS_URL"
BUILD_ID=""
if BUILD_ID="$(get_sdk_cmdline_tools_build_id "$SDK_CMDLINE_TOOLS_URL" 2>/dev/null)"; then
    if DERIVED_URL="$(get_sdk_cmdline_tools_url "$BUILD_ID" 2>/dev/null)"; then
        URL="$DERIVED_URL"
    fi
else
    BUILD_ID=""
fi

LATEST_DIR="$DEST/cmdline-tools/latest"
MARKER_FILE="$LATEST_DIR/.dxx-cmdline-tools-build-id"
if [ -x "$LATEST_DIR/bin/sdkmanager" ] || [ -f "$LATEST_DIR/bin/sdkmanager.bat" ]; then
    INSTALLED_BUILD_ID=""
    if [ -f "$MARKER_FILE" ]; then
        INSTALLED_BUILD_ID="$(head -1 "$MARKER_FILE" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    fi

    if [ -n "$BUILD_ID" ] && [ "$INSTALLED_BUILD_ID" = "$BUILD_ID" ]; then
        echo "Android SDK command-line tools build $BUILD_ID already installed at $DEST"
        exit 0
    fi

    if [ -z "$BUILD_ID" ]; then
        echo "Android SDK command-line tools already installed at $DEST"
        exit 0
    fi

    echo "Refreshing Android SDK command-line tools build ${INSTALLED_BUILD_ID:-unknown} -> $BUILD_ID"
fi

TMPFILE="$(create_temp_file sdk.zip)"
EXTRACT_DIR="$(create_temp_dir sdk-extract)"
cleanup() {
    rm -f "$TMPFILE"
    rm -rf "$EXTRACT_DIR"
}
trap cleanup EXIT

echo "Downloading Android SDK command-line tools..."
download_file "$TMPFILE" "$URL"

echo "Extracting to $LATEST_DIR..."
unzip -q -o "$TMPFILE" -d "$EXTRACT_DIR"
if [ ! -d "$EXTRACT_DIR/cmdline-tools" ]; then
    echo "ERROR: command-line tools archive did not contain cmdline-tools" >&2
    exit 1
fi

mkdir -p "$DEST/cmdline-tools"
case "$LATEST_DIR" in
"$DEST/cmdline-tools/latest") ;;
*)
    echo "ERROR: refusing to replace unexpected SDK command-line tools path: $LATEST_DIR" >&2
    exit 1
    ;;
esac
rm -rf "$LATEST_DIR"
mkdir -p "$LATEST_DIR"
mv "$EXTRACT_DIR/cmdline-tools"/* "$LATEST_DIR/"
if [ -n "$BUILD_ID" ]; then
    printf '%s\n' "$BUILD_ID" >"$MARKER_FILE"
fi

echo "Android SDK command-line tools installed at $DEST"
echo "Run finalize.sh next to accept licenses and install platform packages"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
