#!/usr/bin/env bash
#
# run_automation.sh — Push and execute an automation script on the Android device.
#
# Usage:
#   ./android/run_automation.sh [script.json]           # push + run script
#   ./android/run_automation.sh                         # run default automate_to_automap.json
#   ./android/run_automation.sh --watch                 # run default + tail logcat
#   ./android/run_automation.sh script.json --watch     # push + run + tail logcat
#
# The script is pushed to the app's files directory, then a broadcast
# triggers the native automation engine to load and execute it.

set -euo pipefail

PACKAGE="com.dxxredux.app"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_SCRIPT="$SCRIPT_DIR/automate_to_automap.json"

WATCH=false
SCRIPT=""

for arg in "$@"; do
    case "$arg" in
        --watch|-w) WATCH=true ;;
        *)          SCRIPT="$arg" ;;
    esac
done

if [ -z "$SCRIPT" ]; then
    SCRIPT="$DEFAULT_SCRIPT"
fi

if [ ! -f "$SCRIPT" ]; then
    echo "ERROR: Script file not found: $SCRIPT" >&2
    exit 1
fi

BASENAME="$(basename "$SCRIPT")"
DEVICE_PATH="/data/local/tmp/$BASENAME"

echo "=== DXX-Redux Automation ==="
echo "Script:  $SCRIPT"
echo "Device:  $DEVICE_PATH"
echo ""

# Push the script to a temp location readable by adb
echo "Pushing script to device..."
adb push "$SCRIPT" "$DEVICE_PATH"

# Copy into the app's files directory (requires run-as)
echo "Copying to app files directory..."
adb shell "run-as $PACKAGE cp $DEVICE_PATH files/$BASENAME"

# Clean up temp file
adb shell "rm -f $DEVICE_PATH"

# Send the broadcast to trigger automation
# Note: The Kotlin receiver prepends filesDir (/data/.../files/) automatically,
# so we only pass the bare filename, not "files/..."
echo "Sending AUTOMATE broadcast..."
adb shell "am broadcast -a com.dxxredux.AUTOMATE --es script $BASENAME"

echo ""
echo "Automation started! Script will execute on the game thread."
echo ""

if $WATCH; then
    echo "Tailing logcat for automation output (Ctrl+C to stop)..."
    echo "────────────────────────────────────────"
    adb logcat -s DXX-Automate:I DXX-Introspect:I DXX-Redux:I DXX-Automap:I
fi
