#!/bin/bash
# run_emulator.sh — Build the APK, launch the emulator, install and run the app.
# Usage:  bash run_emulator.sh          (build + launch + install)
#         bash run_emulator.sh --no-build   (skip build, just launch + install)
set -e

AVD_NAME="Pixel_6_API_34"
PACKAGE="com.dxxredux.app"
ACTIVITY="com.dxxredux.app.MainActivity"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ANDROID_DIR="$SCRIPT_DIR"

# Source environment
source "$ANDROID_DIR/set_vars.sh"
echo ""

SDK_DIR="$ANDROID_HOME"
EMULATOR="$SDK_DIR/emulator/emulator"
ADB="$SDK_DIR/platform-tools/adb"

# Windows exe fallbacks
[ ! -x "$EMULATOR" ] && [ -f "$EMULATOR.exe" ] && EMULATOR="$EMULATOR.exe"
[ ! -x "$ADB" ]      && [ -f "$ADB.exe" ]      && ADB="$ADB.exe"

if [ ! -f "$EMULATOR" ] && [ ! -f "$EMULATOR.exe" ]; then
    echo "ERROR: Emulator not found. Run get_emulator.sh first."
    exit 1
fi

# ── 1. Build APK ────────────────────────────────────────────
if [ "$1" != "--no-build" ]; then
    echo "=== Building APK ==="
    cd "$ANDROID_DIR"
    ./gradlew assembleDebug
    echo ""
fi

APK="$ANDROID_DIR/app/build/outputs/apk/debug/app-debug.apk"
if [ ! -f "$APK" ]; then
    echo "ERROR: APK not found at $APK"
    echo "Run without --no-build, or run build.sh first."
    exit 1
fi

# ── 2. Launch emulator (if not already running) ─────────────
echo "=== Launching emulator ($AVD_NAME) ==="

# Check if emulator is already running
RUNNING=$("$ADB" devices 2>/dev/null | grep -c "emulator-" || true)
if [ "$RUNNING" -gt 0 ]; then
    echo "Emulator already running."
else
    # Launch in background; -no-snapshot for clean boot
    "$EMULATOR" -avd "$AVD_NAME" -no-snapshot -gpu auto &
    EMULATOR_PID=$!
    echo "Emulator started (PID $EMULATOR_PID). Waiting for boot..."
fi

# ── 3. Wait for device ──────────────────────────────────────
echo "Waiting for device to come online..."
"$ADB" wait-for-device

echo "Waiting for boot to complete..."
BOOT_COMPLETE=""
for i in $(seq 1 120); do
    BOOT_COMPLETE=$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r\n' || true)
    if [ "$BOOT_COMPLETE" = "1" ]; then
        break
    fi
    sleep 2
done

if [ "$BOOT_COMPLETE" != "1" ]; then
    echo "ERROR: Emulator did not finish booting within 4 minutes."
    exit 1
fi
echo "Device booted."

# ── 4. Install APK ──────────────────────────────────────────
echo ""
echo "=== Installing APK ==="
"$ADB" install -r "$APK"

# ── 5. Launch the app ───────────────────────────────────────
echo ""
echo "=== Launching $PACKAGE ==="
"$ADB" shell am start -n "$PACKAGE/$ACTIVITY"

echo ""
echo "=== App launched. Check logcat for native library output: ==="
echo "  $ADB logcat -s DXX-Redux:V ActivityManager:I AndroidRuntime:E"
echo ""
echo "Quick logcat (10 seconds):"
timeout 10 "$ADB" logcat -s "DXX-Redux:V" "AndroidRuntime:E" "DEBUG:V" 2>/dev/null || true
