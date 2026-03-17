#!/bin/bash
# create_avd.sh - Create a Pixel 6 AVD (API 34, x86_64) if it doesn't exist.
# Run get_emulator.sh first to install the emulator and system image.
set -e

AVD_NAME="Nexus5X_Light_1"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/resolve_dep_base.sh"

INSTALL_DIR="$LOCAL_DIR"

SDK_DIR="$INSTALL_DIR/android-sdk"
AVDMANAGER="$SDK_DIR/cmdline-tools/latest/bin/avdmanager"

# On Windows we need .bat
if [ -f "$AVDMANAGER.bat" ]; then
    AVDMANAGER="$AVDMANAGER.bat"
elif [ ! -x "$AVDMANAGER" ]; then
    echo "ERROR: avdmanager not found. Run get_sdk.sh first."
    exit 1
fi

# Need JAVA_HOME
if [ -z "$JAVA_HOME" ]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
    if [ -f "$SCRIPT_DIR/set_vars.sh" ]; then
        source "$SCRIPT_DIR/set_vars.sh"
    fi
fi

export ANDROID_HOME="$SDK_DIR"
export ANDROID_SDK_ROOT="$SDK_DIR"

# Check if AVD already exists
if "$AVDMANAGER" list avd 2>/dev/null | grep -q "Name: $AVD_NAME"; then
    echo "AVD '$AVD_NAME' already exists."
    exit 0
fi

IMAGE="system-images;android-34;google_apis;x86_64"

echo "Creating AVD '$AVD_NAME' (Nexus 5X, API 34, x86_64)..."
echo "no" | "$AVDMANAGER" create avd \
    --name "$AVD_NAME" \
    --package "$IMAGE" \
    --device "Nexus 5X" \
    --force

# Apply lightweight hardware settings (low RAM, low res, minimal sensors)
AVD_DIR="$HOME/.android/avd/${AVD_NAME}.avd"
if [ -f "$AVD_DIR/config.ini" ]; then
    cat >> "$AVD_DIR/config.ini" <<'EOF'
hw.ramSize=1536
hw.cpu.ncore=2
vm.heapSize=512
hw.gpu.enabled=yes
hw.gpu.mode=host
disk.dataPartition.size=4G
hw.keyboard=yes
hw.lcd.width=1280
hw.lcd.height=720
hw.lcd.density=320
hw.camera.back=none
hw.camera.front=none
hw.gps=no
hw.nfc=no
hw.gsmModem=no
hw.radio=no
hw.sim=no
hw.accelerometer=no
hw.gyroscope=no
hw.audioInput=no
hw.audioOutput=no
hw.sdCard=yes
sdcard.size=4096M
showDeviceFrame=no
skin.path=_no_skin
EOF
    echo "Applied lightweight hardware config (1536 MB RAM, 1280x960, minimal sensors)."
fi

echo "AVD '$AVD_NAME' created."
echo "Launch with: run_emulator.sh"
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
