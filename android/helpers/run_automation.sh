#!/usr/bin/env bash
#
# run_automation.sh - Push and execute automation test scripts on Android.
#
# Usage:
#   ./android/helpers/run_automation.sh [script.json5]          # run a single test
#   ./android/helpers/run_automation.sh                         # run default test
#   ./android/helpers/run_automation.sh --all                   # run all game_scripts/test_*.json5
#   ./android/helpers/run_automation.sh --watch script.json     # run + keep tailing logcat
#
# The script is pushed to the app's files directory, then a broadcast
# triggers the native automation engine to load and execute it.
#
# The runner monitors logcat for SCRIPT_RESULT: PASS or SCRIPT_RESULT: FAIL.
# Exit code 0 = all tests pass, 1 = at least one test failed.

set -euo pipefail

PACKAGE="com.dxxredux.app"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ANDROID_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Find adb: honour $ADB, then PATH, then common Windows SDK locations
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
    echo "ERROR: adb not found. Set ADB= or add it to PATH" >&2
    exit 1
fi
GAME_SCRIPTS_DIR="$ANDROID_DIR/game_scripts"
DEFAULT_SCRIPT="$GAME_SCRIPTS_DIR/test_launch_to_automap.json5"
TIMEOUT_SEC=120 # max time to wait for a single test

WATCH=false
WATCH=false
RUN_ALL=false
SCRIPTS=()

# shellcheck disable=SC2034  # WATCH reserved for future watch-and-rerun mode
for arg in "$@"; do
    case "$arg" in
    --watch | -w) WATCH=true ;;
    --all | -a) RUN_ALL=true ;;
    *) SCRIPTS+=("$arg") ;;
    esac
done

if $RUN_ALL; then
    SCRIPTS=()
    for f in "$GAME_SCRIPTS_DIR"/test_*.json5; do
        [ -f "$f" ] && SCRIPTS+=("$f")
    done
    if [ ${#SCRIPTS[@]} -eq 0 ]; then
        echo "ERROR: No test_*.json5 files found in $GAME_SCRIPTS_DIR" >&2
        exit 1
    fi
elif [ ${#SCRIPTS[@]} -eq 0 ]; then
    SCRIPTS=("$DEFAULT_SCRIPT")
fi

# -- Helpers -------------------------------------------------------------

push_script() {
    local script="$1"
    local basename
    basename="$(basename "$script")"
    local device_tmp="/data/local/tmp/$basename"

    "$ADB" push "$script" "$device_tmp" >/dev/null 2>&1
    "$ADB" shell "run-as $PACKAGE cp $device_tmp files/$basename" 2>/dev/null
    "$ADB" shell "rm -f $device_tmp" 2>/dev/null
    echo "$basename"
}

run_single_test() {
    local script="$1"
    local test_name
    test_name="$(basename "$script" .json)"

    if [ ! -f "$script" ]; then
        echo "ERROR: Script not found: $script" >&2
        return 1
    fi

    echo "----------------------------------------"
    echo "TEST: $test_name"
    echo "  Script: $script"

    # Push the script
    local basename
    basename=$(push_script "$script")

    # Clear logcat buffer to get clean output
    "$ADB" logcat -c 2>/dev/null || true

    # Send the broadcast
    "$ADB" shell "am broadcast -a com.dxxredux.AUTOMATE --es script $basename" >/dev/null 2>&1

    echo "  Running... (timeout: ${TIMEOUT_SEC}s)"

    # Monitor logcat for SCRIPT_RESULT (PASS or FAIL)
    local result=""
    local fail_detail=""
    local start_time=$SECONDS

    # Create a temp file for logcat output
    local logcat_tmp
    logcat_tmp=$(mktemp)

    # Start logcat in background, filtered for our tags
    "$ADB" logcat -s "DXX-Automate:*" >"$logcat_tmp" 2>/dev/null &
    local logcat_pid=$!

    while [ $((SECONDS - start_time)) -lt $TIMEOUT_SEC ]; do
        # Check for PASS
        if grep -q "SCRIPT_RESULT: PASS" "$logcat_tmp" 2>/dev/null; then
            result="PASS"
            break
        fi
        # Check for FAIL
        if grep -q "SCRIPT_RESULT: FAIL" "$logcat_tmp" 2>/dev/null; then
            result="FAIL"
            # Extract the failure detail
            fail_detail=$(grep "SCRIPT_RESULT: FAIL" "$logcat_tmp" | head -1 | sed 's/.*SCRIPT_RESULT: FAIL/FAIL/')
            # Also capture ASSERT_FAIL lines
            local assert_lines
            assert_lines=$(grep "ASSERT_FAIL\|ASSERT_EXPECTED" "$logcat_tmp" 2>/dev/null || true)
            if [ -n "$assert_lines" ]; then
                fail_detail="$fail_detail
$assert_lines"
            fi
            break
        fi
        # Check if process died (crash)
        if ! "$ADB" shell "pidof $PACKAGE" >/dev/null 2>&1; then
            result="CRASH"
            fail_detail="Game process died during test"
            break
        fi
        sleep 1
    done

    # Kill the background logcat
    kill "$logcat_pid" 2>/dev/null || true
    wait "$logcat_pid" 2>/dev/null || true

    if [ -z "$result" ]; then
        result="TIMEOUT"
        fail_detail="Test did not complete within ${TIMEOUT_SEC}s"
    fi

    # Print result
    case "$result" in
    PASS)
        echo "  RESULT: PASS"
        rm -f "$logcat_tmp"
        return 0
        ;;
    FAIL)
        echo "  RESULT: FAIL"
        # shellcheck disable=SC2001  # sed is clearer for multi-line prefix
        echo "$fail_detail" | sed 's/^/    /'
        rm -f "$logcat_tmp"
        return 1
        ;;
    CRASH)
        echo "  RESULT: CRASH"
        echo "    $fail_detail"
        rm -f "$logcat_tmp"
        return 1
        ;;
    TIMEOUT)
        echo "  RESULT: TIMEOUT"
        echo "    $fail_detail"
        # Dump last logcat lines for debugging
        echo "    Last automation log lines:"
        tail -5 "$logcat_tmp" 2>/dev/null | sed 's/^/      /' || true
        rm -f "$logcat_tmp"
        return 1
        ;;
    esac
}

# -- Main ----------------------------------------------------------------

echo "=== DXX-Redux Test Runner ==="
echo "Tests to run: ${#SCRIPTS[@]}"
echo ""

passed=0
failed=0
failed_names=()

for script in "${SCRIPTS[@]}"; do
    if run_single_test "$script"; then
        ((passed++))
    else
        ((failed++))
        failed_names+=("$(basename "$script" .json)")
    fi
    echo ""
done

echo "========================================"
if [ $failed -eq 0 ]; then
    echo "ALL TESTS PASS ($passed/$passed)"
    exit 0
else
    echo "TESTS FAILED: $failed of $((passed + failed))"
    for name in "${failed_names[@]}"; do
        echo "  FAIL: $name"
    done
    exit 1
fi
