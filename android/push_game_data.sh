#!/bin/bash
# push_game_data.sh — Push game data files from game_data/ to the emulator.
# Usage:  bash push_game_data.sh
#
# Expects game data files (descent2.hog, groupa.pig, etc.) in
# <repo_root>/game_data/.  Pushes them to the app's internal storage
# via /data/local/tmp/ staging (necessary because run-as can't read /sdcard).
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GAME_DATA_DIR="$REPO_ROOT/game_data"
PACKAGE="com.dxxredux.app"

source "$SCRIPT_DIR/set_vars.sh"
echo ""

ADB="$ANDROID_HOME/platform-tools/adb"
[ ! -x "$ADB" ] && [ -f "$ADB.exe" ] && ADB="$ADB.exe"

if [ ! -d "$GAME_DATA_DIR" ]; then
    echo "ERROR: game_data/ directory not found at $GAME_DATA_DIR"
    echo "Place your Descent 2 game files (descent2.hog, groupa.pig, etc.) there."
    exit 1
fi

FILES=$(ls "$GAME_DATA_DIR" 2>/dev/null)
if [ -z "$FILES" ]; then
    echo "ERROR: No files found in $GAME_DATA_DIR"
    exit 1
fi

DEST="/data/data/$PACKAGE/files"

# Ensure the files directory exists
"$ADB" shell "run-as $PACKAGE mkdir -p $DEST" 2>/dev/null || true

echo "=== Pushing game data to emulator ==="
for f in "$GAME_DATA_DIR"/*; do
    [ ! -f "$f" ] && continue   # skip directories and non-files
    BASENAME=$(basename "$f")
    LOWER=$(echo "$BASENAME" | tr '[:upper:]' '[:lower:]')
    
    # Check if file already exists with correct size
    LOCAL_SIZE=$(wc -c < "$f" | tr -d ' ')
    REMOTE_SIZE=$("$ADB" shell "run-as $PACKAGE stat -c %s $DEST/$LOWER 2>/dev/null" | tr -d '\r\n' || echo "0")
    
    if [ "$LOCAL_SIZE" = "$REMOTE_SIZE" ]; then
        echo "  $BASENAME → $LOWER  (already present, skipping)"
        continue
    fi
    
    echo "  $BASENAME → $LOWER  ($LOCAL_SIZE bytes)"
    "$ADB" push "$f" "/data/local/tmp/$BASENAME" 2>/dev/null
    "$ADB" shell "run-as $PACKAGE sh -c 'cat /data/local/tmp/$BASENAME > $DEST/$LOWER'" 2>&1
    "$ADB" shell "rm /data/local/tmp/$BASENAME" 2>/dev/null || true
done

echo ""
echo "=== Files in app storage ==="
"$ADB" shell "run-as $PACKAGE ls -la $DEST/" 2>&1

# Remove files from emulator that are no longer in game_data/
echo ""
echo "=== Cleaning removed files ==="
REMOTE_FILES=$("$ADB" shell "run-as $PACKAGE ls $DEST/" 2>/dev/null | tr -d '\r')
for REMOTE in $REMOTE_FILES; do
    # Skip app-generated files (configs, saves, logs, etc.)
    case "$REMOTE" in
        *.cfg|*.txt|*.json|*.ngp|*.plr|*.plx|profileInstalled) continue ;;
    esac
    # Check if any local file (lowercased) matches this remote file
    FOUND=false
    for f in "$GAME_DATA_DIR"/*; do
        [ ! -f "$f" ] && continue
        LOCAL_LOWER=$(basename "$f" | tr '[:upper:]' '[:lower:]')
        if [ "$LOCAL_LOWER" = "$REMOTE" ]; then
            FOUND=true
            break
        fi
    done
    if [ "$FOUND" = "false" ]; then
        echo "  No longer in game_data/, removing $REMOTE"
        "$ADB" shell "run-as $PACKAGE rm -f $DEST/$REMOTE" 2>/dev/null || true
    fi
done
echo ""
echo "Done."
echo ""
echo "Press any key to exit..."
read -r -n1 -s
