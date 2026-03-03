#!/bin/bash
# finalize.sh — Accept Android SDK licenses and install required platform packages.
# Run get_sdk.sh first.
set -e

INSTALL_DIR="/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="/mnt/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="C:/local"

SDK_DIR="$INSTALL_DIR/android-sdk"
SDKMANAGER="$SDK_DIR/cmdline-tools/latest/bin/sdkmanager"

# On Windows we need .bat
if [ -f "$SDKMANAGER.bat" ]; then
    SDKMANAGER="$SDKMANAGER.bat"
elif [ ! -x "$SDKMANAGER" ]; then
    echo "ERROR: sdkmanager not found. Run get_sdk.sh first."
    exit 1
fi

# Need JAVA_HOME for sdkmanager
if [ -z "$JAVA_HOME" ]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
    if [ -f "$SCRIPT_DIR/set_vars.sh" ]; then
        source "$SCRIPT_DIR/set_vars.sh"
    fi
fi

echo "Accepting SDK licenses..."
yes | "$SDKMANAGER" --licenses 2>/dev/null || true

echo "Installing platform packages..."
"$SDKMANAGER" "platforms;android-34" "build-tools;34.0.0" "platform-tools"

echo "Done. SDK is ready."
