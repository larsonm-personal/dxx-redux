#!/bin/bash
# get_ktlint.sh -- Download the ktlint Kotlin linter/formatter if not present.
# Reads version from tool_versions.conf.
# Source: https://github.com/pinterest/ktlint
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"
DEST="$INSTALL_DIR/ktlint-$KTLINT_VERSION"
begin_dependency_install "$DEST"

JDK_DIR="${JAVA_HOME:-$INSTALL_DIR/jdk-$JDK_MAJOR}"
JAVA_NAME="$(get_platform_executable_name java "$DEST")"
for cached in "$DEST/ktlint.jar" "$DEST/ktlint"; do
    if [ -f "$cached" ] && verify_dependency_tool_version "$KTLINT_VERSION" exact "$JDK_DIR/bin/$JAVA_NAME" -jar "$cached" --version >/dev/null 2>&1; then
        echo "Verified ktlint $KTLINT_VERSION already installed at $DEST"
        exit 0
    fi
done

URL="$KTLINT_URL"

echo "Downloading ktlint $KTLINT_VERSION..."
echo "  URL: $URL"

prepare_dependency_workspace "$DEST"
download_file "$DEPENDENCY_ARCHIVE" "$URL"
mv "$DEPENDENCY_ARCHIVE" "$DEPENDENCY_STAGE_DIR/ktlint.jar"

# Validate before publication so a bad download never becomes the installed tool
verify_dependency_tool_version "$KTLINT_VERSION" exact "$JDK_DIR/bin/$JAVA_NAME" -jar "$DEPENDENCY_STAGE_DIR/ktlint.jar" --version
publish_dependency_directory "$DEPENDENCY_STAGE_DIR" "$DEST"
echo "ktlint $KTLINT_VERSION installed at $DEST/ktlint.jar"

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
