#!/bin/bash
# get_clang_format.sh -- Download a pre-built clang-format binary if not present.
# Reads version from tool_versions.conf.
# Source: https://github.com/muttleyxd/clang-tools-static-binaries
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/clang-format-$CLANG_FORMAT_VERSION"
begin_dependency_install "$DEST"

DEST_NAME="$(get_platform_executable_name clang-format "$DEST")"
if [ -f "$DEST/$DEST_NAME" ] && verify_dependency_tool_version "${CLANG_FORMAT_VERSION}" prefix "$DEST/$DEST_NAME" --version >/dev/null 2>&1; then
    echo "Verified clang-format ${CLANG_FORMAT_VERSION} already installed at $DEST"
    exit 0
fi

# Pick the right binary name for the current platform.
# When running in WSL but targeting a Windows filesystem (/mnt/), use Windows binary.
if is_windows_target_path "$DEST"; then
    ASSET_NAME="clang-format-${CLANG_FORMAT_VERSION}_windows-amd64.exe"
    DEST_NAME="clang-format.exe"
elif [ "$(uname -s)" = "Darwin" ]; then
    ASSET_NAME="clang-format-${CLANG_FORMAT_VERSION}_macosx-amd64"
    DEST_NAME="clang-format"
else
    ASSET_NAME="clang-format-${CLANG_FORMAT_VERSION}_linux-amd64"
    DEST_NAME="clang-format"
fi

URL="https://github.com/muttleyxd/clang-tools-static-binaries/releases/download/${CLANG_FORMAT_RELEASE_TAG}/${ASSET_NAME}"

echo "Downloading clang-format $CLANG_FORMAT_VERSION..."
echo "  URL: $URL"

prepare_dependency_workspace "$DEST"
TMPFILE="$DEPENDENCY_ARCHIVE"
download_file "$TMPFILE" "$URL"
mv "$TMPFILE" "$DEPENDENCY_STAGE_DIR/$DEST_NAME"
chmod +x "$DEPENDENCY_STAGE_DIR/$DEST_NAME"

# --- Windows VC++ runtime check --------------------------------------------
if is_windows_target_path "$DEST"; then
    echo "Checking clang-format runtime..."

    if ! "$DEPENDENCY_STAGE_DIR/$DEST_NAME" --version >/dev/null 2>&1; then
        echo "clang-format failed to run; attempting to install Microsoft Visual C++ Redistributable..."

        VC_REDIST_URL="https://aka.ms/vs/17/release/vc_redist.x64.exe"
        VC_TMP_DIR="$DEPENDENCY_WORK_DIR/vc-redist"
        mkdir "$VC_TMP_DIR"
        VC_TMP="$VC_TMP_DIR/vc_redist.exe"

        echo "  Downloading VC++ Redistributable from:"
        echo "    $VC_REDIST_URL"

        download_file "$VC_TMP" "$VC_REDIST_URL"

        echo "  Running installer silently..."
        cmd.exe /C "$VC_TMP /install /quiet /norestart" || {
            echo "VC++ Redistributable installation failed"
            exit 1
        }
        rm -rf "$VC_TMP_DIR"

        echo "VC++ Redistributable installed. Re-testing clang-format..."
        "$DEPENDENCY_STAGE_DIR/$DEST_NAME" --version
    else
        echo "clang-format runs successfully; VC++ runtime appears to be present"
    fi
fi
# ---------------------------------------------------------------------------

verify_dependency_tool_version "${CLANG_FORMAT_VERSION}" prefix "$DEPENDENCY_STAGE_DIR/$DEST_NAME" --version
publish_dependency_directory "$DEPENDENCY_STAGE_DIR" "$DEST"
echo "clang-format $CLANG_FORMAT_VERSION installed at $DEST/$DEST_NAME"

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
