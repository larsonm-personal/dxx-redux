#!/bin/bash
# finalize.sh - Accept Android SDK licenses and install required platform packages.
# Reads versions from tool_versions.conf. Run get_sdk.sh first.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/../tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

source "$SCRIPT_DIR/sdk_tools.sh"
initialize_sdk_environment
SDKMANAGER="$(resolve_sdk_tool sdkmanager)"

echo "Accepting SDK licenses..."
(
    set +o pipefail
    yes | "$SDKMANAGER" --licenses >/dev/null
)

resolve_platform_package() {
    local api_level="$1"
    local package="platforms;android-$api_level"

    if printf '%s\n' "$SDK_PACKAGE_LIST" | grep -q "^[[:space:]]*${package}[[:space:]|]"; then
        printf '%s\n' "$package"
        return
    fi

    if printf '%s\n' "$SDK_PACKAGE_LIST" | grep -q "^[[:space:]]*${package}\\.0[[:space:]|]"; then
        printf '%s\n' "$package.0"
        return
    fi

    printf '%s\n' "$package"
}

echo "Installing platform packages..."
SDK_PACKAGE_LIST="$("$SDKMANAGER" --list)"
# Android CLI-backed sdkmanager lists slash paths instead of the classic semicolon IDs
SDK_PACKAGE_LIST="${SDK_PACKAGE_LIST//\//;}"
COMPILE_SDK_PACKAGE="$(resolve_platform_package "$COMPILE_SDK")"
PACKAGES=("$COMPILE_SDK_PACKAGE" "build-tools;$BUILD_TOOLS_VERSION" platform-tools "cmake;$CMAKE_VERSION")
# Also install the emulator's platform if it differs from COMPILE_SDK
if [ -n "$EMULATOR_API_LEVEL" ] && [ "$EMULATOR_API_LEVEL" != "$COMPILE_SDK" ]; then
    EMULATOR_PLATFORM_PACKAGE="$(resolve_platform_package "$EMULATOR_API_LEVEL")"
    PACKAGES+=("$EMULATOR_PLATFORM_PACKAGE")
fi
assert_dependency_disk_space "$SDK_DIR" 1
"$SDKMANAGER" "${PACKAGES[@]}"

for package in "${PACKAGES[@]}"; do
    if ! sdk_package_has_metadata "$package"; then
        echo "ERROR: sdkmanager did not complete package metadata: $package" >&2
        exit 1
    fi
done
complete_sdk_provisioning

echo "Done. SDK is ready"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
