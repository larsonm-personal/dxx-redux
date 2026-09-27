#!/bin/bash
# get_cmake.sh - Download and install CMake if not present.
# Reads version/URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

CMAKE_DIR_NAME="cmake-$CMAKE_VERSION"
INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/$CMAKE_DIR_NAME"
begin_dependency_install "$DEST"

if [ -d "$DEST" ] && { [ -x "$DEST/bin/cmake" ] || [ -x "$DEST/bin/cmake.exe" ]; }; then
    echo "CMake $CMAKE_VERSION already installed at $DEST"
    "$DEST/bin/cmake" --version | head -1
    exit 0
fi

URL="$CMAKE_URL"
ARCHIVE_KIND="zip"
case "$(get_host_os)" in
linux)
    URL="https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-linux-x86_64.tar.gz"
    ARCHIVE_KIND="tar.gz"
    ;;
macos)
    URL="https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-macos-universal.tar.gz"
    ARCHIVE_KIND="tar.gz"
    ;;
esac
prepare_dependency_workspace "$DEST" 1
TMPFILE="$DEPENDENCY_ARCHIVE"
STAGE_DIR="$DEPENDENCY_STAGE_DIR"

echo "Downloading CMake $CMAKE_VERSION..."
download_file "$TMPFILE" "$URL"

echo "Extracting to $DEST..."
if [ "$ARCHIVE_KIND" = "zip" ]; then
    unzip -q -o "$TMPFILE" -d "$STAGE_DIR"
else
    tar -xzf "$TMPFILE" -C "$STAGE_DIR"
fi
STAGED_CMAKE=""
for candidate in "$STAGE_DIR"/cmake-"${CMAKE_VERSION}"*; do
    if [ -d "$candidate/CMake.app/Contents" ]; then candidate="$candidate/CMake.app/Contents"; fi
    if [ -x "$candidate/bin/cmake" ] || [ -x "$candidate/bin/cmake.exe" ]; then
        STAGED_CMAKE="$candidate"
        break
    fi
done
if [ -z "$STAGED_CMAKE" ]; then
    echo "ERROR: CMake archive did not contain the expected executable" >&2
    exit 1
fi
"$STAGED_CMAKE/bin/cmake" --version
publish_dependency_directory "$STAGED_CMAKE" "$DEST"

rm -f "$TMPFILE"
echo "CMake $CMAKE_VERSION installed at $DEST"
"$DEST/bin/cmake" --version | head -1
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
