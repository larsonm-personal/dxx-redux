#!/bin/bash
# create_avd.sh - Create a Nexus 5X AVD if it doesn't exist
# Run get_emulator.sh first to install the emulator and system image
set -e

AVD_NAME="Nexus5X_Light_1"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/tool_versions.conf"
source "$SCRIPT_DIR/platform.sh"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"

SDK_DIR="$INSTALL_DIR/android-sdk"
AVDMANAGER="$SDK_DIR/cmdline-tools/latest/bin/avdmanager"

# On Windows we need .bat
if [ -f "$AVDMANAGER.bat" ]; then
    AVDMANAGER="$AVDMANAGER.bat"
elif [ ! -x "$AVDMANAGER" ]; then
    echo "ERROR: avdmanager not found. Run get_sdk.sh first"
    exit 1
fi

# Need JAVA_HOME
if [ -z "$JAVA_HOME" ]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
    if [ -f "$SCRIPT_DIR/set_vars.sh" ]; then
        source "$SCRIPT_DIR/set_vars.sh"
    fi
fi

# avdmanager.bat needs Windows-style paths; Git Bash's /c/local/... won't work
if command -v cygpath >/dev/null 2>&1; then
    ANDROID_HOME="$(cygpath -w "$SDK_DIR")"
    export ANDROID_HOME
    ANDROID_SDK_ROOT="$(cygpath -w "$SDK_DIR")"
    export ANDROID_SDK_ROOT
else
    export ANDROID_HOME="$SDK_DIR"
    export ANDROID_SDK_ROOT="$SDK_DIR"
fi

set_avd_config() {
    local key="$1"
    local value="$2"
    local config_file="$3"

    if grep -Eq "^${key}[[:space:]]*=" "$config_file"; then
        sed -i "s|^${key}[[:space:]]*=.*|${key}=${value}|" "$config_file"
    else
        printf '%s=%s\n' "$key" "$value" >>"$config_file"
    fi
}

RESOLVED_EMULATOR_API_LEVEL="${EMULATOR_API_LEVEL:-34}"
IMAGE="system-images;android-${RESOLVED_EMULATOR_API_LEVEL};google_apis;x86_64"
AVD_DIR="$HOME/.android/avd/${AVD_NAME}.avd"

if [ -f "$AVD_DIR/config.ini" ]; then
    if grep -Eq "^target[[:space:]]*=[[:space:]]*android-${RESOLVED_EMULATOR_API_LEVEL}$" "$AVD_DIR/config.ini"; then
        echo "AVD '$AVD_NAME' already exists at $AVD_DIR"
        exit 0
    fi
    echo "Recreating AVD '$AVD_NAME' for API ${RESOLVED_EMULATOR_API_LEVEL}"
    "$AVDMANAGER" delete avd --name "$AVD_NAME" >/dev/null 2>&1 || true
fi

echo "Creating AVD '$AVD_NAME' (Nexus 5X, API ${RESOLVED_EMULATOR_API_LEVEL}, x86_64)"
echo "no" | "$AVDMANAGER" create avd \
    --name "$AVD_NAME" \
    --package "$IMAGE" \
    --device "Nexus 5X" \
    --force

# Apply lightweight hardware settings (low RAM, low res, minimal sensors)
if [ -f "$AVD_DIR/config.ini" ]; then
    set_avd_config "hw.ramSize" "1536" "$AVD_DIR/config.ini"
    set_avd_config "hw.cpu.ncore" "2" "$AVD_DIR/config.ini"
    set_avd_config "vm.heapSize" "512" "$AVD_DIR/config.ini"
    set_avd_config "hw.gpu.enabled" "yes" "$AVD_DIR/config.ini"
    set_avd_config "hw.gpu.mode" "host" "$AVD_DIR/config.ini"
    set_avd_config "hw.gpu.vulkan" "no" "$AVD_DIR/config.ini"
    set_avd_config "disk.dataPartition.size" "4G" "$AVD_DIR/config.ini"
    set_avd_config "hw.keyboard" "yes" "$AVD_DIR/config.ini"
    set_avd_config "hw.lcd.width" "1280" "$AVD_DIR/config.ini"
    set_avd_config "hw.lcd.height" "720" "$AVD_DIR/config.ini"
    set_avd_config "hw.lcd.density" "320" "$AVD_DIR/config.ini"
    set_avd_config "hw.camera.back" "none" "$AVD_DIR/config.ini"
    set_avd_config "hw.camera.front" "none" "$AVD_DIR/config.ini"
    set_avd_config "hw.gps" "no" "$AVD_DIR/config.ini"
    set_avd_config "hw.nfc" "no" "$AVD_DIR/config.ini"
    set_avd_config "hw.gsmModem" "no" "$AVD_DIR/config.ini"
    set_avd_config "hw.radio" "no" "$AVD_DIR/config.ini"
    set_avd_config "hw.sim" "no" "$AVD_DIR/config.ini"
    set_avd_config "hw.accelerometer" "yes" "$AVD_DIR/config.ini"
    set_avd_config "hw.gyroscope" "yes" "$AVD_DIR/config.ini"
    set_avd_config "hw.audioInput" "no" "$AVD_DIR/config.ini"
    set_avd_config "hw.audioOutput" "yes" "$AVD_DIR/config.ini"
    set_avd_config "hw.sdCard" "yes" "$AVD_DIR/config.ini"
    set_avd_config "sdcard.size" "4096M" "$AVD_DIR/config.ini"
    set_avd_config "showDeviceFrame" "no" "$AVD_DIR/config.ini"
    echo "Applied lightweight hardware config (1536 MB RAM, 1280x720, minimal sensors)"
fi

echo "AVD '$AVD_NAME' created"
echo "From the repo root, launch with: pwsh android/Run-Emulator.ps1"
if [ -z "${GET_ALL_RUNNING:-}" ] && [ -t 0 ]; then
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
fi
