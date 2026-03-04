#!/bin/bash
# push_game_data.sh — Push game data files from game_data/ to the emulator.
# Usage:  bash push_game_data.sh
#
# Expects game data files (descent2.hog, groupa.pig, etc.) in
# <repo_root>/game_data/.  Pushes them to the app's internal storage
# via /data/local/tmp/ staging (necessary because run-as can't read /sdcard).

wait_for_key() {
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
}
trap wait_for_key EXIT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GAME_DATA_DIR="$REPO_ROOT/game_data"
PACKAGE="com.dxxredux.app"

source "$SCRIPT_DIR/set_vars.sh"
echo ""

ADB="$ANDROID_HOME/platform-tools/adb"
[ ! -x "$ADB" ] && [ -f "$ADB.exe" ] && ADB="$ADB.exe"

# When running in WSL, adb.exe needs Windows-style paths for local files.
# Git Bash/MSYS adb.exe handles Unix-style paths natively.
_host_path() {
    if command -v wslpath >/dev/null 2>&1; then
        wslpath -w "$1"
    else
        echo "$1"
    fi
}

# Run a command with a timeout (seconds).  Works with Windows .exe under WSL
# where coreutils `timeout` can misbehave.
# Usage: _timed <seconds> <cmd> [args...]
_timed() {
    local limit=$1; shift
    "$@" &
    local pid=$!
    ( sleep "$limit"; kill "$pid" 2>/dev/null ) &
    local watcher=$!
    if wait "$pid" 2>/dev/null; then
        kill "$watcher" 2>/dev/null; wait "$watcher" 2>/dev/null
        return 0
    else
        kill "$watcher" 2>/dev/null; wait "$watcher" 2>/dev/null
        return 1
    fi
}

# Timeout (seconds) per file — scales with size: 30s base + ~1s per MB
_push_timeout() {
    local bytes=$1
    echo $(( bytes / 1000000 + 30 ))
}

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
ERRORS=0
for f in "$GAME_DATA_DIR"/*; do
    [ ! -f "$f" ] && continue   # skip directories and non-files
    BASENAME=$(basename "$f")
    LOWER=$(echo "$BASENAME" | tr '[:upper:]' '[:lower:]')
    
    # Check if file already exists with correct size
    LOCAL_SIZE=$(wc -c < "$f" | tr -d ' ')
    REMOTE_SIZE=$("$ADB" shell "run-as $PACKAGE stat -c %s $DEST/$LOWER 2>/dev/null" 2>/dev/null | tr -d '\r\n') || true
    [ -z "$REMOTE_SIZE" ] && REMOTE_SIZE=0
    
    if [ "$LOCAL_SIZE" = "$REMOTE_SIZE" ]; then
        echo "  $BASENAME → $LOWER  (already present, skipping)"
        continue
    fi
    
    echo "  $BASENAME → $LOWER  ($LOCAL_SIZE bytes)"
    
    # Stage file to /data/local/tmp via adb push (use Windows path for adb.exe)
    HOST_FILE=$(_host_path "$f")
    TIMEOUT=$(_push_timeout "$LOCAL_SIZE")
    if ! _timed "$TIMEOUT" "$ADB" push "$HOST_FILE" "/data/local/tmp/$LOWER" >/dev/null 2>&1; then
        echo "    ERROR: adb push failed or timed out (${TIMEOUT}s) for $BASENAME"
        ERRORS=$((ERRORS + 1))
        continue
    fi
    
    # Copy from staging to app-private storage via run-as
    "$ADB" shell "chmod 644 /data/local/tmp/$LOWER" 2>/dev/null || true
    if ! _timed "$TIMEOUT" "$ADB" shell "run-as $PACKAGE sh -c 'cat /data/local/tmp/$LOWER > $DEST/$LOWER'" 2>/dev/null; then
        echo "    ERROR: copy to app storage failed or timed out for $BASENAME"
        ERRORS=$((ERRORS + 1))
    fi
    "$ADB" shell "rm -f /data/local/tmp/$LOWER" 2>/dev/null || true
done

echo ""
echo "=== Files in app storage ==="
"$ADB" shell "run-as $PACKAGE ls -la $DEST/" 2>&1 || true

# Remove files from emulator that are no longer in game_data/
echo ""
echo "=== Cleaning removed files ==="
REMOTE_FILES=$("$ADB" shell "run-as $PACKAGE ls $DEST/" 2>/dev/null | tr -d '\r') || true
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
if [ "$ERRORS" -gt 0 ]; then
    echo "Done with $ERRORS error(s)."
    exit 1
else
    echo "Done. All files pushed successfully."
fi
