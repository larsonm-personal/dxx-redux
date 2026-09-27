#!/bin/bash
# get_shfmt.sh -- Download a pre-built shfmt binary if not present.
# Reads version from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/shfmt-$SHFMT_VERSION"
begin_dependency_install "$DEST"

DEST_NAME="$(get_platform_executable_name shfmt "$DEST")"
if [ -f "$DEST/$DEST_NAME" ] && verify_dependency_tool_version "${SHFMT_VERSION}" exact "$DEST/$DEST_NAME" --version >/dev/null 2>&1; then
    echo "Verified shfmt ${SHFMT_VERSION} already installed at $DEST"
    exit 0
fi

# Pick URL for the current platform
if is_windows_target_path "$DEST"; then
    URL="https://github.com/mvdan/sh/releases/download/v${SHFMT_VERSION}/shfmt_v${SHFMT_VERSION}_windows_amd64.exe"
    DEST_NAME="shfmt.exe"
elif [ "$(uname -s)" = "Darwin" ]; then
    URL="https://github.com/mvdan/sh/releases/download/v${SHFMT_VERSION}/shfmt_v${SHFMT_VERSION}_darwin_amd64"
    DEST_NAME="shfmt"
else
    URL="https://github.com/mvdan/sh/releases/download/v${SHFMT_VERSION}/shfmt_v${SHFMT_VERSION}_linux_amd64"
    DEST_NAME="shfmt"
fi

echo "Downloading shfmt $SHFMT_VERSION..."
echo "  URL: $URL"

prepare_dependency_workspace "$DEST"
TMPFILE="$DEPENDENCY_ARCHIVE"
download_file "$TMPFILE" "$URL"
mv "$TMPFILE" "$DEPENDENCY_STAGE_DIR/$DEST_NAME"
chmod +x "$DEPENDENCY_STAGE_DIR/$DEST_NAME"
verify_dependency_tool_version "${SHFMT_VERSION}" exact "$DEPENDENCY_STAGE_DIR/$DEST_NAME" --version
publish_dependency_directory "$DEPENDENCY_STAGE_DIR" "$DEST"
echo "shfmt $SHFMT_VERSION installed at $DEST"
