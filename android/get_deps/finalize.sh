#!/bin/bash
# finalize.sh - Accept Android SDK licenses and install required platform packages.
# Reads versions from tool_versions.conf. Run get_sdk.sh first.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
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
    SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
    if [ -f "$SCRIPT_DIR/set_vars.sh" ]; then
        source "$SCRIPT_DIR/set_vars.sh"
    fi
fi

echo "Accepting SDK licenses..."
yes | "$SDKMANAGER" --licenses >/dev/null 2>&1 || true

echo "Installing platform packages..."
PACKAGES="platforms;android-$COMPILE_SDK build-tools;$BUILD_TOOLS_VERSION platform-tools cmake;$CMAKE_VERSION"
# Also install the emulator's platform if it differs from COMPILE_SDK
if [ -n "$EMULATOR_API_LEVEL" ] && [ "$EMULATOR_API_LEVEL" != "$COMPILE_SDK" ]; then
    PACKAGES="$PACKAGES platforms;android-$EMULATOR_API_LEVEL"
fi
# shellcheck disable=SC2086  # intentional word splitting on $PACKAGES
$SDKMANAGER $PACKAGES

echo "Done. SDK is ready"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
