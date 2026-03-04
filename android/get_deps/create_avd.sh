#!/bin/bash
# create_avd.sh — Create a Pixel 6 AVD (API 34, x86_64) if it doesn't exist.
# Run get_emulator.sh first to install the emulator and system image.
set -e

AVD_NAME="Pixel_6_API_34"

INSTALL_DIR="/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="/mnt/c/local"
[ ! -d "$INSTALL_DIR" ] && INSTALL_DIR="C:/local"

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

echo "Creating AVD '$AVD_NAME' (Pixel 6, API 34, x86_64)..."
echo "no" | "$AVDMANAGER" create avd \
    --name "$AVD_NAME" \
    --package "$IMAGE" \
    --device "pixel_6" \
    --force

# Apply hardware settings for a reasonable emulator experience
AVD_DIR="$HOME/.android/avd/${AVD_NAME}.avd"
if [ -f "$AVD_DIR/config.ini" ]; then
    cat >> "$AVD_DIR/config.ini" <<'EOF'
hw.ramSize=4096
hw.gpu.enabled=yes
hw.gpu.mode=auto
disk.dataPartition.size=4096M
hw.keyboard=yes
EOF
    echo "Applied hardware config (4 GB RAM, GPU accel, keyboard)."
fi

echo "AVD '$AVD_NAME' created."
echo "Launch with: run_emulator.sh"
if [ -z "$GET_ALL_RUNNING" ]; then
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
fi
