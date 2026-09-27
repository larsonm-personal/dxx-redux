#!/bin/bash
# get_emulator.sh - Install the Android emulator and an x86_64 system image via sdkmanager.
# Reads API level from tool_versions.conf.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

source "$SCRIPT_DIR/sdk_tools.sh"
initialize_sdk_environment
SDKMANAGER="$(resolve_sdk_tool sdkmanager)"

emulator_ready() {
    sdk_package_has_metadata emulator || return 1
    if [ "$(get_host_os)" = windows ]; then
        [ -s "$SDK_DIR/emulator/emulator.exe" ]
    else
        [ -x "$SDK_DIR/emulator/emulator" ]
    fi
}

# Check if emulator is already installed
if emulator_ready; then
    echo "Emulator already installed"
else
    echo "Installing Android emulator..."
    assert_dependency_disk_space "$SDK_DIR" 2
    "$SDKMANAGER" "emulator"
    if ! emulator_ready; then
        echo 'ERROR: sdkmanager did not complete the emulator installation' >&2
        exit 1
    fi
fi

# Install the x86_64 system image
IMAGE="system-images;android-$EMULATOR_API_LEVEL;google_apis;x86_64"
IMAGE_DIR="$SDK_DIR/system-images/android-$EMULATOR_API_LEVEL/google_apis/x86_64"

if sdk_package_has_metadata "$IMAGE" && [ -s "$IMAGE_DIR/system.img" ]; then
    echo "System image already installed: $IMAGE"
else
    echo "Installing system image: $IMAGE..."
    assert_dependency_disk_space "$SDK_DIR" 6
    "$SDKMANAGER" "$IMAGE"
    if ! sdk_package_has_metadata "$IMAGE" || [ ! -s "$IMAGE_DIR/system.img" ]; then
        echo 'ERROR: sdkmanager did not complete the system image installation' >&2
        exit 1
    fi
fi

echo "Emulator and system image ready"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
