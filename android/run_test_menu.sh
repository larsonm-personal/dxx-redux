#!/usr/bin/env bash
#
# run_test_menu.sh - Interactive menu for running regression tests
#
# This script presents a menu of all available test_*.json5 tests,
# allows you to select one, optionally preps the emulator, and runs it
# with full output visibility.
#
# Usage:
#   ./android/run_test_menu.sh

set -euo pipefail

# Resolve script directory portably (handles backslash paths from PowerShell,
# WSL /mnt/c/ paths, and MSYS2 /c/ paths)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
GAME_SCRIPTS_DIR="$SCRIPT_DIR/game_scripts"

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

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# -- Helper functions --------------------------------------------------------

print_header() {
    echo -e "${CYAN}=============================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}=============================================${NC}"
}

print_info() {
    echo -e "${GREEN}i  ${NC}$1"
}

print_error() {
    echo -e "${RED}X  ${NC}$1" >&2
}

print_success() {
    echo -e "${GREEN}OK ${NC}$1"
}

# -- Discover tests ----------------------------------------------------------

TESTS_DIR="$SCRIPT_DIR/tests"

print_header "DXX-Redux Regression Test Menu"
echo

# Collect json5 game scripts
test_names=()
test_types=()
test_relpath=()
for f in "$GAME_SCRIPTS_DIR"/test_*.json5; do
    [ -f "$f" ] || continue
    name=$(basename "$f" .json5)
    test_names+=("$name")
    test_types+=("json5")
    test_relpath+=("game_scripts/$(basename "$f")")
done

# Collect ps1 integration tests
for f in "$TESTS_DIR"/test_*.ps1; do
    [ -f "$f" ] || continue
    name=$(basename "$f" .ps1)
    test_names+=("$name")
    test_types+=("ps1")
    test_relpath+=("tests/$(basename "$f")")
done

# Sort all tests together by name
mapfile -t sorted_indices < <(
    for i in "${!test_names[@]}"; do
        echo "$i ${test_names[$i]}"
    done | sort -k2 | awk '{print $1}'
)

sorted_names=()
sorted_types=()
sorted_relpath=()
for i in "${sorted_indices[@]}"; do
    sorted_names+=("${test_names[$i]}")
    sorted_types+=("${test_types[$i]}")
    sorted_relpath+=("${test_relpath[$i]}")
done
test_names=("${sorted_names[@]}")
test_types=("${sorted_types[@]}")
test_relpath=("${sorted_relpath[@]}")

if [ ${#test_names[@]} -eq 0 ]; then
    print_error "No test files found in $GAME_SCRIPTS_DIR or $TESTS_DIR"
    exit 1
fi

# Display menu
echo "Available tests:"
echo
for i in "${!test_names[@]}"; do
    printf "  %2d) %-45s [%s]\n" "$((i + 1))" "${test_names[$i]}" "${test_types[$i]}"
done
echo

# Read user input
read -p "Select test (1-${#test_names[@]}): " -r choice

# Validate input
if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#test_names[@]} ]; then
    print_error "Invalid selection"
    exit 1
fi

idx=$((choice - 1))
test_name="${test_names[$idx]}"
test_type="${test_types[$idx]}"
selected_relpath="${test_relpath[$idx]}"

echo
print_info "Selected: $test_name  [$test_type]"
echo

# Ask about emulator prep
read -p "Prep emulator before running test? (y/n) [y]: " -r prep_emulator
prep_emulator=${prep_emulator:-y}

if [[ "$prep_emulator" == "y" || "$prep_emulator" == "Y" ]]; then
    print_info "Preparing emulator..."
    "$ADB" shell am force-stop com.dxxredux.app 2>/dev/null || true
    sleep 1
    "$ADB" logcat -c
    sleep 1
    print_success "Emulator prepped"
    echo
fi

# Run the test
print_header "Running: $test_name"
echo

cd "$SCRIPT_DIR"
if [ "$test_type" = "json5" ]; then
    # Use relative path to avoid MSYS/Git Bash path mangling with adb
    bash ./run_automation.sh "$selected_relpath"
    exit_code=$?
elif [ "$test_type" = "ps1" ]; then
    # ps1 tests run via PowerShell
    pwsh -NoProfile -NonInteractive -File "$selected_relpath"
    exit_code=$?
else
    print_error "Unknown test type: $test_type"
    exit 1
fi

echo
if [ $exit_code -eq 0 ]; then
    print_success "Test completed successfully"
    exit 0
else
    print_error "Test failed with exit code $exit_code"
    exit 1
fi
