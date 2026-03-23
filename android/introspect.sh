#!/usr/bin/env bash
# introspect.sh - grab game/setup state from a running DXX-Redux Android emulator
# Usage:
#   ./android/introspect.sh            # pretty-print full game state
#   ./android/introspect.sh raw        # raw JSON (no formatting)
#   ./android/introspect.sh menu       # show only the menu section
#   ./android/introspect.sh player     # show only the player section
#   ./android/introspect.sh position   # show only the position section
#   ./android/introspect.sh console    # show recent con_printf output (ring buffer)
#   ./android/introspect.sh setup      # dump SetupActivity state (files, readiness, downloads)
#   ./android/introspect.sh setup raw  # raw JSON from SetupActivity
#   ./android/introspect.sh autolog    # dump automation step log (automation_log.jsonl)
#   ./android/introspect.sh autoresult # dump automation result (automation_result.json)

set -euo pipefail

# Find adb: honour $ADB, then PATH, then common Windows SDK location
if [[ -n "${ADB:-}" ]]; then
    : # already set
elif command -v adb &>/dev/null; then
    ADB="adb"
elif [[ -x "/mnt/c/local/android-sdk/platform-tools/adb.exe" ]]; then
    ADB="/mnt/c/local/android-sdk/platform-tools/adb.exe"
elif [[ -x "/c/local/android-sdk/platform-tools/adb.exe" ]]; then
    ADB="/c/local/android-sdk/platform-tools/adb.exe"
elif [[ -n "${LOCALAPPDATA:-}" && -x "${LOCALAPPDATA}/Android/Sdk/platform-tools/adb.exe" ]]; then
    ADB="${LOCALAPPDATA}/Android/Sdk/platform-tools/adb.exe"
else
    echo "ERROR: adb not found. Set ADB= or add it to PATH." >&2
    exit 1
fi

PACKAGE="com.dxxredux.app"
ACTION="com.dxxredux.INTROSPECT"

# For non-setup modes, request a game-state dump
JSON=""
if [[ "${1:-}" != "setup" ]]; then
    "$ADB" shell am broadcast -a "$ACTION" >/dev/null 2>&1 || {
        echo "ERROR: broadcast failed - is the emulator running?" >&2
        exit 1
    }

    # Wait for the game thread to write the file
    sleep 1

    # Read the JSON
    JSON=$("$ADB" shell run-as "$PACKAGE" cat files/introspect.json 2>/dev/null) || {
        echo "ERROR: Could not read introspect.json. Is the game running?" >&2
        exit 1
    }
fi

# Pick a formatter
pretty_print() {
    if command -v python3 &>/dev/null; then
        python3 -m json.tool <<<"$1"
    elif command -v jq &>/dev/null; then
        jq . <<<"$1"
    else
        echo "$1"
    fi
}

extract_key() {
    local key="$1" json="$2"
    if command -v python3 &>/dev/null; then
        python3 -c "
import json, sys
d = json.loads(sys.stdin.read())
v = d.get('$key')
if v is None:
    print('(not present in current state)')
else:
    print(json.dumps(v, indent=2))
" <<<"$json"
    elif command -v jq &>/dev/null; then
        jq ".$key // \"(not present in current state)\"" <<<"$json"
    else
        echo "(install python3 or jq to extract fields)"
        echo "$json"
    fi
}

case "${1:-}" in
setup)
    # Setup-screen introspection - different broadcast & file
    "$ADB" shell am broadcast -a com.dxxredux.SETUP_INTROSPECT >/dev/null 2>&1 || {
        echo "ERROR: broadcast failed - is the emulator running?" >&2
        exit 1
    }
    sleep 1
    SJSON=$("$ADB" shell run-as "$PACKAGE" cat files/setup_introspect.json 2>/dev/null) || {
        echo "ERROR: Could not read setup_introspect.json. Is SetupActivity visible?" >&2
        exit 1
    }
    if [[ "${2:-}" == "raw" ]]; then
        echo "$SJSON"
    else
        pretty_print "$SJSON"
    fi
    ;;
raw)
    echo "$JSON"
    ;;
menu)
    extract_key menu "$JSON"
    ;;
player)
    extract_key player "$JSON"
    ;;
position)
    extract_key position "$JSON"
    ;;
console)
    extract_key console "$JSON"
    ;;
autolog)
    # Dump automation step log (file-based, no introspection needed)
    ALOG=$("$ADB" shell run-as "$PACKAGE" cat files/automation_log.jsonl 2>/dev/null) || {
        echo "ERROR: Could not read automation_log.jsonl." >&2
        exit 1
    }
    echo "$ALOG"
    ;;
autoresult)
    # Dump automation result (file-based, no introspection needed)
    ARES=$("$ADB" shell run-as "$PACKAGE" cat files/automation_result.json 2>/dev/null) || {
        echo "ERROR: Could not read automation_result.json." >&2
        exit 1
    }
    if [[ "${2:-}" == "raw" ]]; then
        echo "$ARES"
    else
        pretty_print "$ARES"
    fi
    ;;
*)
    pretty_print "$JSON"
    ;;
esac
echo ""
echo "Press any key to exit..."
read -r -n1 -s
