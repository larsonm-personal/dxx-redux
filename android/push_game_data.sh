#!/bin/bash
# push_game_data.sh - Push game data files from game_data_to_copy_to_emulator/.
# Usage:  bash push_game_data.sh
#
# Expects game data files in two subdirectories:
#   data/     -- HOG, PIG, etc. plus music (BIN/CUE, GOG/INST).
#                Pushed to the app's active file set (sets/default/).
#   download/ -- Files destined for the app's files dir root.
#
# Files are staged via /data/local/tmp/ (run-as can't read /sdcard).

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

# Timeout (seconds) per file - scales with size: 30s base + ~1s per MB
_push_timeout() {
    local bytes=$1
    echo $(( bytes / 1000000 + 30 ))
}

if [ ! -d "$GAME_DATA_DIR" ]; then
    echo "NOTE: game_data_to_copy_to_emulator/ directory not found at $GAME_DATA_DIR"
    echo "Place game files in data/ and/or download/ subdirectories."
fi

DATA_DIR="$GAME_DATA_DIR/data"
DOWNLOAD_DIR="$GAME_DATA_DIR/download"

# At least one subfolder must have files (ignore .gitkeep)
_has_files() {
    ls -p "$1" 2>/dev/null | grep -v '/$' | grep -v '^\.gitkeep$' >/dev/null
}
if ! _has_files "$DATA_DIR" && ! _has_files "$DOWNLOAD_DIR"; then
    echo "NOTE: No files found in data/ or download/ under $GAME_DATA_DIR"
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

# _push_files <local_dir> <remote_dest>
# Push all regular files from local_dir to remote_dest, lowercasing names.
_push_files() {
    local src_dir="$1" dest_dir="$2"
    for f in "$src_dir"/*; do
        [ ! -f "$f" ] && continue
        BASENAME=$(basename "$f")
        [ "$BASENAME" = ".gitkeep" ] && continue
        LOWER=$(echo "$BASENAME" | tr '[:upper:]' '[:lower:]')

        LOCAL_SIZE=$(wc -c < "$f" | tr -d ' ')
        REMOTE_SIZE=$("$ADB" shell "run-as $PACKAGE stat -c %s $dest_dir/$LOWER 2>/dev/null" 2>/dev/null | tr -d '\r\n') || true
        [ -z "$REMOTE_SIZE" ] && REMOTE_SIZE=0

        if [ "$LOCAL_SIZE" = "$REMOTE_SIZE" ]; then
            echo "  $BASENAME -> $LOWER  (already present, skipping)"
            continue
        fi

        echo "  $BASENAME -> $LOWER  ($LOCAL_SIZE bytes)"

        HOST_FILE=$(_host_path "$f")
        TIMEOUT=$(_push_timeout "$LOCAL_SIZE")
        if ! _timed "$TIMEOUT" "$ADB" push "$HOST_FILE" "/data/local/tmp/$LOWER" >/dev/null 2>&1; then
            echo "    ERROR: adb push failed or timed out (${TIMEOUT}s) for $BASENAME"
            ERRORS=$((ERRORS + 1))
            continue
        fi

        "$ADB" shell "chmod 644 /data/local/tmp/$LOWER" 2>/dev/null || true
        if ! _timed "$TIMEOUT" "$ADB" shell "run-as $PACKAGE sh -c 'cat /data/local/tmp/$LOWER > $dest_dir/$LOWER'" 2>/dev/null; then
            echo "    ERROR: copy to app storage failed or timed out for $BASENAME"
            ERRORS=$((ERRORS + 1))
        fi
        "$ADB" shell "rm -f /data/local/tmp/$LOWER" 2>/dev/null || true
    done
}

# Push data/ files to set dir
if [ -d "$DATA_DIR" ] && _has_files "$DATA_DIR"; then
    echo "--- data/ -> $DEST ---"
    _push_files "$DATA_DIR" "$DEST"
fi

DEVICE_DOWNLOAD_DIR="/sdcard/Download"
# Push download/ files to public Downloads folder
if [ -d "$DOWNLOAD_DIR" ] && _has_files "$DOWNLOAD_DIR"; then
    echo "--- download/ -> $DEVICE_DOWNLOAD_DIR ---"
    "$ADB" shell "mkdir -p $DEVICE_DOWNLOAD_DIR" 2>/dev/null || true
    for f in "$DOWNLOAD_DIR"/*; do
        [ ! -f "$f" ] && continue
        BASENAME=$(basename "$f")
        [ "$BASENAME" = ".gitkeep" ] && continue
        LOWER=$(echo "$BASENAME" | tr '[:upper:]' '[:lower:]')

        HOST_FILE=$(_host_path "$f")
        TIMEOUT=$(_push_timeout "$(wc -c < "$f")")

        echo "  $BASENAME -> $LOWER"
        if ! _timed "$TIMEOUT" "$ADB" push "$HOST_FILE" "$DEVICE_DOWNLOAD_DIR/$LOWER" >/dev/null 2>&1; then
            echo "    ERROR: adb push failed or timed out (${TIMEOUT}s) for $BASENAME"
        fi
    done
fi

echo ""
echo "=== Files in app storage ==="
"$ADB" shell "run-as $PACKAGE ls -la $DEST/" 2>&1 || true

# Remove set-dir files no longer in data/
echo ""
echo "=== Cleaning removed files ==="
REMOTE_FILES=$("$ADB" shell "run-as $PACKAGE ls $DEST/" 2>/dev/null | tr -d '\r') || true
for REMOTE in $REMOTE_FILES; do
    case "$REMOTE" in
        *.json|*.cfg) continue ;;
    esac
    FOUND=false
    if [ -d "$DATA_DIR" ]; then
        for f in "$DATA_DIR"/*; do
            [ ! -f "$f" ] && continue
            LOCAL_LOWER=$(basename "$f" | tr '[:upper:]' '[:lower:]')
            if [ "$LOCAL_LOWER" = "$REMOTE" ]; then
                FOUND=true
                break
            fi
        done
    fi
    if [ "$FOUND" = "false" ]; then
        echo "  No longer in data/, removing $REMOTE"
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
