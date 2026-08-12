#!/bin/bash
# push_game_data.sh - Push game data files from game_data_to_copy_to_emulator/.
# Usage:  bash push_game_data.sh [--sync-owned]
#
# Expects game data files in two subdirectories:
#   data/     -- HOG, PIG, etc. plus music (BIN/CUE, GOG/INST).
#                Pushed to the app's active file set (sets/default/).
#   download/ -- Files staged in a dedicated public Downloads subdirectory.
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
ANDROID_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$ANDROID_DIR/.." && pwd)"
GAME_DATA_DIR="$REPO_ROOT/game_data_to_copy_to_emulator"
PACKAGE="com.dxxredux.app"
SYNC_OWNED=0
if [ "${1:-}" = "--sync-owned" ]; then
    SYNC_OWNED=1
    shift
fi
if [ "$#" -ne 0 ]; then
    echo "Usage: $0 [--sync-owned]" >&2
    exit 2
fi

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
    local limit=$1
    shift
    "$@" &
    local pid=$!
    (
        sleep "$limit"
        kill "$pid" 2>/dev/null
    ) &
    local watcher=$!
    if wait "$pid" 2>/dev/null; then
        kill "$watcher" 2>/dev/null
        wait "$watcher" 2>/dev/null
        return 0
    else
        kill "$watcher" 2>/dev/null
        wait "$watcher" 2>/dev/null
        return 1
    fi
}

# Timeout (seconds) per file - scales with size: 30s base + ~1s per MB
_push_timeout() {
    local bytes=$1
    echo $((bytes / 1000000 + 30))
}

if [ ! -d "$GAME_DATA_DIR" ]; then
    echo "NOTE: game_data_to_copy_to_emulator/ directory not found at $GAME_DATA_DIR"
    echo "Place game files in data/ and/or download/ subdirectories"
fi

DATA_DIR="$GAME_DATA_DIR/data"
DOWNLOAD_DIR="$GAME_DATA_DIR/download"

