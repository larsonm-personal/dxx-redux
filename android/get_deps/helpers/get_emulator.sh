#!/bin/bash
# get_emulator.sh - Install the Android emulator and an x86_64 system image via sdkmanager.
# Reads API level from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"

SDK_DIR="$INSTALL_DIR/android-sdk"
SDKMANAGER="$SDK_DIR/cmdline-tools/latest/bin/sdkmanager"

# On Windows we need .bat
if [ -f "$SDKMANAGER.bat" ]; then
    SDKMANAGER="$SDKMANAGER.bat"
elif [ ! -x "$SDKMANAGER" ]; then
    echo "ERROR: sdkmanager not found. Run get_sdk.sh first"
    exit 1
fi

# Need JAVA_HOME for sdkmanager
if [ -z "$JAVA_HOME" ]; then
    ANDROID_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
    if [ -f "$ANDROID_DIR/set_vars.sh" ]; then
        source "$ANDROID_DIR/set_vars.sh"
    fi
fi

# Check if emulator is already installed
if [ -f "$SDK_DIR/emulator/emulator.exe" ] || [ -x "$SDK_DIR/emulator/emulator" ]; then
    echo "Emulator already installed"
else
    echo "Installing Android emulator..."
    "$SDKMANAGER" "emulator"
fi

# Install the x86_64 system image
IMAGE="system-images;android-$EMULATOR_API_LEVEL;google_apis;x86_64"
IMAGE_DIR="$SDK_DIR/system-images/android-$EMULATOR_API_LEVEL/google_apis/x86_64"

if [ -d "$IMAGE_DIR" ]; then
    echo "System image already installed: $IMAGE"
else
    echo "Installing system image: $IMAGE (~1.5 GB)..."
    "$SDKMANAGER" "$IMAGE"
fi

echo "Emulator and system image ready"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
