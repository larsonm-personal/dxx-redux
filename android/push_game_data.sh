#!/bin/bash
# push_game_data.sh — Push game data files from game_data_to_copy_to_emulator/.
# Usage:  bash push_game_data.sh
#
# Expects game data files (descent2.hog, groupa.pig, etc.) in
# <repo_root>/game_data_to_copy_to_emulator/.  Pushes them to the app's internal storage
# via /data/local/tmp/ staging (necessary because run-as can't read /sdcard).

wait_for_key() {
    echo ""
    echo "Press any key to exit..."
    read -r -n1 -s
}
if [ "${CALLED_FROM_SCRIPT:-0}" != "1" ]; then
    trap wait_for_key EXIT
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
GAME_DATA_DIR="$REPO_ROOT/game_data_to_copy_to_emulator"
PACKAGE="com.dxxredux.app"

source "$SCRIPT_DIR/set_vars.sh"

# Prevent MSYS/Git Bash from mangling Unix-style device paths
# (e.g. /data/local/tmp/) into Windows paths when calling adb.exe.
# Local paths are converted explicitly via _host_path/cygpath.
export MSYS_NO_PATHCONV=1

echo ""

ADB="$ANDROID_HOME/platform-tools/adb"
[ ! -x "$ADB" ] && [ -f "$ADB.exe" ] && ADB="$ADB.exe"

# When running in WSL, adb.exe needs Windows-style paths for local files.
# Git Bash/MSYS adb.exe handles Unix-style paths natively.
_host_path() {
    if command -v wslpath >/dev/null 2>&1; then
        wslpath -w "$1"
    elif command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
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
    echo "ERROR: game_data_to_copy_to_emulator/ directory not found at $GAME_DATA_DIR"
    echo "Place your Descent 2 game files (descent2.hog, groupa.pig, etc.) there."
    exit 1
fi

FILES=$(ls "$GAME_DATA_DIR" 2>/dev/null)
if [ -z "$FILES" ]; then
    echo "ERROR: No files found in $GAME_DATA_DIR"
    exit 1
fi

FILES_DIR="/data/data/$PACKAGE/files"
DEST="$FILES_DIR/sets/default"

# Ensure the set directory exists
"$ADB" shell "run-as $PACKAGE mkdir -p $DEST" 2>/dev/null || true

# Ensure "default" is the active file set (a test run may have switched it)
CURRENT_ACTIVE=$("$ADB" shell "run-as $PACKAGE cat $FILES_DIR/file_sets.json 2>/dev/null" 2>/dev/null \
    | grep -o '"active"[^"]*"[^"]*"' | grep -o '"[^"]*"$' | tr -d '"' || true)
if [ -n "$CURRENT_ACTIVE" ] && [ "$CURRENT_ACTIVE" != "default" ]; then
    echo "  (Resetting active file set from '$CURRENT_ACTIVE' to 'default')"
    TMPJSON="${TMPDIR:-/tmp}/_fsets_$$.json"
    "$ADB" shell "run-as $PACKAGE cat $FILES_DIR/file_sets.json" > "$TMPJSON" 2>/dev/null
    sed -i 's/"active"[[:space:]]*:[[:space:]]*"[^"]*"/"active": "default"/' "$TMPJSON"
    "$ADB" push "$(_host_path "$TMPJSON")" /data/local/tmp/_fsets.json >/dev/null 2>&1
    "$ADB" shell "run-as $PACKAGE sh -c 'cat /data/local/tmp/_fsets.json > $FILES_DIR/file_sets.json'" 2>/dev/null
    "$ADB" shell "rm -f /data/local/tmp/_fsets.json" 2>/dev/null || true
    rm -f "$TMPJSON" 2>/dev/null || true
fi

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

# Remove game files from set dir that are no longer in game_data_to_copy_to_emulator/
echo ""
echo "=== Cleaning removed files ==="
REMOTE_FILES=$("$ADB" shell "run-as $PACKAGE ls $DEST/" 2>/dev/null | tr -d '\r') || true
for REMOTE in $REMOTE_FILES; do
    # Skip manifests and metadata
    case "$REMOTE" in
        *.json|*.cfg) continue ;;
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
        echo "  No longer in game_data_to_copy_to_emulator/, removing $REMOTE"
        "$ADB" shell "run-as $PACKAGE rm -f $DEST/$REMOTE" 2>/dev/null || true
    fi
done

# Clean any stale game data from filesDir root (prevents PhysFS leaking)
echo "=== Cleaning filesDir root ==="
ROOT_FILES=$("$ADB" shell "run-as $PACKAGE ls $FILES_DIR/" 2>/dev/null | tr -d '\r') || true
for RF in $ROOT_FILES; do
    case "$RF" in
        *.pig|*.hog|*.ham|*.mvl|*.s11|*.s22|*.mn2|*.msn|*.dxa|*.pog|*.rl2|*.dtx)
            echo "  Removing stale $RF from filesDir root"
            "$ADB" shell "run-as $PACKAGE rm -f $FILES_DIR/$RF" 2>/dev/null || true
            ;;
    esac
done
echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo "Done with $ERRORS error(s)."
    exit 1
else
    echo "Done. All files pushed successfully."
fi