# At least one subfolder must have files (ignore .gitkeep)
_has_files() {
    for _f in "$1"/*; do
        [ -f "$_f" ] && [ "$(basename "$_f")" != ".gitkeep" ] && return 0
    done
    return 1
}
if ! _has_files "$DATA_DIR" && ! _has_files "$DOWNLOAD_DIR"; then
    echo "NOTE: No files found in data/ or download/ under $GAME_DATA_DIR"
    if [ "$SYNC_OWNED" != "1" ]; then
        echo "Nothing to push; leaving all device files unchanged"
        exit 0
    fi
fi

if [ "$SYNC_OWNED" = "1" ]; then
    if [ -z "${ANDROID_SERIAL:-}" ]; then
        echo "ERROR: --sync-owned requires an explicit ANDROID_SERIAL" >&2
        exit 2
    fi
    SELECTED_SERIAL=$("$ADB" get-serialno 2>/dev/null | tr -d '\r\n')
    if [ "$SELECTED_SERIAL" != "$ANDROID_SERIAL" ]; then
        echo "ERROR: selected adb device '$SELECTED_SERIAL' does not match ANDROID_SERIAL '$ANDROID_SERIAL'" >&2
        exit 2
    fi
    echo "Explicit owned-file sync enabled for device $SELECTED_SERIAL"
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
    "$ADB" shell "run-as $PACKAGE cat $FILES_DIR/file_sets.json" >"$TMPJSON" 2>/dev/null
    sed -i 's/"active"[[:space:]]*:[[:space:]]*"[^"]*"/"active": "default"/' "$TMPJSON"
    "$ADB" push "$(_host_path "$TMPJSON")" /data/local/tmp/_fsets.json >/dev/null 2>&1
    "$ADB" shell "run-as $PACKAGE sh -c 'cat /data/local/tmp/_fsets.json > $FILES_DIR/file_sets.json'" 2>/dev/null
    "$ADB" shell "rm -f /data/local/tmp/_fsets.json" 2>/dev/null || true
    rm -f "$TMPJSON" 2>/dev/null || true
fi

echo "=== Pushing game data to emulator ==="
ERRORS=0
DATA_DESIRED="${TMPDIR:-/tmp}/dxx-push-data-$$.txt"
DOWNLOAD_DESIRED="${TMPDIR:-/tmp}/dxx-push-download-$$.txt"
DATA_EXISTING="${TMPDIR:-/tmp}/dxx-push-data-existing-$$.txt"
DATA_MERGED="${TMPDIR:-/tmp}/dxx-push-data-merged-$$.txt"
DOWNLOAD_EXISTING="${TMPDIR:-/tmp}/dxx-push-download-existing-$$.txt"
DOWNLOAD_MERGED="${TMPDIR:-/tmp}/dxx-push-download-merged-$$.txt"
: >"$DATA_DESIRED"
: >"$DOWNLOAD_DESIRED"
_cleanup_push_files() {
    rm -f "$DATA_DESIRED" "$DOWNLOAD_DESIRED" "$DATA_EXISTING" "$DATA_MERGED" "$DOWNLOAD_EXISTING" "$DOWNLOAD_MERGED"
    if [ "${CALLED_FROM_SCRIPT:-0}" != "1" ]; then
        wait_for_key
    fi
}
trap _cleanup_push_files EXIT

_remote_quote() {
    printf "'%s'" "$(printf '%s' "$1" | sed "s/'/'\"'\"'/g")"
}

_local_sha256() {
    sha256sum "$1" | awk '{print $1}'
}

_remote_sha256() {
    local path="$1" access="$2"
    if [ "$access" = "app" ]; then
        "$ADB" shell "run-as $PACKAGE sha256sum $path" 2>/dev/null | awk '{print $1}' | tr -d '\r\n'
    else
        "$ADB" shell "sha256sum $path" 2>/dev/null | awk '{print $1}' | tr -d '\r\n'
    fi
}

# _push_files <local_dir> <remote_dest>
# Push all regular files from local_dir to remote_dest, lowercasing names.
_push_files() {
    local src_dir="$1" dest_dir="$2"
    for f in "$src_dir"/*; do
        [ ! -f "$f" ] && continue
        BASENAME=$(basename "$f")
        [ "$BASENAME" = ".gitkeep" ] && continue
        LOWER=$(echo "$BASENAME" | tr '[:upper:]' '[:lower:]')
        REMOTE_PATH=$(_remote_quote "$dest_dir/$LOWER")
        STAGED_PATH=$(_remote_quote "/data/local/tmp/$LOWER")
        REMOTE_TEMP=$(_remote_quote "$dest_dir/.$LOWER.push-$$.tmp")

        LOCAL_SIZE=$(wc -c <"$f" | tr -d ' ')
        LOCAL_SHA=$(_local_sha256 "$f")
        if [ -z "$LOCAL_SHA" ]; then
            echo "    ERROR: cannot hash $BASENAME"
            ERRORS=$((ERRORS + 1))
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

        "$ADB" shell "chmod 644 $STAGED_PATH" 2>/dev/null || true
        if ! _timed "$TIMEOUT" "$ADB" shell "run-as $PACKAGE sh -c 'cat $STAGED_PATH > $REMOTE_TEMP'" 2>/dev/null; then
            echo "    ERROR: copy to app storage failed or timed out for $BASENAME"
            ERRORS=$((ERRORS + 1))
        else
            REMOTE_SHA=$(_remote_sha256 "$REMOTE_TEMP" app)
            if [ "$REMOTE_SHA" != "$LOCAL_SHA" ]; then
                echo "    ERROR: staged digest mismatch for $BASENAME"
                ERRORS=$((ERRORS + 1))
            elif ! "$ADB" shell "run-as $PACKAGE mv -f $REMOTE_TEMP $REMOTE_PATH" >/dev/null 2>&1; then
                echo "    ERROR: atomic publication failed for $BASENAME"
                ERRORS=$((ERRORS + 1))
            elif [ "$(_remote_sha256 "$REMOTE_PATH" app)" != "$LOCAL_SHA" ]; then
                echo "    ERROR: final digest mismatch for $BASENAME"
                ERRORS=$((ERRORS + 1))
            else
                printf '%s\n' "$LOWER" >>"$DATA_DESIRED"
            fi
        fi
        "$ADB" shell "run-as $PACKAGE rm -f $REMOTE_TEMP" 2>/dev/null || true
        "$ADB" shell "rm -f $STAGED_PATH" 2>/dev/null || true
    done
}

# Push data/ files to set dir
if [ -d "$DATA_DIR" ] && _has_files "$DATA_DIR"; then
    echo "--- data/ -> $DEST ---"
    _push_files "$DATA_DIR" "$DEST"
fi

DEVICE_DOWNLOAD_DIR="/sdcard/Download/dxx-redux-test-data"
# Push download/ files to public Downloads folder
if [ -d "$DOWNLOAD_DIR" ] && _has_files "$DOWNLOAD_DIR"; then
    echo "--- download/ -> $DEVICE_DOWNLOAD_DIR ---"
    "$ADB" shell "mkdir -p $DEVICE_DOWNLOAD_DIR" 2>/dev/null || true
    for f in "$DOWNLOAD_DIR"/*; do
        [ ! -f "$f" ] && continue
        BASENAME=$(basename "$f")
        [ "$BASENAME" = ".gitkeep" ] && continue
        LOWER=$(echo "$BASENAME" | tr '[:upper:]' '[:lower:]')
        REMOTE_PATH=$(_remote_quote "$DEVICE_DOWNLOAD_DIR/$LOWER")
        REMOTE_TEMP=$(_remote_quote "$DEVICE_DOWNLOAD_DIR/.$LOWER.push-$$.tmp")

        HOST_FILE=$(_host_path "$f")
        TIMEOUT=$(_push_timeout "$(wc -c <"$f")")
        LOCAL_SHA=$(_local_sha256 "$f")
        if [ -z "$LOCAL_SHA" ]; then
            echo "    ERROR: cannot hash $BASENAME"
            ERRORS=$((ERRORS + 1))
            continue
        fi

        echo "  $BASENAME -> $LOWER"
        if ! _timed "$TIMEOUT" "$ADB" push "$HOST_FILE" "$DEVICE_DOWNLOAD_DIR/.$LOWER.push-$$.tmp" >/dev/null 2>&1; then
            echo "    ERROR: adb push failed or timed out (${TIMEOUT}s) for $BASENAME"
            ERRORS=$((ERRORS + 1))
        elif [ "$(_remote_sha256 "$REMOTE_TEMP" public)" != "$LOCAL_SHA" ]; then
            echo "    ERROR: staged digest mismatch for $BASENAME"
            ERRORS=$((ERRORS + 1))
        elif ! "$ADB" shell "mv -f $REMOTE_TEMP $REMOTE_PATH" >/dev/null 2>&1; then
            echo "    ERROR: atomic publication failed for $BASENAME"
            ERRORS=$((ERRORS + 1))
        elif [ "$(_remote_sha256 "$REMOTE_PATH" public)" != "$LOCAL_SHA" ]; then
            echo "    ERROR: final digest mismatch for $BASENAME"
            ERRORS=$((ERRORS + 1))
        else
            printf '%s\n' "$LOWER" >>"$DOWNLOAD_DESIRED"
        fi
        "$ADB" shell "rm -f $REMOTE_TEMP" >/dev/null 2>&1 || true
    done
fi

echo ""
echo "=== Files in app storage ==="
"$ADB" shell "run-as $PACKAGE ls -la $DEST/" 2>&1 || true

# Cleanup is deliberately manifest-based. Never enumerate either destination
# as an authoritative mirror: both can contain files owned by users or the app.
DATA_OWNED_REMOTE="$DEST/.push_game_data_owned"
DOWNLOAD_OWNED_REMOTE="$DEVICE_DOWNLOAD_DIR/.push_game_data_owned"

_sync_owned_manifest() {
    local desired="$1" existing="$2" merged="$3" destination="$4" manifest="$5" access="$6"
    local errors_before=$ERRORS
    if [ "$access" = "app" ]; then
        "$ADB" exec-out run-as "$PACKAGE" cat "$manifest" 2>/dev/null | tr -d '\r' >"$existing" || true
    else
        "$ADB" exec-out cat "$manifest" 2>/dev/null | tr -d '\r' >"$existing" || true
    fi

    if [ "$SYNC_OWNED" = "1" ]; then
        while IFS= read -r owned; do
            [ -z "$owned" ] && continue
            case "$owned" in
            */* | "." | "..")
                echo "ERROR: invalid owned-file manifest entry '$owned'" >&2
                ERRORS=$((ERRORS + 1))
                continue
                ;;
            esac
            if ! grep -Fqx -- "$owned" "$desired"; then
                echo "  Removing previously helper-owned $owned"
                local owned_path
                owned_path=$(_remote_quote "$destination/$owned")
                if [ "$access" = "app" ]; then
                    "$ADB" shell "run-as $PACKAGE rm -f $owned_path" >/dev/null 2>&1 || ERRORS=$((ERRORS + 1))
                else
                    "$ADB" shell "rm -f $owned_path" >/dev/null 2>&1 || ERRORS=$((ERRORS + 1))
                fi
            fi
        done <"$existing"
        if [ "$ERRORS" -ne "$errors_before" ]; then
            echo "ERROR: owned-file cleanup failed; preserving the prior ownership manifest" >&2
            return
        fi
        sort -u "$desired" >"$merged"
    else
        sort -u "$existing" "$desired" >"$merged"
    fi

    local staged="/data/local/tmp/dxx-push-owned-$$.txt"
    if ! "$ADB" push "$(_host_path "$merged")" "$staged" >/dev/null 2>&1; then
        ERRORS=$((ERRORS + 1))
        return
    fi
    if [ "$access" = "app" ]; then
        "$ADB" shell "run-as $PACKAGE sh -c 'cat $staged > $manifest'" >/dev/null 2>&1 || ERRORS=$((ERRORS + 1))
    else
        "$ADB" shell "sh -c 'cat $staged > $manifest'" >/dev/null 2>&1 || ERRORS=$((ERRORS + 1))
    fi
    "$ADB" shell "rm -f $staged" >/dev/null 2>&1 || true
}

echo ""
echo "=== Updating helper ownership manifests ==="
if [ "$ERRORS" -eq 0 ]; then
    "$ADB" shell "mkdir -p $DEVICE_DOWNLOAD_DIR" >/dev/null 2>&1 || ERRORS=$((ERRORS + 1))
    _sync_owned_manifest "$DATA_DESIRED" "$DATA_EXISTING" "$DATA_MERGED" "$DEST" "$DATA_OWNED_REMOTE" app
    _sync_owned_manifest "$DOWNLOAD_DESIRED" "$DOWNLOAD_EXISTING" "$DOWNLOAD_MERGED" "$DEVICE_DOWNLOAD_DIR" "$DOWNLOAD_OWNED_REMOTE" public
else
    echo "Skipping ownership cleanup and publication because one or more pushes failed"
fi
echo ""
if [ "$ERRORS" -gt 0 ]; then
    echo "Done with $ERRORS error(s)."
    exit 1
else
    echo "Done. All files pushed successfully"
fi
