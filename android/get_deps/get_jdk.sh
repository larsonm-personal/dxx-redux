#!/bin/bash
# get_jdk.sh — Download and install OpenJDK 17 to C:/local/jdk-17 if not present.
set -e

JDK_VERSION="17.0.2"
JDK_DIR_NAME="jdk-17"
INSTALL_DIR="/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="/mnt/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="C:/local"

DEST="$INSTALL_DIR/$JDK_DIR_NAME"

if [ -d "$DEST" ] && [ -x "$DEST/bin/java" -o -x "$DEST/bin/java.exe" ]; then
    echo "JDK 17 already installed at $DEST"
    exit 0
fi

URL="https://download.java.net/java/GA/jdk17.0.2/dfd4a8d0985749f896bed50d7138ee7f/8/GPL/openjdk-${JDK_VERSION}_windows-x64_bin.zip"
TMPFILE="$(mktemp -p "${TMPDIR:-/tmp}" jdk-XXXXXX.zip)"

echo "Downloading OpenJDK $JDK_VERSION..."
curl -fSL --progress-bar -o "$TMPFILE" "$URL"

echo "Extracting to $DEST..."
# The zip contains jdk-17.0.2/ at the top level
unzip -q -o "$TMPFILE" -d "$INSTALL_DIR"
# Rename to the canonical folder name
if [ -d "$INSTALL_DIR/jdk-$JDK_VERSION" ] && [ ! -d "$DEST" ]; then
    mv "$INSTALL_DIR/jdk-$JDK_VERSION" "$DEST"
fi

rm -f "$TMPFILE"
echo "JDK 17 installed at $DEST"
"$DEST/bin/java" -version 2>&1 | head -1
