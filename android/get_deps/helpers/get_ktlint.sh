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

if [ -f "$DEST/ktlint" ] || [ -f "$DEST/ktlint.jar" ]; then
    echo "ktlint $KTLINT_VERSION already installed at $DEST"
    exit 0
fi

URL="$KTLINT_URL"

echo "Downloading ktlint $KTLINT_VERSION..."
echo "  URL: $URL"

mkdir -p "$DEST"
TMPFILE="$(create_temp_file ktlint)"

download_file "$TMPFILE" "$URL"
mv "$TMPFILE" "$DEST/ktlint.jar"

echo "ktlint $KTLINT_VERSION installed at $DEST/ktlint.jar"

# Verify it runs (requires JDK)
JDK_DIR="$INSTALL_DIR/jdk-$JDK_MAJOR"
if [ -x "$JDK_DIR/bin/java" ] || [ -x "$JDK_DIR/bin/java.exe" ]; then
    "$JDK_DIR/bin/java" -jar "$DEST/ktlint.jar" --version 2>/dev/null || true
else
    echo "  (JDK not found at $JDK_DIR -- install JDK first to verify)"
fi

if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
