#!/bin/bash
# get_all.sh - One-shot Android dependency bootstrap
set -euo pipefail

wait_for_key() {
    if [ "${NO_PROMPT:-0}" -eq 1 ] || [ ! -t 0 ]; then
        return 0
    fi
    echo ""
    echo "Press any key to exit"
    read -r -n1 -s
}
trap wait_for_key EXIT

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# shellcheck disable=SC1091
source "$SCRIPT_DIR/platform.sh"
export GET_ALL_RUNNING=1 # tell sub-scripts to skip their own wait-for-key

SKIP_HOST_PREREQS=0
SKIP_POWERSHELL=0
SKIP_AVD=0
NO_PROMPT=0

usage() {
    cat <<'EOF'
Usage: ./get_all.sh [options]

Download and install the Android build dependencies under dependency_base.txt

Options:
  --skip-host-prereqs  Do not install Linux host packages
  --skip-powershell    Do not install PowerShell when pwsh is missing
  --skip-avd           Install emulator packages but do not create an AVD
  --no-prompt          Do not wait for a key before exiting
  -h, --help           Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
    --skip-host-prereqs)
        SKIP_HOST_PREREQS=1
        shift
        ;;
    --skip-powershell)
        SKIP_POWERSHELL=1
        shift
        ;;
    --skip-avd)
        SKIP_AVD=1
        shift
        ;;
    --no-prompt)
        NO_PROMPT=1
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        echo "Unknown argument: $1" >&2
        usage >&2
        exit 1
        ;;
    esac
done

TOTAL_STEPS=12
if [ "$(get_host_os)" = "linux" ] && [ "$SKIP_HOST_PREREQS" -eq 0 ]; then
    TOTAL_STEPS=$((TOTAL_STEPS + 1))
fi
if [ "$(get_host_os)" = "linux" ] && ! command -v pwsh >/dev/null 2>&1 && [ "$SKIP_POWERSHELL" -eq 0 ]; then
    TOTAL_STEPS=$((TOTAL_STEPS + 1))
fi
if [ "$SKIP_AVD" -eq 1 ]; then
    TOTAL_STEPS=$((TOTAL_STEPS - 1))
fi

STEP_NUM=0
run_step() {
    local label="$1"
    shift
    STEP_NUM=$((STEP_NUM + 1))
    echo ""
    echo "=== Step $STEP_NUM/$TOTAL_STEPS: $label ==="
    "$@"
}

if [ "$(get_host_os)" = "linux" ] && [ "$SKIP_HOST_PREREQS" -eq 0 ]; then
    run_step "Linux host prerequisites" bash "$SCRIPT_DIR/get_linux_build_prereqs.sh"
fi

if ! command -v pwsh >/dev/null 2>&1 && [ "$SKIP_POWERSHELL" -eq 0 ]; then
    if [ "$(get_host_os)" = "linux" ]; then
        run_step "PowerShell" bash "$SCRIPT_DIR/get_powershell.sh"
    else
        echo "PowerShell is not on PATH; install pwsh manually for PowerShell helper scripts"
    fi
elif command -v pwsh >/dev/null 2>&1; then
    echo "PowerShell already available: $(command -v pwsh)"
fi

run_step "JDK" bash "$SCRIPT_DIR/get_jdk.sh"

run_step "Android SDK" bash "$SCRIPT_DIR/get_sdk.sh"

run_step "Android NDK" bash "$SCRIPT_DIR/get_ndk.sh"

# Export JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT so remaining steps
# (finalize, emulator, avd) inherit them without re-detecting
# shellcheck disable=SC1091
source "$SCRIPT_DIR/../set_vars.sh"

run_step "Gradle wrapper" bash "$SCRIPT_DIR/get_gradle_wrapper.sh"

run_step "Finalize licenses and platform packages" bash "$SCRIPT_DIR/finalize.sh"

run_step "Emulator and system image" bash "$SCRIPT_DIR/get_emulator.sh"

if [ "$SKIP_AVD" -eq 0 ]; then
    run_step "Create AVD" bash "$SCRIPT_DIR/create_avd.sh"
fi

run_step "clang-format" bash "$SCRIPT_DIR/get_clang_format.sh"

run_step "shellcheck" bash "$SCRIPT_DIR/get_shellcheck.sh"

run_step "shfmt" bash "$SCRIPT_DIR/get_shfmt.sh"

run_step "ktlint" bash "$SCRIPT_DIR/get_ktlint.sh"

run_step "cmake-format / cmake-lint" bash "$SCRIPT_DIR/get_cmake_format.sh"

echo ""
echo "=== All dependencies installed. ==="
echo "From the repo root, source android/set_vars.sh to set JAVA_HOME / ANDROID_HOME / ANDROID_NDK_ROOT"
echo "Run pwsh android/Run-Emulator.ps1 to build, launch emulator, and test"
