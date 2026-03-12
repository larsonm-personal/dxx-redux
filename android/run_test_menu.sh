#!/usr/bin/env bash
#
# run_test_menu.sh — Interactive menu for running regression tests
#
# This script presents a menu of all available test_*.json5 tests,
# allows you to select one, optionally preps the emulator, and runs it
# with full output visibility.
#
# Usage:
#   ./android/run_test_menu.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
GAME_SCRIPTS_DIR="$SCRIPT_DIR/game_scripts"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ── Helper functions ────────────────────────────────────────────────────────

print_header() {
    echo -e "${CYAN}═════════════════════════════════════════════${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}═════════════════════════════════════════════${NC}"
}

print_info() {
    echo -e "${GREEN}ℹ ${NC}$1"
}

print_error() {
    echo -e "${RED}✗ ${NC}$1" >&2
}

print_success() {
    echo -e "${GREEN}✓ ${NC}$1"
}

# ── Discover tests ──────────────────────────────────────────────────────────

print_header "DXX-Redux Regression Test Menu"
echo

# Find all test_*.json5 files
mapfile -t tests < <(find "$GAME_SCRIPTS_DIR" -name "test_*.json5" -type f | sort)

if [ ${#tests[@]} -eq 0 ]; then
    print_error "No test_*.json5 files found in $GAME_SCRIPTS_DIR"
    exit 1
fi

# Display menu
echo "Available tests:"
echo
for i in "${!tests[@]}"; do
    test_file="${tests[$i]}"
    test_name=$(basename "$test_file" .json5)
    echo "  $((i + 1))) $test_name"
done
echo

# Read user input
read -p "Select test (1-${#tests[@]}): " -r choice

# Validate input
if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#tests[@]} ]; then
    print_error "Invalid selection"
    exit 1
fi

selected_test="${tests[$((choice - 1))]}"
test_name=$(basename "$selected_test" .json5)

echo
print_info "Selected: $test_name"
echo

# Ask about emulator prep
read -p "Prep emulator before running test? (y/n) [y]: " -r prep_emulator
prep_emulator=${prep_emulator:-y}

if [[ "$prep_emulator" == "y" || "$prep_emulator" == "Y" ]]; then
    print_info "Preparing emulator..."
    # Clear game app state and restart
    adb shell am force-stop com.dxxredux.app 2>/dev/null || true
    sleep 1
    adb logcat -c
    sleep 1
    print_success "Emulator prepped"
    echo
fi

# Run the test
print_header "Running: $test_name"
echo

cd "$SCRIPT_DIR"
bash ./run_automation.sh "$selected_test"
exit_code=$?

echo
if [ $exit_code -eq 0 ]; then
    print_success "Test completed successfully"
    exit 0
else
    print_error "Test failed with exit code $exit_code"
    exit 1
fi
