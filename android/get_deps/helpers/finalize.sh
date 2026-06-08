#!/bin/bash
# finalize.sh - Accept Android SDK licenses and install required platform packages.
# Reads versions from tool_versions.conf. Run get_sdk.sh first.
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
    if [ -f "$ANDROID_DIR/helpers/set_vars.sh" ]; then
        source "$ANDROID_DIR/helpers/set_vars.sh"
    fi
fi

echo "Accepting SDK licenses..."
yes | "$SDKMANAGER" --licenses >/dev/null 2>&1 || true

resolve_platform_package() {
    local api_level="$1"
    local package="platforms;android-$api_level"
    local package_list
    package_list="$("$SDKMANAGER" --list 2>/dev/null || true)"

    if printf '%s\n' "$package_list" | grep -q "^[[:space:]]*${package}[[:space:]|]"; then
        printf '%s\n' "$package"
        return
    fi

    if printf '%s\n' "$package_list" | grep -q "^[[:space:]]*${package}\\.0[[:space:]|]"; then
        printf '%s\n' "$package.0"
        return
    fi

    printf '%s\n' "$package"
}

echo "Installing platform packages..."
COMPILE_SDK_PACKAGE="$(resolve_platform_package "$COMPILE_SDK")"
PACKAGES="$COMPILE_SDK_PACKAGE build-tools;$BUILD_TOOLS_VERSION platform-tools cmake;$CMAKE_VERSION"
# Also install the emulator's platform if it differs from COMPILE_SDK
if [ -n "$EMULATOR_API_LEVEL" ] && [ "$EMULATOR_API_LEVEL" != "$COMPILE_SDK" ]; then
    EMULATOR_PLATFORM_PACKAGE="$(resolve_platform_package "$EMULATOR_API_LEVEL")"
    PACKAGES="$PACKAGES $EMULATOR_PLATFORM_PACKAGE"
fi
# shellcheck disable=SC2086  # intentional word splitting on $PACKAGES
$SDKMANAGER $PACKAGES

echo "Done. SDK is ready"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
