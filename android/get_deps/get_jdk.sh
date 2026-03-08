#!/bin/bash
# get_jdk.sh — Download and install OpenJDK if not present.
# Reads version/URL from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/resolve_dep_base.sh"

JDK_DIR_NAME="jdk-$JDK_MAJOR"
INSTALL_DIR="$LOCAL_DIR"

DEST="$INSTALL_DIR/$JDK_DIR_NAME"

if [ -d "$DEST" ] && [ -x "$DEST/bin/java" -o -x "$DEST/bin/java.exe" ]; then
    echo "JDK $JDK_MAJOR already installed at $DEST"
    exit 0
fi

URL="$JDK_URL"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" jdk-XXXXXX.zip)"

echo "Downloading OpenJDK $JDK_VERSION..."
curl -fSL --progress-bar -o "$TMPFILE" "$URL"

echo "Extracting to $DEST..."
# The zip contains a folder like jdk-21.0.10+7/ at the top level
unzip -q -o "$TMPFILE" -d "$INSTALL_DIR"
# Rename the extracted folder (may include build number like +7) to the canonical name
if [ ! -d "$DEST" ]; then
    EXTRACTED=$(find "$INSTALL_DIR" -maxdepth 1 -type d -name "jdk-${JDK_VERSION}*" | head -1)
    if [ -n "$EXTRACTED" ]; then
        mv "$EXTRACTED" "$DEST"
    fi
fi

rm -f "$TMPFILE"
echo "JDK $JDK_MAJOR installed at $DEST"
"$DEST/bin/java" -version 2>&1 | head -1
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
